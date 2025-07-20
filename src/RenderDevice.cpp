#include "RenderDevice.h"
#include "AnimationTask.h"
#include <iostream>

// =========================================================================
// 着色器源码定义 - 支持VTF多角色骨骼偏移
// =========================================================================

const char* vertexShaderSource = R"(#version 300 es
precision highp float;

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 4) in uvec4 aJoints;
layout(location = 5) in vec4 aWeights;

layout(location = 6) in vec4 aInstanceMatrix0;
layout(location = 7) in vec4 aInstanceMatrix1;
layout(location = 8) in vec4 aInstanceMatrix2;
layout(location = 9) in vec4 aInstanceMatrix3;

uniform mat4 uViewProjection;
uniform mat4 uModel;
uniform highp sampler2D uBoneTexture;
uniform bool uUseSkinning;
uniform bool uUseInstancing;
uniform int uBoneOffset;  // 骨骼偏移 - 支持多角色

out vec3 vNormal;
out vec2 vTexCoord;
out vec3 vWorldPos;
out vec4 vWeights;

mat4 getBoneMatrix(uint boneIndex) {
    mat4 bone;
    uint actualIndex = uint(uBoneOffset) + boneIndex;
    for (int i = 0; i < 4; i++) {
        bone[i] = texelFetch(uBoneTexture, ivec2(i, int(actualIndex)), 0);
    }
    return bone;
}

void main() {
    vec4 localPos = vec4(aPosition, 1.0);
    vec3 localNormal = aNormal;

    if (uUseSkinning) {
        mat4 skinMatrix =
            getBoneMatrix(aJoints.x) * aWeights.x +
            getBoneMatrix(aJoints.y) * aWeights.y +
            getBoneMatrix(aJoints.z) * aWeights.z +
            getBoneMatrix(aJoints.w) * aWeights.w;

        localPos = skinMatrix * localPos;
        localNormal = mat3(skinMatrix) * localNormal;
    }

    mat4 modelMatrix;
    if (uUseInstancing) {
        modelMatrix = mat4(
            aInstanceMatrix0,
            aInstanceMatrix1,
            aInstanceMatrix2,
            aInstanceMatrix3
        );
    } else {
        modelMatrix = uModel;
    }

    vec4 worldPos = modelMatrix * localPos;
    gl_Position = uViewProjection * worldPos;

    mat3 normalMatrix = mat3(transpose(inverse(modelMatrix)));
    vNormal = normalize(normalMatrix * localNormal);
    vTexCoord = aTexCoord;
    vWorldPos = worldPos.xyz;
    vWeights = aWeights;
}
)";

const char* fragmentShaderSource = R"(#version 300 es
precision highp float;

in vec3 vNormal;
in vec2 vTexCoord;
in vec3 vWorldPos;
in vec4 vWeights;

out vec4 fragColor;

uniform sampler2D uBaseColorTexture;
uniform vec4 uBaseColorFactor;
uniform vec3 uLightDir;
uniform vec3 uViewPos;
uniform bool uDebugWeights;

void main() {
    vec4 baseColor = texture(uBaseColorTexture, vTexCoord) * uBaseColorFactor;
    if (baseColor.a < 0.1) baseColor.a = 1.0;

    vec3 normal = normalize(vNormal);
    vec3 lightDir = normalize(-uLightDir);

    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * baseColor.rgb;

    vec3 viewDir = normalize(uViewPos - vWorldPos);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = spec * vec3(0.5);

    vec3 ambient = 0.3 * baseColor.rgb;
    vec3 result = ambient + diffuse + specular;

    fragColor = vec4(result, baseColor.a);
}
)";

// =========================================================================
// RenderDevice 实现
// =========================================================================

bool RenderDevice::initialize() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL初始化失败: " << SDL_GetError() << std::endl;
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    window = SDL_CreateWindow(
            "Spartan",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            windowWidth, windowHeight,
            SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
    );

    if (!window) {
        std::cerr << "窗口创建失败: " << SDL_GetError() << std::endl;
        return false;
    }

    glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        std::cerr << "OpenGL上下文创建失败: " << SDL_GetError() << std::endl;
        return false;
    }

    if (!gladLoadGLES2Loader((GLADloadproc)SDL_GL_GetProcAddress)) {
        std::cerr << "GLAD初始化失败" << std::endl;
        return false;
    }

    std::cout << "OpenGL版本: " << glGetString(GL_VERSION) << std::endl;

    if (!compileShaders()) {
        return false;
    }

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDisable(GL_CULL_FACE);
    glViewport(0, 0, windowWidth, windowHeight);

    createDefaultTexture();

    // 初始化GPU骨骼纹理管理器
    if (!BoneTextureManager::getInstance().initialize()) {
        std::cerr << "骨骼纹理管理器初始化失败" << std::endl;
        return false;
    }

    return true;
}

bool RenderDevice::compileShaders() {
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vertexShader);

    if (!checkShaderCompile(vertexShader, "顶点着色器")) {
        return false;
    }

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);

    if (!checkShaderCompile(fragmentShader, "片段着色器")) {
        return false;
    }

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    GLint success;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        std::cerr << "着色器程序链接失败: " << infoLog << std::endl;
        return false;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return true;
}

bool RenderDevice::checkShaderCompile(GLuint shader, const char* name) {
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << name << "编译失败: " << infoLog << std::endl;
        return false;
    }
    return true;
}

void RenderDevice::createDefaultTexture() {
    glGenTextures(1, &defaultTexture);
    glBindTexture(GL_TEXTURE_2D, defaultTexture);
    uint32_t white = 0xFFFFFFFF;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

GLuint RenderDevice::getBoneTexture() const {
    return BoneTextureManager::getInstance().getBoneTextureID();
}

void RenderDevice::createDummyTexture(spartan::asset::TextureData& texture) {
    glGenTextures(1, &texture.gpu_texture_id);
    glBindTexture(GL_TEXTURE_2D, texture.gpu_texture_id);
    uint32_t white = 0xFFFFFFFF;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

void RenderDevice::checkGLError(const char* operation) {
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "OpenGL错误在 " << operation << ": 0x" << std::hex << error << std::dec << std::endl;
    }
}

void RenderDevice::cleanup() {
    if (isCleanedUp) {
        return;
    }
    isCleanedUp = true;

    std::cout << "正在清理渲染设备资源..." << std::endl;

    if (!glContext) {
        std::cout << "OpenGL上下文已无效，跳过GPU资源清理" << std::endl;
        return;
    }

    // 清理骨骼纹理管理器
    BoneTextureManager::getInstance().cleanup();

    if (defaultTexture) {
        glDeleteTextures(1, &defaultTexture);
        defaultTexture = 0;
    }
    if (shaderProgram) {
        glDeleteProgram(shaderProgram);
        shaderProgram = 0;
    }

    if (glContext) {
        SDL_GL_DeleteContext(glContext);
        glContext = nullptr;
    }
    if (window) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }

    SDL_Quit();
    std::cout << "渲染设备资源清理完成" << std::endl;
}

void RenderDevice::shutdown() {
    cleanup();
}
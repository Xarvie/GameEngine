//这是一个正确的例子，模型很正常。

#include <glad/glad.h>
#include <SDL2/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_EXTERNAL_IMAGE
#include "tiny_gltf.h"

#include <iostream>
#include <vector>
#include <map>
#include <string>

// Window settings
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

// Camera settings
glm::vec3 cameraPos = glm::vec3(0.0f, 0.5f, 1.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, -0.3f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
float yaw = -90.0f;
float pitch = -20.0f;
float cameraSpeed = 0.005f;
float mouseSensitivity = 0.1f;
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;
// GLES 3.0 Vertex shader
const char* vertexShaderSource = R"(#version 300 es
precision highp float;

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat3 normalMatrix;

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = normalMatrix * aNormal;
    TexCoord = aTexCoord;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)";

// GLES 3.0 Fragment shader
const char* fragmentShaderSource = R"(#version 300 es
precision highp float;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

out vec4 FragColor;

uniform sampler2D baseColorTexture;
uniform sampler2D metallicRoughnessTexture;
uniform sampler2D normalTexture;
uniform sampler2D occlusionTexture;

uniform vec4 baseColorFactor;
uniform float metallicFactor;
uniform float roughnessFactor;
uniform int hasBaseColorTexture;
uniform int hasMetallicRoughnessTexture;
uniform int hasNormalTexture;
uniform int hasOcclusionTexture;

uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 viewPos;

vec3 getNormalFromMap()
{
    if (hasNormalTexture == 0) return normalize(Normal);

    vec3 tangentNormal = texture(normalTexture, TexCoord).xyz * 2.0 - 1.0;

    vec3 Q1 = dFdx(FragPos);
    vec3 Q2 = dFdy(FragPos);
    vec2 st1 = dFdx(TexCoord);
    vec2 st2 = dFdy(TexCoord);

    vec3 N = normalize(Normal);
    vec3 T = normalize(Q1 * st2.t - Q2 * st1.t);
    vec3 B = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}

void main()
{
    vec3 color = baseColorFactor.rgb;
    if (hasBaseColorTexture == 1) {
        color = pow(texture(baseColorTexture, TexCoord).rgb, vec3(2.2));
    }

    float metallic = metallicFactor;
    float roughness = roughnessFactor;
    if (hasMetallicRoughnessTexture == 1) {
        vec3 mrSample = texture(metallicRoughnessTexture, TexCoord).rgb;
        metallic *= mrSample.b;
        roughness *= mrSample.g;
    }

    float ao = 1.0;
    if (hasOcclusionTexture == 1) {
        ao = texture(occlusionTexture, TexCoord).r;
    }

    vec3 normal = getNormalFromMap();

    // Simple PBR-inspired lighting
    vec3 lightDir = normalize(lightPos - FragPos);
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);

    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * lightColor * color;

    float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0 * (1.0 - roughness));
    vec3 specular = spec * lightColor * mix(vec3(0.04), color, metallic);

    vec3 ambient = 0.2 * color * ao;
    vec3 result = ambient + diffuse + specular;

    // Tone mapping
    result = result / (result + vec3(1.0));
    // Gamma correction
    result = pow(result, vec3(1.0/2.2));

    FragColor = vec4(result, baseColorFactor.a);
}
)";


struct Primitive {
    GLuint VAO;
    GLuint indexCount;
    GLenum indexType;
    GLuint indexOffset;
    int materialIndex;
};

struct Mesh {
    std::vector<Primitive> primitives;
};

struct Node {
    glm::mat4 matrix;
    std::vector<int> children;
    int mesh;
};

std::map<int, GLuint> textureCache;
std::vector<Mesh> meshes;
std::vector<Node> nodes;
std::vector<GLuint> buffers;

GLuint compileShader(const char* source, GLenum type) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    int success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cerr << "Shader compilation failed: " << infoLog << std::endl;
    }

    return shader;
}

GLuint createShaderProgram() {
    GLuint vertexShader = compileShader(vertexShaderSource, GL_VERTEX_SHADER);
    GLuint fragmentShader = compileShader(fragmentShaderSource, GL_FRAGMENT_SHADER);

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    int success;
    char infoLog[512];
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cerr << "Shader linking failed: " << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

GLuint loadTexture(const tinygltf::Image& image) {
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    GLenum format = GL_RGB;
    if (image.component == 4) {
        format = GL_RGBA;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, format, image.width, image.height, 0, format, GL_UNSIGNED_BYTE, &image.image[0]);

    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    return texture;
}

void loadGLTF(const std::string& filename, tinygltf::Model& model) {
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, filename);

    if (!warn.empty()) {
        std::cout << "GLTF Warning: " << warn << std::endl;
    }

    if (!err.empty()) {
        std::cerr << "GLTF Error: " << err << std::endl;
    }

    if (!ret) {
        std::cerr << "Failed to load GLTF file" << std::endl;
        exit(1);
    }
}

void setupBuffers(const tinygltf::Model& model) {
    // Load buffers
    for (size_t i = 0; i < model.buffers.size(); ++i) {
        const tinygltf::Buffer& buffer = model.buffers[i];
        GLuint vbo;
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, buffer.data.size(), &buffer.data[0], GL_STATIC_DRAW);
        buffers.push_back(vbo);
    }
}

void setupMeshes(const tinygltf::Model& model) {
    meshes.resize(model.meshes.size());

    for (size_t meshIdx = 0; meshIdx < model.meshes.size(); ++meshIdx) {
        const tinygltf::Mesh& mesh = model.meshes[meshIdx];

        for (size_t primIdx = 0; primIdx < mesh.primitives.size(); ++primIdx) {
            const tinygltf::Primitive& primitive = mesh.primitives[primIdx];

            Primitive prim;
            glGenVertexArrays(1, &prim.VAO);
            glBindVertexArray(prim.VAO);

            // Position attribute
            if (primitive.attributes.find("POSITION") != primitive.attributes.end()) {
                int accessorIdx = primitive.attributes.at("POSITION");
                const tinygltf::Accessor& accessor = model.accessors[accessorIdx];
                const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];

                glBindBuffer(GL_ARRAY_BUFFER, buffers[bufferView.buffer]);
                glVertexAttribPointer(0, 3, accessor.componentType, GL_FALSE, 0,
                                      (void*)(bufferView.byteOffset + accessor.byteOffset));
                glEnableVertexAttribArray(0);
            }

            // Normal attribute
            if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
                int accessorIdx = primitive.attributes.at("NORMAL");
                const tinygltf::Accessor& accessor = model.accessors[accessorIdx];
                const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];

                glBindBuffer(GL_ARRAY_BUFFER, buffers[bufferView.buffer]);
                glVertexAttribPointer(1, 3, accessor.componentType, GL_FALSE, 0,
                                      (void*)(bufferView.byteOffset + accessor.byteOffset));
                glEnableVertexAttribArray(1);
            }

            // TexCoord attribute
            if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
                int accessorIdx = primitive.attributes.at("TEXCOORD_0");
                const tinygltf::Accessor& accessor = model.accessors[accessorIdx];
                const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];

                glBindBuffer(GL_ARRAY_BUFFER, buffers[bufferView.buffer]);
                glVertexAttribPointer(2, 2, accessor.componentType, GL_FALSE, 0,
                                      (void*)(bufferView.byteOffset + accessor.byteOffset));
                glEnableVertexAttribArray(2);
            }

            // Index buffer
            if (primitive.indices >= 0) {
                const tinygltf::Accessor& accessor = model.accessors[primitive.indices];
                const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];

                GLuint ebo;
                glGenBuffers(1, &ebo);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, bufferView.byteLength,
                             &model.buffers[bufferView.buffer].data[bufferView.byteOffset], GL_STATIC_DRAW);

                prim.indexCount = accessor.count;
                prim.indexType = accessor.componentType;
                prim.indexOffset = accessor.byteOffset;
            }

            prim.materialIndex = primitive.material;
            meshes[meshIdx].primitives.push_back(prim);

            glBindVertexArray(0);
        }
    }
}

glm::mat4 getNodeTransform(const tinygltf::Node& node) {
    glm::mat4 matrix(1.0f);

    if (node.matrix.size() == 16) {
        matrix = glm::make_mat4(node.matrix.data());
    } else {
        if (node.translation.size() == 3) {
            matrix = glm::translate(matrix, glm::vec3(node.translation[0], node.translation[1], node.translation[2]));
        }
        if (node.rotation.size() == 4) {
            glm::quat q(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
            matrix *= glm::mat4_cast(q);
        }
        if (node.scale.size() == 3) {
            matrix = glm::scale(matrix, glm::vec3(node.scale[0], node.scale[1], node.scale[2]));
        }
    }

    return matrix;
}

void setupNodes(const tinygltf::Model& model) {
    nodes.resize(model.nodes.size());

    for (size_t i = 0; i < model.nodes.size(); ++i) {
        const tinygltf::Node& node = model.nodes[i];
        nodes[i].matrix = getNodeTransform(node);
        nodes[i].children = node.children;
        nodes[i].mesh = node.mesh;
    }
}

void renderNode(const tinygltf::Model& model, int nodeIdx, const glm::mat4& parentMatrix, GLuint shaderProgram) {
    const Node& node = nodes[nodeIdx];
    glm::mat4 nodeMatrix = parentMatrix * node.matrix;

    if (node.mesh >= 0) {
        const Mesh& mesh = meshes[node.mesh];

        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(nodeMatrix));
        glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(nodeMatrix)));
        glUniformMatrix3fv(glGetUniformLocation(shaderProgram, "normalMatrix"), 1, GL_FALSE, glm::value_ptr(normalMatrix));

        for (const Primitive& prim : mesh.primitives) {
            // Set material properties
            if (prim.materialIndex >= 0) {
                const tinygltf::Material& material = model.materials[prim.materialIndex];

                // Base color
                glm::vec4 baseColor(1.0f);
                if (material.values.find("baseColorFactor") != material.values.end()) {
                    const auto& factor = material.values.at("baseColorFactor").ColorFactor();
                    baseColor = glm::vec4(factor[0], factor[1], factor[2], factor[3]);
                }
                glUniform4fv(glGetUniformLocation(shaderProgram, "baseColorFactor"), 1, glm::value_ptr(baseColor));

                // Metallic and roughness
                float metallic = 1.0f;
                float roughness = 1.0f;
                if (material.values.find("metallicFactor") != material.values.end()) {
                    metallic = material.values.at("metallicFactor").Factor();
                }
                if (material.values.find("roughnessFactor") != material.values.end()) {
                    roughness = material.values.at("roughnessFactor").Factor();
                }
                glUniform1f(glGetUniformLocation(shaderProgram, "metallicFactor"), metallic);
                glUniform1f(glGetUniformLocation(shaderProgram, "roughnessFactor"), roughness);

                // Textures
                int textureUnit = 0;

                // Base color texture
                glUniform1i(glGetUniformLocation(shaderProgram, "hasBaseColorTexture"), 0);
                if (material.values.find("baseColorTexture") != material.values.end()) {
                    int texIndex = material.values.at("baseColorTexture").TextureIndex();
                    if (texIndex >= 0) {
                        int imgIndex = model.textures[texIndex].source;
                        if (textureCache.find(imgIndex) == textureCache.end()) {
                            textureCache[imgIndex] = loadTexture(model.images[imgIndex]);
                        }
                        glActiveTexture(GL_TEXTURE0 + textureUnit);
                        glBindTexture(GL_TEXTURE_2D, textureCache[imgIndex]);
                        glUniform1i(glGetUniformLocation(shaderProgram, "baseColorTexture"), textureUnit++);
                        glUniform1i(glGetUniformLocation(shaderProgram, "hasBaseColorTexture"), 1);
                    }
                }

                // Metallic roughness texture
                glUniform1i(glGetUniformLocation(shaderProgram, "hasMetallicRoughnessTexture"), 0);
                if (material.values.find("metallicRoughnessTexture") != material.values.end()) {
                    int texIndex = material.values.at("metallicRoughnessTexture").TextureIndex();
                    if (texIndex >= 0) {
                        int imgIndex = model.textures[texIndex].source;
                        if (textureCache.find(imgIndex) == textureCache.end()) {
                            textureCache[imgIndex] = loadTexture(model.images[imgIndex]);
                        }
                        glActiveTexture(GL_TEXTURE0 + textureUnit);
                        glBindTexture(GL_TEXTURE_2D, textureCache[imgIndex]);
                        glUniform1i(glGetUniformLocation(shaderProgram, "metallicRoughnessTexture"), textureUnit++);
                        glUniform1i(glGetUniformLocation(shaderProgram, "hasMetallicRoughnessTexture"), 1);
                    }
                }

                // Normal texture
                glUniform1i(glGetUniformLocation(shaderProgram, "hasNormalTexture"), 0);
                if (material.normalTexture.index >= 0) {
                    int imgIndex = model.textures[material.normalTexture.index].source;
                    if (textureCache.find(imgIndex) == textureCache.end()) {
                        textureCache[imgIndex] = loadTexture(model.images[imgIndex]);
                    }
                    glActiveTexture(GL_TEXTURE0 + textureUnit);
                    glBindTexture(GL_TEXTURE_2D, textureCache[imgIndex]);
                    glUniform1i(glGetUniformLocation(shaderProgram, "normalTexture"), textureUnit++);
                    glUniform1i(glGetUniformLocation(shaderProgram, "hasNormalTexture"), 1);
                }

                // Occlusion texture
                glUniform1i(glGetUniformLocation(shaderProgram, "hasOcclusionTexture"), 0);
                if (material.occlusionTexture.index >= 0) {
                    int imgIndex = model.textures[material.occlusionTexture.index].source;
                    if (textureCache.find(imgIndex) == textureCache.end()) {
                        textureCache[imgIndex] = loadTexture(model.images[imgIndex]);
                    }
                    glActiveTexture(GL_TEXTURE0 + textureUnit);
                    glBindTexture(GL_TEXTURE_2D, textureCache[imgIndex]);
                    glUniform1i(glGetUniformLocation(shaderProgram, "occlusionTexture"), textureUnit++);
                    glUniform1i(glGetUniformLocation(shaderProgram, "hasOcclusionTexture"), 1);
                }
            }

            glBindVertexArray(prim.VAO);
            glDrawElements(GL_TRIANGLES, prim.indexCount, prim.indexType, (void*)(size_t)prim.indexOffset);
            glBindVertexArray(0);
        }
    }

    for (int childIdx : node.children) {
        renderNode(model, childIdx, nodeMatrix, shaderProgram);
    }
}

void processInput(SDL_Window* window, bool& running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            running = false;
        } else if (event.type == SDL_KEYDOWN) {
            switch (event.key.keysym.sym) {
                case SDLK_ESCAPE:
                    running = false;
                    break;
            }
        } else if (event.type == SDL_MOUSEMOTION) {
            float xpos = event.motion.x;
            float ypos = event.motion.y;

            if (firstMouse) {
                lastX = xpos;
                lastY = ypos;
                firstMouse = false;
            }

            float xoffset = xpos - lastX;
            float yoffset = lastY - ypos;
            lastX = xpos;
            lastY = ypos;

            xoffset *= mouseSensitivity;
            yoffset *= mouseSensitivity;

            yaw += xoffset;
            pitch += yoffset;

            if (pitch > 89.0f) pitch = 89.0f;
            if (pitch < -89.0f) pitch = -89.0f;

            glm::vec3 front;
            front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
            front.y = sin(glm::radians(pitch));
            front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
            cameraFront = glm::normalize(front);
        }
    }

    const Uint8* keystate = SDL_GetKeyboardState(NULL);
    if (keystate[SDL_SCANCODE_W]) {
        cameraPos += cameraSpeed * cameraFront;
    }
    if (keystate[SDL_SCANCODE_S]) {
        cameraPos -= cameraSpeed * cameraFront;
    }
    if (keystate[SDL_SCANCODE_A]) {
        cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    }
    if (keystate[SDL_SCANCODE_D]) {
        cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    }
}

int main(int argc, char* argv[]) {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
        return -1;
    }

    // Set OpenGL attributes
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    // Create window
    SDL_Window* window = SDL_CreateWindow("GLTF Chess Renderer",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          SCR_WIDTH, SCR_HEIGHT,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);

    if (!window) {
        std::cerr << "Failed to create window: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return -1;
    }

    // Create OpenGL context
    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        std::cerr << "Failed to create OpenGL context: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    // Initialize GLAD
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    // Set viewport
    glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);

    // Enable depth testing
    glEnable(GL_DEPTH_TEST);

    // Enable face culling
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // Create shader program
    GLuint shaderProgram = createShaderProgram();

    // Load GLTF model
    tinygltf::Model model;
    loadGLTF("ABeautifulGame.gltf", model);

    // Setup buffers and meshes
    setupBuffers(model);
    setupMeshes(model);
    setupNodes(model);

    // Set mouse mode
    SDL_SetRelativeMouseMode(SDL_TRUE);

    // Main loop
    bool running = true;
    while (running) {
        processInput(window, running);

        // Clear screen
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Use shader program
        glUseProgram(shaderProgram);

        // Set view and projection matrices
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);

        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

        // Set light properties
        glm::vec3 lightPos(0.5f, 1.0f, 0.5f);
        glm::vec3 lightColor(1.0f, 1.0f, 1.0f);
        glUniform3fv(glGetUniformLocation(shaderProgram, "lightPos"), 1, glm::value_ptr(lightPos));
        glUniform3fv(glGetUniformLocation(shaderProgram, "lightColor"), 1, glm::value_ptr(lightColor));
        glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, glm::value_ptr(cameraPos));

        // Render scene
        const tinygltf::Scene& scene = model.scenes[model.defaultScene];
        for (int nodeIdx : scene.nodes) {
            renderNode(model, nodeIdx, glm::mat4(1.0f), shaderProgram);
        }

        // Swap buffers
        SDL_GL_SwapWindow(window);
    }

    // Cleanup
    glDeleteProgram(shaderProgram);

    for (auto& pair : textureCache) {
        glDeleteTextures(1, &pair.second);
    }

    for (const Mesh& mesh : meshes) {
        for (const Primitive& prim : mesh.primitives) {
            glDeleteVertexArrays(1, &prim.VAO);
        }
    }

    for (GLuint buffer : buffers) {
        glDeleteBuffers(1, &buffer);
    }

    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
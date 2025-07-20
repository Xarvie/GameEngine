#pragma once

#include <SDL2/SDL.h>
#include "glad/glad.h"
#include "GltfTools/AssetSerializer.h"

// =========================================================================
// 渲染设备 - 负责底层OpenGL设备管理
// =========================================================================

class RenderDevice {
public:
    static RenderDevice& getInstance() {
        static RenderDevice instance;
        return instance;
    }

    bool initialize();
    void cleanup();
    void shutdown();

    // 着色器相关
    bool compileShaders();
    bool checkShaderCompile(GLuint shader, const char* name);

    // 纹理相关
    void createDefaultTexture();
    void createDummyTexture(spartan::asset::TextureData& texture);

    // 工具函数
    void checkGLError(const char* operation);

    // 获取器
    SDL_Window* getWindow() const { return window; }
    SDL_GLContext getGLContext() const { return glContext; }
    int getWindowWidth() const { return windowWidth; }
    int getWindowHeight() const { return windowHeight; }
    GLuint getShaderProgram() const { return shaderProgram; }
    GLuint getDefaultTexture() const { return defaultTexture; }

    // 骨骼纹理现在通过BoneTextureManager管理
    GLuint getBoneTexture() const;

private:
    RenderDevice() = default;
    ~RenderDevice() {}

    SDL_Window* window = nullptr;
    SDL_GLContext glContext = nullptr;
    int windowWidth = 1280;
    int windowHeight = 720;

    GLuint shaderProgram = 0;
    GLuint defaultTexture = 0;

    bool isCleanedUp = false;
};
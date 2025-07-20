#pragma once
#include "RenderDevice.h"
#include "GltfTools/AssetSerializer.h"
#include "EntityComponents.h"
#include "glad/glad.h"

// =========================================================================
// 渲染器 - 负责资源管理和绘制调用执行
// =========================================================================

class Renderer {
public:
    static Renderer& getInstance() {
        static Renderer instance;
        return instance;
    }

    void shutdown() { cleanup(); }

    bool initialize();
    void cleanup();

    // 资产管理
    bool loadAsset(const char* gltfPath);
    void uploadMesh(MeshData& mesh);

    // 材质和纹理
    void setupMaterial(const MaterialHandle& materialHandle);
    void setupInstancedVAO(MeshData& mesh, uint32_t instanceBuffer);

    // 绘制执行
    void executeDrawMesh(const RenderCommand::DrawMeshData& data);
    void executeDrawInstancedMesh(const RenderCommand::DrawInstancedMeshData& data);
    void executeSetBones(const RenderCommand::SetBonesData& data);
    void executeSetUniform(const RenderCommand::SetUniformData& data);
    void executeBatchedDraw(const RenderCommand::DrawMeshData& data, const std::vector<glm::mat4>& instanceMatrices);

    // 获取器
    ProcessedAsset& getAsset() { return asset; }
    const ProcessedAsset& getAsset() const { return asset; }
    int getWindowWidth() const { return device.getWindowWidth(); }
    int getWindowHeight() const { return device.getWindowHeight(); }

private:
    Renderer() = default;
    ~Renderer() = default;  // 私有析构函数，防止意外销毁

    RenderDevice& device = RenderDevice::getInstance();
    ProcessedAsset asset;
    bool isCleanedUp = false;

    // 动态合批相关
    struct BatchingState {
        GLuint tempInstanceBuffer = 0;
        std::vector<glm::mat4> instanceMatrices;
        size_t maxInstances = 1000;  // 最大合批实例数
        void ensureBufferCreated();
        void cleanup();
    } batchingState;
};
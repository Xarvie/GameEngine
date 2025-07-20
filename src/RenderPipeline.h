#pragma once

#include "Core.h"
#include "Renderer.h"

// =========================================================================
// 渲染管线 - 负责渲染指令队列管理和处理
// =========================================================================

class RenderPipeline {
public:
    static RenderPipeline& getInstance() {
        static RenderPipeline instance;
        return instance;
    }

    // 渲染队列管理
    void clearRenderQueue();
    void addRenderCommand(const RenderCommand& command);
    void processRenderQueue();

    // 渲染指令提交
    void submitGlobalUniforms(entt::registry& registry, const glm::mat4& viewMatrix);
    void submitMeshes(entt::registry& registry, const glm::mat4& viewMatrix);
    void submitInstancedMeshes(entt::registry& registry, const glm::mat4& viewMatrix);
    void submitRenderCommands(entt::registry& registry);

    // 辅助函数
    static entt::entity findModelRoot(entt::registry& registry, entt::entity start_entity);

private:
    RenderPipeline() = default;
    ~RenderPipeline() = default;

    std::deque<RenderCommand> renderQueue;
    Renderer& renderer = Renderer::getInstance();
    RenderDevice& device = RenderDevice::getInstance();

    // 批处理统计
    struct BatchingStats {
        int frameCount = 0;
        int totalDrawCalls = 0;
        int batchedDrawCalls = 0;
    } batchingStats;

    void processBatchedRendering();
};
// SimpleApp.cpp - 使用插件式架构的渲染引擎示例
#include "Core.h"
#include "RenderDevice.h"
#include "Renderer.h"
#include "RenderPipeline.h"
#include "RenderWorld.h"
#include "EntityComponents.h"

class SimpleApplication {
private:
    entt::registry registry;
    bool running = true;

    // 核心组件引用
    Renderer& renderer = Renderer::getInstance();
    RenderPipeline& pipeline = RenderPipeline::getInstance();

    // 新的插件式系统管理器
    std::unique_ptr<RenderWorld> renderWorld;

public:
    bool initialize() {
        std::cout << "=== 初始化插件式渲染应用程序 ===" << std::endl;

        // 1. 初始化渲染器
        if (!renderer.initialize()) {
            std::cerr << "渲染器初始化失败" << std::endl;
            return false;
        }

        // 2. 加载资产
        std::vector<std::string> modelPath = {"art/Fox/glTF/Fox.gltf", //纯蒙皮测试
                             "art/AnimatedCube/glTF/AnimatedCube.gltf", //纯节点测试
                             "art/robot/glTF/robot.gltf" //蒙皮+节点测试
                             };
        if (!renderer.loadAsset(modelPath[0].c_str())) {
            std::cerr << "模型加载失败" << std::endl;
            return false;
        }

        // 3. 创建并初始化RenderWorld
        renderWorld = std::make_unique<RenderWorld>();
        renderWorld->createDefaultSystems(); // 自动注册所有核心系统

        if (!renderWorld->initialize(registry)) {
            std::cerr << "RenderWorld初始化失败" << std::endl;
            return false;
        }

        // 4. 创建基础实体
        setupScene();

        // 5. 打印系统信息
        renderWorld->printSystemsInfo();

        std::cout << "插件式应用程序初始化完成" << std::endl;
        return true;
    }

    void setupScene() {
        std::cout << "=== 设置场景 ===" << std::endl;

        // 创建相机
        auto camera = EntityFactory::createCamera(registry);
        std::cout << "创建相机实体: " << static_cast<uint32_t>(camera) << std::endl;

        // 创建输入处理器
        auto inputHandler = EntityFactory::createInputHandler(registry);
        std::cout << "创建输入处理器: " << static_cast<uint32_t>(inputHandler) << std::endl;

        // 创建动画模型
        auto modelEntities = EntityFactory::createAnimatedModelFromAsset(
                registry, renderer.getAsset(), 0);
        std::cout << "创建了 " << modelEntities.size() << " 个模型实体" << std::endl;

        // 查找并配置模型根节点
        for (auto entity : modelEntities) {
            if (registry.all_of<ModelRootTag>(entity)) {
                auto& transform = registry.get<Transform>(entity);
                transform.position = glm::vec3(0.0f, 0.0f, 0.0f);
                transform.scale = glm::vec3(1.0f);

                // 通过系统接口标记变换为脏状态
                if (auto* transformSystem = renderWorld->getSystem<ITransformSystem>()) {
                    transformSystem->markDirty(registry, entity);
                }

                registry.emplace<ControllableCharacterTag>(entity);
                std::cout << "配置模型根实体: " << static_cast<uint32_t>(entity) << std::endl;
                break;
            }
        }

        // 创建模型后，让变换系统重建缓存
        if (auto* transformSystem = renderWorld->getSystem<ITransformSystem>()) {
            transformSystem->invalidateCache();
        }

        // 设置默认光照
        if (auto* lightManager = renderWorld->getSystem<ILightManager>()) {
            lightManager->addDirectionalLight(
                    glm::vec3(-0.5f, -1.0f, -0.5f),
                    glm::vec3(1.0f, 1.0f, 1.0f),
                    1.0f
            );
        }
    }

    void run() {
        std::cout << "=== 开始主循环（插件式架构）===" << std::endl;

        auto lastTime = std::chrono::high_resolution_clock::now();

        while (running) {
            // 计算帧时间
            auto currentTime = std::chrono::high_resolution_clock::now();
            float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
            lastTime = currentTime;

            // 处理事件（通过RenderWorld转发给InputSystem）
            processEvents();

            // 更新所有系统（通过RenderWorld统一管理）
            updateSystems(deltaTime);

            // 渲染
            render();

            // 控制帧率
            SDL_Delay(16); // ~60 FPS
        }

        std::cout << "主循环结束" << std::endl;
    }

private:
    void processEvents() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            // 通过RenderWorld处理事件，自动转发给InputSystem
            renderWorld->processEvent(event, registry, running);
        }
    }

    void updateSystems(float deltaTime) {
        // 分阶段执行，优化并行效率

        // 阶段1：分发异步动画任务
        if (auto* animSystem = renderWorld->getSystem<IAnimationSystem>()) {
            animSystem->update(registry, deltaTime);  // 包含任务分发和结果应用
        }

        // 阶段2：更新其他系统（变换、输入、相机等）
        renderWorld->update(registry, deltaTime);
    }

    void render() {
        // 提交渲染指令
        pipeline.submitRenderCommands(registry);

        // 处理渲染队列
        pipeline.processRenderQueue();

        // 交换缓冲区
        SDL_GL_SwapWindow(RenderDevice::getInstance().getWindow());
    }

public:
    void shutdown() {
        std::cout << "=== 安全清理VTF渲染应用程序 ===" << std::endl;

        // 1. 清理实例化缓冲区
        auto instancedView = registry.view<InstancedMeshComponent>();
        for (auto entity : instancedView) {
            auto& instancedMesh = instancedView.get<InstancedMeshComponent>(entity);
            if (instancedMesh.instanceBuffer != 0) {
                glDeleteBuffers(1, &instancedMesh.instanceBuffer);
                instancedMesh.instanceBuffer = 0;
            }
        }

        // 2. 清理RenderWorld（包括TaskSystem）
        if (renderWorld) {
            renderWorld->cleanup(registry);
            renderWorld.reset();
        }

        // 3. 【关键】手动清理渲染器资源，避免静态析构问题
        renderer.cleanup();

        // 4. 清理ECS注册表
        registry.clear();

        std::cout << "VTF渲染应用程序安全清理完成" << std::endl;
    }

    void printControls() {
        std::cout << "\n=== 控制说明（插件式架构版本）===" << std::endl;
        std::cout << "🖱️  鼠标:" << std::endl;
        std::cout << "   - 左键拖拽: 旋转相机" << std::endl;
        std::cout << "   - 滚轮: 缩放距离" << std::endl;
        std::cout << "⌨️  键盘:" << std::endl;
        std::cout << "   - WASD: 移动相机" << std::endl;
        std::cout << "   - Q/E: 缩放模型" << std::endl;
        std::cout << "   - 空格: 暂停/继续动画" << std::endl;
        std::cout << "   - R: 重置动画" << std::endl;
        std::cout << "   - F: 线框模式" << std::endl;
        std::cout << "   - C: 自动旋转" << std::endl;
        std::cout << "   - ESC: 退出程序" << std::endl;
        std::cout << "🔧 系统调试:" << std::endl;
        std::cout << "   - F1: 打印系统状态" << std::endl;
        std::cout << "   - F2: 打印动画统计" << std::endl;
        std::cout << "   - F3: 打印变换统计" << std::endl;
        std::cout << "========================\n" << std::endl;
    }

    // 新增：系统调试功能
    void handleDebugKeys() {
        auto inputView = registry.view<InputStateComponent>();
        for (auto entity : inputView) {
            auto& input = inputView.get<InputStateComponent>(entity);

            for (SDL_Keycode key : input.keyboard.pressedThisFrame) {
                switch (key) {
                    case SDLK_F1:
                        renderWorld->printSystemsInfo();
                        break;

                    case SDLK_F2:
                        if (auto* animSystem = renderWorld->getSystem<IAnimationSystem>()) {
                            animSystem->printAnimationStats();
                        }
                        break;

                    case SDLK_F3:
                        if (auto* transformSystem = renderWorld->getSystem<ITransformSystem>()) {
                            transformSystem->printStats();
                        }
                        break;

                    case SDLK_F4:
                        // 强制刷新动画系统
                        if (auto* animSystem = renderWorld->getSystem<IAnimationSystem>()) {
                            animSystem->forceRefresh(registry);
                        }
                        break;

                    case SDLK_F5:
                        // 强制重建变换系统缓存
                        if (auto* transformSystem = renderWorld->getSystem<ITransformSystem>()) {
                            transformSystem->invalidateCache();
                        }
                        break;
                }
            }
        }
    }

    // 在updateSystems中调用调试功能
    void updateSystemsWithDebug(float deltaTime) {
        handleDebugKeys();
        updateSystems(deltaTime);
    }
};

// 主函数
int main(int argc, char* argv[]) {
    SimpleApplication app;

    if (!app.initialize()) {
        return -1;
    }

    app.printControls();
    app.run();
    app.shutdown();

    return 0;
}

// =========================================================================
// 高级插件式架构使用示例
// =========================================================================

class AdvancedPluginExample {
public:
    static void demonstrateCustomSystem() {
        std::cout << "\n=== 演示自定义系统插件 ===" << std::endl;

        entt::registry registry;
        RenderWorld renderWorld;

        // 创建默认系统
        renderWorld.createDefaultSystems();

        // 可以添加自定义系统
        // renderWorld.registerSystem(std::make_unique<CustomParticleSystem>());
        // renderWorld.registerSystem(std::make_unique<CustomAudioSystem>());

        if (renderWorld.initialize(registry)) {
            std::cout << "自定义系统配置初始化成功" << std::endl;
            renderWorld.printSystemsInfo();
        }

        renderWorld.cleanup(registry);
    }

    static void demonstrateSystemInteraction() {
        std::cout << "\n=== 演示系统间交互 ===" << std::endl;

        entt::registry registry;
        RenderWorld renderWorld;
        renderWorld.createDefaultSystems();
        renderWorld.initialize(registry);

        // 通过接口与系统交互
        if (auto* animSystem = renderWorld.getSystem<IAnimationSystem>()) {
            animSystem->setGlobalPlayState(false);
            std::cout << "通过接口暂停了全局动画" << std::endl;
        }

        if (auto* lightManager = renderWorld.getSystem<ILightManager>()) {
            lightManager->addDirectionalLight(
                    glm::vec3(1.0f, -0.5f, 0.0f),
                    glm::vec3(1.0f, 0.8f, 0.6f),
                    0.8f
            );
            std::cout << "通过接口添加了暖色调光源" << std::endl;
        }

        renderWorld.cleanup(registry);
    }
};

// 性能监控增强版
class PerformanceMonitor {
private:
    std::chrono::high_resolution_clock::time_point lastPrintTime;
    int frameCount = 0;
    float totalFrameTime = 0.0f;
    RenderWorld* renderWorld;

public:
    PerformanceMonitor(RenderWorld* world) : renderWorld(world) {
        lastPrintTime = std::chrono::high_resolution_clock::now();
    }

    void update(float deltaTime, entt::registry& registry) {
        frameCount++;
        totalFrameTime += deltaTime;

        auto currentTime = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration<float>(currentTime - lastPrintTime).count();

        // 每5秒打印一次性能统计
        if (elapsed >= 5.0f) {
            float avgFPS = frameCount / elapsed;
            float avgFrameTime = (totalFrameTime / frameCount) * 1000.0f; // 毫秒

            std::cout << "\n=== 插件式架构性能统计 ===" << std::endl;
            std::cout << "平均FPS: " << std::fixed << std::setprecision(1) << avgFPS << std::endl;
            std::cout << "平均帧时间: " << std::fixed << std::setprecision(2) << avgFrameTime << "ms" << std::endl;

            // 打印系统统计
            if (renderWorld) {
                if (auto* transformSystem = renderWorld->getSystem<ITransformSystem>()) {
                    transformSystem->printStats();
                }
                if (auto* animSystem = renderWorld->getSystem<IAnimationSystem>()) {
                    animSystem->printAnimationStats();
                }
            }

            // 重置计数器
            frameCount = 0;
            totalFrameTime = 0.0f;
            lastPrintTime = currentTime;
        }
    }
};
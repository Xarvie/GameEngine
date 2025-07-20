#include "RenderWorld.h"
#include "Systems.h"
#include <algorithm>
#include <iostream>

// =========================================================================
// RenderWorld 实现
// =========================================================================

RenderWorld::RenderWorld() = default;

RenderWorld::~RenderWorld() = default;

bool RenderWorld::initialize(entt::registry& registry) {
    if (initialized) {
        std::cout << "警告：RenderWorld 已经初始化过了" << std::endl;
        return true;
    }

    std::cout << "初始化 RenderWorld，共 " << systems.size() << " 个系统..." << std::endl;

    // 按优先级排序系统
    sortSystemsByPriority();

    // 初始化所有系统
    for (auto& system : systems) {
        std::cout << "初始化系统: " << system->getName()
                  << " (优先级: " << system->getPriority() << ")" << std::endl;
        try {
            system->initialize(registry);
        } catch (const std::exception& e) {
            std::cerr << "系统 " << system->getName() << " 初始化失败: " << e.what() << std::endl;
            return false;
        }
    }

    initialized = true;
    std::cout << "RenderWorld 初始化完成" << std::endl;
    return true;
}

void RenderWorld::update(entt::registry& registry, float deltaTime) {
    if (!initialized) {
        std::cerr << "错误：RenderWorld 未初始化" << std::endl;
        return;
    }

    // 按优先级顺序更新所有系统
    for (auto& system : systems) {
        try {
            system->update(registry, deltaTime);
        } catch (const std::exception& e) {
            std::cerr << "系统 " << system->getName() << " 更新失败: " << e.what() << std::endl;
        }
    }
}

void RenderWorld::cleanup(entt::registry& registry) {
    if (!initialized) {
        return;
    }

    std::cout << "清理 RenderWorld..." << std::endl;

    // 反向清理系统（与初始化顺序相反）
    for (auto it = systems.rbegin(); it != systems.rend(); ++it) {
        std::cout << "清理系统: " << (*it)->getName() << std::endl;
        try {
            (*it)->cleanup(registry);
        } catch (const std::exception& e) {
            std::cerr << "系统 " << (*it)->getName() << " 清理失败: " << e.what() << std::endl;
        }
    }

    initialized = false;
    std::cout << "RenderWorld 清理完成" << std::endl;
}

void RenderWorld::registerSystem(std::unique_ptr<ISystem> system) {
    if (!system) {
        std::cerr << "错误：尝试注册空系统" << std::endl;
        return;
    }

    std::cout << "注册系统: " << system->getName()
              << " (优先级: " << system->getPriority() << ")" << std::endl;

    systems.push_back(std::move(system));

    // 如果已经初始化，重新排序
    if (initialized) {
        sortSystemsByPriority();
    }
}

void RenderWorld::processEvent(SDL_Event& event, entt::registry& registry, bool& running) {
    // 将事件转发给输入系统
    if (auto* inputSystem = getSystem<IInputSystem>()) {
        inputSystem->processEvent(event, registry, running);
    }
}

glm::mat4 RenderWorld::getViewMatrix(entt::registry& registry) const {
    if (auto* cameraSystem = getSystem<ICameraSystem>()) {
        return cameraSystem->getViewMatrix(registry);
    }
    return glm::mat4(1.0f);
}

glm::mat4 RenderWorld::getProjectionMatrix(entt::registry& registry) const {
    if (auto* cameraSystem = getSystem<ICameraSystem>()) {
        return cameraSystem->getProjectionMatrix(registry);
    }
    return glm::mat4(1.0f);
}

void RenderWorld::printSystemsInfo() const {
    std::cout << "\n=== RenderWorld 系统信息 ===" << std::endl;
    std::cout << "总系统数量: " << systems.size() << std::endl;
    std::cout << "初始化状态: " << (initialized ? "已初始化" : "未初始化") << std::endl;

    for (size_t i = 0; i < systems.size(); ++i) {
        const auto& system = systems[i];
        std::cout << "[" << i << "] " << system->getName()
                  << " (优先级: " << system->getPriority() << ")" << std::endl;
    }
    std::cout << "========================\n" << std::endl;
}

void RenderWorld::createDefaultSystems() {
    std::cout << "创建默认系统配置..." << std::endl;

    // 按功能重要性和依赖关系注册系统
    registerSystem(std::make_unique<TransformSystem>());      // 最高优先级
    registerSystem(std::make_unique<AnimationSystem>());      // 动画系统
    registerSystem(std::make_unique<InputSystem>());          // 输入系统
    registerSystem(std::make_unique<CameraSystem>());         // 相机系统
    registerSystem(std::make_unique<InstancedRenderSystem>());// 实例化渲染
    registerSystem(std::make_unique<InstanceControlSystem>());// 实例控制
    registerSystem(std::make_unique<LightManager>());         // 光照管理

    std::cout << "默认系统配置创建完成，共 " << systems.size() << " 个系统" << std::endl;
}

void RenderWorld::sortSystemsByPriority() {
    std::sort(systems.begin(), systems.end(),
              [](const std::unique_ptr<ISystem>& a, const std::unique_ptr<ISystem>& b) {
                  return a->getPriority() < b->getPriority();
              });
}
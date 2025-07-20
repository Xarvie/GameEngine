#pragma once

#include "ISystem.h"
#include <memory>
#include <vector>
#include <string>

// =========================================================================
// RenderWorld - 插件式系统管理器
// =========================================================================

/**
 * 渲染世界管理器 - 统一管理所有系统插件的生命周期和执行顺序
 */
class RenderWorld {
public:
    RenderWorld();
    ~RenderWorld();

    // 禁止拷贝和赋值
    RenderWorld(const RenderWorld&) = delete;
    RenderWorld& operator=(const RenderWorld&) = delete;

    /**
     * 初始化所有系统
     * @param registry ECS注册表
     * @return 是否成功初始化
     */
    bool initialize(entt::registry& registry);

    /**
     * 更新所有系统 - 按优先级顺序执行
     * @param registry ECS注册表
     * @param deltaTime 帧时间间隔
     */
    void update(entt::registry& registry, float deltaTime);

    /**
     * 清理所有系统
     * @param registry ECS注册表
     */
    void cleanup(entt::registry& registry);

    /**
     * 注册系统插件
     * @param system 系统实例（智能指针）
     */
    void registerSystem(std::unique_ptr<ISystem> system);

    /**
     * 根据类型获取系统 - 模板方法
     * @return 系统指针，若不存在返回nullptr
     */
    template<typename T>
    T* getSystem() {
        for (auto& system : systems) {
            if (auto* casted = dynamic_cast<T*>(system.get())) {
                return casted;
            }
        }
        return nullptr;
    }

    /**
     * 根据类型获取系统 - 常量版本
     * @return 系统指针，若不存在返回nullptr
     */
    template<typename T>
    const T* getSystem() const {
        for (const auto& system : systems) {
            if (const auto* casted = dynamic_cast<const T*>(system.get())) {
                return casted;
            }
        }
        return nullptr;
    }

    /**
     * 处理SDL事件 - 转发给输入系统
     * @param event SDL事件
     * @param registry ECS注册表
     * @param running 运行状态引用
     */
    void processEvent(SDL_Event& event, entt::registry& registry, bool& running);

    /**
     * 获取当前视图矩阵 - 从相机系统获取
     * @param registry ECS注册表
     * @return 视图矩阵
     */
    glm::mat4 getViewMatrix(entt::registry& registry) const;

    /**
     * 获取当前投影矩阵 - 从相机系统获取
     * @param registry ECS注册表
     * @return 投影矩阵
     */
    glm::mat4 getProjectionMatrix(entt::registry& registry) const;

    /**
     * 打印所有系统的状态信息
     */
    void printSystemsInfo() const;

    /**
     * 创建默认系统配置 - 自动注册所有核心系统
     */
    void createDefaultSystems();

private:
    std::vector<std::unique_ptr<ISystem>> systems;
    bool initialized = false;

    /**
     * 根据优先级排序系统
     */
    void sortSystemsByPriority();
};
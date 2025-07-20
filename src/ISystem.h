#pragma once

#include <entt/entt.hpp>
#include <SDL2/SDL.h>
#include <glm/glm.hpp>

// =========================================================================
// 系统接口定义 - 插件式架构的核心抽象
// =========================================================================

/**
 * 系统基础接口 - 所有渲染系统的统一抽象
 */
class ISystem {
public:
    virtual ~ISystem() = default;

    /**
     * 系统更新函数 - 每帧调用
     * @param registry ECS注册表
     * @param deltaTime 帧时间间隔
     */
    virtual void update(entt::registry& registry, float deltaTime) = 0;

    /**
     * 系统初始化 - 可选实现
     * @param registry ECS注册表
     */
    virtual void initialize(entt::registry& registry) {}

    /**
     * 系统清理 - 可选实现
     * @param registry ECS注册表
     */
    virtual void cleanup(entt::registry& registry) {}

    /**
     * 获取系统名称 - 用于调试和日志
     */
    virtual const char* getName() const = 0;

    /**
     * 获取系统优先级 - 数值越小，执行顺序越靠前
     */
    virtual int getPriority() const { return 100; }
};

/**
 * 动画系统接口
 */
class IAnimationSystem : public ISystem {
public:
    virtual ~IAnimationSystem() = default;

    /**
     * 强制刷新动画状态
     * @param registry ECS注册表
     */
    virtual void forceRefresh(entt::registry& registry) = 0;

    /**
     * 设置全局动画播放状态
     * @param playing 是否播放
     */
    virtual void setGlobalPlayState(bool playing) = 0;

    /**
     * 获取当前动画统计信息
     */
    virtual void printAnimationStats() const = 0;
};

/**
 * 变换系统接口
 */
class ITransformSystem : public ISystem {
public:
    virtual ~ITransformSystem() = default;

    /**
     * 标记实体变换为脏状态
     * @param registry ECS注册表
     * @param entity 目标实体
     */
    virtual void markDirty(entt::registry& registry, entt::entity entity) = 0;

    /**
     * 强制重建变换缓存
     */
    virtual void invalidateCache() = 0;

    /**
     * 打印变换系统统计信息
     */
    virtual void printStats() const = 0;
};

/**
 * 输入系统接口
 */
class IInputSystem : public ISystem {
public:
    virtual ~IInputSystem() = default;

    /**
     * 处理SDL事件
     * @param event SDL事件
     * @param registry ECS注册表
     * @param running 运行状态引用
     */
    virtual void processEvent(SDL_Event& event, entt::registry& registry, bool& running) = 0;
};

/**
 * 相机系统接口
 */
class ICameraSystem : public ISystem {
public:
    virtual ~ICameraSystem() = default;

    /**
     * 获取当前视图矩阵
     * @param registry ECS注册表
     * @return 视图矩阵
     */
    virtual glm::mat4 getViewMatrix(entt::registry& registry) const = 0;

    /**
     * 获取当前投影矩阵
     * @param registry ECS注册表
     * @return 投影矩阵
     */
    virtual glm::mat4 getProjectionMatrix(entt::registry& registry) const = 0;
};

/**
 * 实例化渲染系统接口
 */
class IInstancedRenderSystem : public ISystem {
public:
    virtual ~IInstancedRenderSystem() = default;

    /**
     * 强制更新所有实例化数据
     * @param registry ECS注册表
     */
    virtual void forceUpdateInstances(entt::registry& registry) = 0;
};

/**
 * 光照管理器接口 - 预留给后续扩展
 */
class ILightManager : public ISystem {
public:
    virtual ~ILightManager() = default;

    /**
     * 添加方向光
     * @param direction 光线方向
     * @param color 光线颜色
     * @param intensity 光线强度
     */
    virtual void addDirectionalLight(const glm::vec3& direction,
                                     const glm::vec3& color,
                                     float intensity) = 0;

    /**
     * 获取主要光线方向
     */
    virtual glm::vec3 getMainLightDirection() const = 0;
};
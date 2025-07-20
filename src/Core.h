#pragma once

// 核心包含文件
#include "GltfTools/GltfTools.h"
#include "GltfTools/AssetSerializer.h"
#include <SDL2/SDL.h>
#include "glad/glad.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/local_to_model_job.h"
#include "ozz/animation/runtime/sampling_job.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/base/span.h"
#include "ozz/base/maths/soa_transform.h"
#include "ozz/base/maths/soa_float4x4.h"
#include "ozz/base/memory/unique_ptr.h"
#include <entt/entt.hpp>
#include <iostream>
#include <chrono>
#include <vector>
#include <memory>
#include <algorithm>
#include <cmath>
#include <ctime>
#include <map>
#include <queue>
#include <deque>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <iomanip>

#define M_PI 3.1415926

using namespace spartan::asset;

// =========================================================================
// 插件式架构 - 按依赖关系有序包含
// =========================================================================

// 1. 首先包含组件定义（其他模块的基础）
#include "EntityComponents.h"

// 2. 然后包含系统接口定义（系统实现的抽象）
#include "ISystem.h"



// =========================================================================
// 着色器源码（保持向后兼容）
// =========================================================================

extern const char* vertexShaderSource;
extern const char* fragmentShaderSource;

// =========================================================================
// 全局工具函数和常量
// =========================================================================

namespace SpartanEngine {
    // 版本信息
    constexpr const char* VERSION = "v0.1";
    constexpr int VERSION_MAJOR = 0;
    constexpr int VERSION_MINOR = 1;

    // 调试助手函数
    inline void PrintEntityHierarchy(entt::registry& registry, entt::entity rootEntity, int depth = 0) {
        if (!registry.valid(rootEntity)) return;

        std::string indent(depth * 2, ' ');
        std::cout << indent << "Entity " << static_cast<uint32_t>(rootEntity);

        // 打印组件信息
        if (registry.all_of<Transform>(rootEntity)) {
            std::cout << " [Transform]";
        }
        if (registry.all_of<MeshComponent>(rootEntity)) {
            std::cout << " [Mesh]";
        }
        if (registry.all_of<SkeletonComponent>(rootEntity)) {
            std::cout << " [Skeleton]";
        }
        if (registry.all_of<MultiTrackAnimationComponent>(rootEntity)) {
            std::cout << " [Animation]";
        }
        if (registry.all_of<ModelRootTag>(rootEntity)) {
            std::cout << " [ModelRoot]";
        }

        std::cout << std::endl;

        // 递归打印子实体
        if (auto* children = registry.try_get<ChildrenComponent>(rootEntity)) {
            for (auto child : children->children) {
                PrintEntityHierarchy(registry, child, depth + 1);
            }
        }
    }

    // 性能监控助手
    class ScopedTimer {
    private:
        std::chrono::high_resolution_clock::time_point startTime;
        const char* name;

    public:
        explicit ScopedTimer(const char* timerName) : name(timerName) {
            startTime = std::chrono::high_resolution_clock::now();
        }

        ~ScopedTimer() {
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration<float, std::milli>(endTime - startTime);
            std::cout << "[TIMER] " << name << ": "
                      << std::fixed << std::setprecision(3)
                      << duration.count() << "ms" << std::endl;
        }
    };
}
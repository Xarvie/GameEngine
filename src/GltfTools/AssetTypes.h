#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <atomic>
#include <chrono>
#include <cstring>
#include <unordered_map>
#include <algorithm>
#ifdef _WIN32
#include <malloc.h>  // for _aligned_malloc
#endif
#include "ozz/base/maths/vec_float.h"
#include "ozz/base/maths/quaternion.h"
#include "ozz/base/maths/transform.h"
#include "ozz/base/memory/unique_ptr.h"

// GLM数学库（用于渲染）
#define GLM_ENABLE_EXPERIMENTAL  // 启用实验性扩展
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/type_precision.hpp>
#include <glm/ext/vector_uint4.hpp>
#include <glm/ext/vector_uint2.hpp>
#include <glm/gtc/type_ptr.hpp>

// OpenGL常量（避免包含整个glad.h）
#ifndef GL_FLOAT
#define GL_FLOAT 0x1406
#define GL_UNSIGNED_BYTE 0x1401
#define GL_UNSIGNED_SHORT 0x1403
#define GL_UNSIGNED_INT 0x1405
#endif

namespace spartan {
    namespace asset {

// =========================================================================
// 资源句柄定义
// =========================================================================

// 句柄基础模板
        template<typename T>
        struct Handle {
            uint32_t id = 0;
            uint32_t generation = 0;

            constexpr Handle() = default;
            constexpr Handle(uint32_t id, uint32_t gen) : id(id), generation(gen) {}

            bool IsValid() const { return id != 0; }
            void Invalidate() { id = 0; generation = 0; }

            bool operator==(const Handle& other) const {
                return id == other.id && generation == other.generation;
            }

            bool operator!=(const Handle& other) const {
                return !(*this == other);
            }

            bool operator<(const Handle& other) const {
                return id < other.id || (id == other.id && generation < other.generation);
            }
        };

// 句柄哈希函数
        template<typename T>
        struct HandleHash {
            size_t operator()(const Handle<T>& handle) const {
                return std::hash<uint64_t>{}((uint64_t(handle.id) << 32) | handle.generation);
            }
        };

// 具体句柄类型
        using MeshHandle = Handle<struct MeshTag>;
        using MaterialHandle = Handle<struct MaterialTag>;
        using TextureHandle = Handle<struct TextureTag>;
        using SkeletonHandle = Handle<struct SkeletonTag>;
        using AnimationHandle = Handle<struct AnimationTag>;
        using ShaderHandle = Handle<struct ShaderTag>;

// =========================================================================
// 基础数学类型转换
// =========================================================================

// OZZ到GLM的转换
        inline glm::vec3 ToGLM(const ozz::math::Float3& v) {
            return glm::vec3(v.x, v.y, v.z);
        }

        inline glm::vec4 ToGLM(const ozz::math::Float4& v) {
            return glm::vec4(v.x, v.y, v.z, v.w);
        }

        inline glm::quat ToGLM(const ozz::math::Quaternion& q) {
            return glm::quat(q.w, q.x, q.y, q.z);
        }

        inline glm::mat4 ToGLM(const float* m) {
            // glTF 和 GLM 都是列主序 (Column-Major)，所以我们直接从内存加载即可，无需转置。
            // 十分确定
            return glm::make_mat4(m);
        }

// GLM到OZZ的转换
        inline ozz::math::Float3 ToOZZ(const glm::vec3& v) {
            return ozz::math::Float3(v.x, v.y, v.z);
        }

        inline ozz::math::Float4 ToOZZ(const glm::vec4& v) {
            return ozz::math::Float4(v.x, v.y, v.z, v.w);
        }

        inline ozz::math::Quaternion ToOZZ(const glm::quat& q) {
            return ozz::math::Quaternion(q.x, q.y, q.z, q.w);
        }

// =========================================================================
// 通用常量
// =========================================================================

        namespace Constants {
            // 顶点属性位置
            constexpr uint32_t ATTRIB_POSITION = 0;
            constexpr uint32_t ATTRIB_NORMAL = 1;
            constexpr uint32_t ATTRIB_UV0 = 2;
            constexpr uint32_t ATTRIB_TANGENT = 3;
            constexpr uint32_t ATTRIB_JOINTS = 4;
            constexpr uint32_t ATTRIB_WEIGHTS = 5;
            constexpr uint32_t ATTRIB_INSTANCE_MATRIX = 6; // 实例化矩阵起始位置

            // 限制
            constexpr uint32_t MAX_BONES_PER_VERTEX = 4;
            constexpr uint32_t MAX_UV_SETS = 2;

            // GPU限制 - 只使用纹理传输
            constexpr uint32_t MAX_TEXTURE_BONES = 256;    // 纹理骨骼数限制
            constexpr uint32_t MAX_INSTANCES = 10000;       // 最大实例数

            // 默认值
            constexpr float DEFAULT_ANIMATION_FPS = 30.0f;
            constexpr float LOD_TRANSITION_TIME = 0.3f;
        }

// =========================================================================
// 错误码
// =========================================================================

        enum class ErrorCode : uint32_t {
            SUCCESS = 0,

            // 文件错误
            FILE_NOT_FOUND = 100,
            FILE_READ_ERROR,
            FILE_WRITE_ERROR,
            FILE_FORMAT_ERROR,

            // 资源错误
            RESOURCE_NOT_FOUND = 200,
            RESOURCE_ALREADY_EXISTS,
            RESOURCE_INVALID,
            RESOURCE_LIMIT_EXCEEDED,

            // 动画错误
            ANIMATION_INVALID = 300,
            ANIMATION_SKELETON_MISMATCH,
            ANIMATION_TRACK_MISSING,

            // GPU错误
            GPU_RESOURCE_CREATION_FAILED = 400,
            GPU_OUT_OF_MEMORY,
            GPU_FEATURE_NOT_SUPPORTED,

            // 验证错误
            VALIDATION_FAILED = 500,
            VALIDATION_MESH_INVALID,
            VALIDATION_SKELETON_INVALID,
            VALIDATION_ANIMATION_INVALID,
        };

// 错误结果类型
        template<typename T>
        class Result {
        public:
            Result(T&& value) : value_(std::move(value)), error_(ErrorCode::SUCCESS) {}
            Result(ErrorCode error) : error_(error) {}

            bool IsSuccess() const { return error_ == ErrorCode::SUCCESS; }
            bool IsError() const { return error_ != ErrorCode::SUCCESS; }

            ErrorCode GetError() const { return error_; }
            const T& GetValue() const { return value_; }
            T& GetValue() { return value_; }

            operator bool() const { return IsSuccess(); }

        private:
            T value_;
            ErrorCode error_;
        };

// =========================================================================
// 内存分配器接口
// =========================================================================

        class IAllocator {
        public:
            virtual ~IAllocator() = default;

            virtual void* Allocate(size_t size, size_t alignment = 16) = 0;
            virtual void Deallocate(void* ptr) = 0;
            virtual size_t GetAllocatedSize() const = 0;
        };

// 默认分配器
        class DefaultAllocator : public IAllocator {
        public:
            DefaultAllocator() = default;
            ~DefaultAllocator() {
                if (allocated_size_.load() > 0) {
                    // 警告：可能存在内存泄漏
                    printf("Warning: DefaultAllocator destroyed with %zu bytes still allocated\n",
                           allocated_size_.load());
                }
            }

            void* Allocate(size_t size, size_t alignment = 16) override {
                void* ptr = nullptr;
#ifdef _WIN32
                ptr = _aligned_malloc(size, alignment);
#else
                // POSIX要求size是alignment的倍数
                size_t aligned_size = ((size + alignment - 1) / alignment) * alignment;
                ptr = aligned_alloc(alignment, aligned_size);
#endif
                if (ptr) {
                    allocated_size_.fetch_add(size);

                    // 在生产代码中，应该使用mutex保护这个map
                    // 这里为了简单起见，假设是单线程使用
                    allocation_map_[ptr] = size;
                }
                return ptr;
            }

            void Deallocate(void* ptr) override {
                if (ptr) {
                    // 查找原始分配大小
                    auto it = allocation_map_.find(ptr);
                    if (it != allocation_map_.end()) {
                        allocated_size_.fetch_sub(it->second);
                        allocation_map_.erase(it);
                    }

#ifdef _WIN32
                    _aligned_free(ptr);
#else
                    free(ptr);
#endif
                }
            }

            size_t GetAllocatedSize() const override {
                return allocated_size_.load();
            }

        private:
            std::atomic<size_t> allocated_size_{0};
            std::unordered_map<void*, size_t> allocation_map_;  // 注意：线程不安全
        };

// =========================================================================
// 性能计时器
// =========================================================================

        class ScopedTimer {
        public:
            using Callback = std::function<void(float ms)>;

            explicit ScopedTimer(Callback callback)
                    : callback_(callback), start_time_(std::chrono::high_resolution_clock::now()) {}

            ~ScopedTimer() {
                auto end_time = std::chrono::high_resolution_clock::now();
                float ms = std::chrono::duration<float, std::milli>(end_time - start_time_).count();
                if (callback_) callback_(ms);
            }

        private:
            Callback callback_;
            std::chrono::high_resolution_clock::time_point start_time_;
        };

#define PROFILE_SCOPE(name) \
    ScopedTimer timer_##__LINE__([](float ms) { \
        printf("Profile: %s took %.2f ms\n", name, ms); \
    })

    } // namespace asset
} // namespace spartan
#pragma once

#include <cstdint>
#include <vector>
#include <fstream>
#include "AssetTypes.h"
#include "GltfTools.h"

namespace spartan {
    namespace asset {

// 序列化文件格式定义
        struct AssetFileHeader {
            static constexpr uint32_t MAGIC = 0x54525053; // 'SPRT'
            static constexpr uint32_t VERSION = 1;

            uint32_t magic;
            uint32_t version;
            uint32_t flags; // 压缩等标志
            uint32_t chunk_count;
            uint64_t total_size;
            uint64_t checksum; // 可选的校验和
        };

// 数据块类型
        enum class ChunkType : uint32_t {
            METADATA = 0,
            MESHES,
            MATERIALS,
            TEXTURES,
            SKELETONS,
            ANIMATIONS,
            SCENE_NODES,
            STRING_TABLE,
            CHUNK_COUNT
        };

// 数据块头
        struct ChunkHeader {
            ChunkType type;
            uint32_t item_count;
            uint64_t offset;
            uint64_t size;
            uint64_t uncompressed_size; // 如果压缩的话
        };

// 字符串表（避免重复存储字符串）
        class StringTable {
        public:
            uint32_t AddString(const char* str);
            uint32_t AddString(const ozz::string& str);
            const char* GetString(uint32_t index) const;

            void Serialize(std::vector<uint8_t>& buffer) const;
            bool Deserialize(const uint8_t* data, size_t size);

            void Clear() { strings_.clear(); string_map_.clear(); }

        private:
            std::vector<std::string> strings_;
            std::unordered_map<std::string, uint32_t> string_map_;
        };

// 序列化器主类
        class AssetSerializer {
        public:
            AssetSerializer() = default;
            ~AssetSerializer() = default;

            // 序列化到文件
            bool SerializeToFile(const ProcessedAsset& asset, const char* filepath);

            // 从文件反序列化
            bool DeserializeFromFile(ProcessedAsset& asset, const char* filepath);

            // 获取错误信息
            const std::string& GetLastError() const { return last_error_; }

            // 设置压缩选项
            void SetCompressionEnabled(bool enabled) { enable_compression_ = enabled; }

        private:
            // 序列化各种资源
            void SerializeMeshes(const ProcessedAsset& asset, std::vector<uint8_t>& buffer);
            void SerializeMaterials(const ProcessedAsset& asset, std::vector<uint8_t>& buffer);
            void SerializeTextures(const ProcessedAsset& asset, std::vector<uint8_t>& buffer);
            void SerializeSkeletons(const ProcessedAsset& asset, std::vector<uint8_t>& buffer);
            void SerializeAnimations(const ProcessedAsset& asset, std::vector<uint8_t>& buffer);
            void SerializeSceneNodes(const ProcessedAsset& asset, std::vector<uint8_t>& buffer);
            void SerializeMetadata(const ProcessedAsset& asset, std::vector<uint8_t>& buffer);

            // 反序列化各种资源
            bool DeserializeMeshes(ProcessedAsset& asset, const uint8_t* data, size_t size);
            bool DeserializeMaterials(ProcessedAsset& asset, const uint8_t* data, size_t size);
            bool DeserializeTextures(ProcessedAsset& asset, const uint8_t* data, size_t size);
            bool DeserializeSkeletons(ProcessedAsset& asset, const uint8_t* data, size_t size);
            bool DeserializeAnimations(ProcessedAsset& asset, const uint8_t* data, size_t size);
            bool DeserializeSceneNodes(ProcessedAsset& asset, const uint8_t* data, size_t size);
            bool DeserializeMetadata(ProcessedAsset& asset, const uint8_t* data, size_t size);

            // 工具函数
            template<typename T>
            void Write(std::vector<uint8_t>& buffer, const T& value);

            template<typename T>
            bool Read(const uint8_t*& data, size_t& remaining, T& value);

            void WriteString(std::vector<uint8_t>& buffer, const char* str);
            void WriteString(std::vector<uint8_t>& buffer, const ozz::string& str);
            bool ReadString(const uint8_t*& data, size_t& remaining, std::string& str);

            // 压缩/解压（可选）
            bool CompressChunk(const std::vector<uint8_t>& input, std::vector<uint8_t>& output);
            bool DecompressChunk(const uint8_t* input, size_t input_size,
                                 std::vector<uint8_t>& output, size_t output_size);

            // 错误处理
            void SetError(const std::string& error);

        private:
            StringTable string_table_;
            std::string last_error_;
            bool enable_compression_ = false;

            // 临时缓冲区，避免频繁分配
            std::vector<uint8_t> temp_buffer_;
        };

// 内联实现
        template<typename T>
        inline void AssetSerializer::Write(std::vector<uint8_t>& buffer, const T& value) {
            size_t offset = buffer.size();
            buffer.resize(offset + sizeof(T));
            std::memcpy(buffer.data() + offset, &value, sizeof(T));
        }

        template<typename T>
        inline bool AssetSerializer::Read(const uint8_t*& data, size_t& remaining, T& value) {
            if (remaining < sizeof(T)) {
                return false;
            }
            std::memcpy(&value, data, sizeof(T));
            data += sizeof(T);
            remaining -= sizeof(T);
            return true;
        }

    } // namespace asset
} // namespace spartan
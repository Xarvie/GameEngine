#pragma once

#include <vector>
#include <cstring>
#include <cstdio>
#include <memory>
#include "ozz/base/io/stream.h"
#include "ozz/base/io/archive.h"

namespace spartan {
    namespace asset {

// 辅助类：用于序列化OZZ对象到内存缓冲区
        class OzzMemoryBuffer : public ozz::io::Stream {
        public:
            OzzMemoryBuffer() : position_(0) {}

            // 从已有数据构造（用于反序列化）
            explicit OzzMemoryBuffer(const uint8_t* data, size_t size)
                    : buffer_(data, data + size), position_(0) {}

            // Stream接口实现
            bool opened() const override { return true; }

            size_t Read(void* buffer, size_t size) override {
                if (position_ + size > buffer_.size()) {
                    size = buffer_.size() - position_;
                }
                if (size > 0) {
                    std::memcpy(buffer, buffer_.data() + position_, size);
                    position_ += size;
                }
                return size;
            }

            size_t Write(const void* buffer, size_t size) override {
                size_t new_size = position_ + size;
                if (new_size > buffer_.size()) {
                    buffer_.resize(new_size);
                }
                std::memcpy(buffer_.data() + position_, buffer, size);
                position_ += size;
                return size;
            }

            int Seek(int offset, Origin origin) override {
                size_t new_pos = position_;
                switch (origin) {
                    case kSet:
                        new_pos = offset;
                        break;
                    case kCurrent:
                        new_pos = position_ + offset;
                        break;
                    case kEnd:
                        new_pos = buffer_.size() + offset;
                        break;
                }

                if (new_pos > buffer_.size()) {
                    return -1;
                }

                position_ = new_pos;
                return 0;
            }

            int Tell() const override {
                return static_cast<int>(position_);
            }

            size_t Size() const override {
                return buffer_.size();
            }

            // 获取内部缓冲区
            const std::vector<uint8_t>& GetBuffer() const { return buffer_; }
            std::vector<uint8_t>& GetBuffer() { return buffer_; }

        private:
            std::vector<uint8_t> buffer_;
            size_t position_;
        };

// 辅助函数：序列化OZZ对象到缓冲区
        template<typename T>
        inline std::vector<uint8_t> SerializeOzzObject(const T& object) {
            OzzMemoryBuffer buffer;
            ozz::io::OArchive archive(&buffer);
            archive << object;
            return std::move(buffer.GetBuffer());
        }

// 辅助函数：从缓冲区反序列化OZZ对象
        template<typename T>
        inline bool DeserializeOzzObject(const uint8_t* data, size_t size, T& object) {
            OzzMemoryBuffer buffer(data, size);
            ozz::io::IArchive archive(&buffer);
            archive >> object;
            return true;
        }

    } // namespace asset
} // namespace spartan
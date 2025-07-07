#ifndef EXAMPLE_UTIL_H
#define EXAMPLE_UTIL_H

#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <map>
#include <cmath>
#include <algorithm>
#include <unordered_map>

#include <glad/glad.h>
#include <SDL2/SDL.h>

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"


#include "stb_image.h"

#include "ozz/base/maths/vec_float.h"
#include "ozz/base/maths/simd_math.h"

// ============================================================================
// 数据结构定义
// ============================================================================

// 顶点数据结构定义
struct VertexData {
    ozz::math::Float3 position;
    ozz::math::Float3 normal;
    ozz::math::Float2 uv0;
    ozz::math::Float4 tangent;
    uint16_t joint_indices[4];
    ozz::math::Float4 joint_weights;
};

// 材质数据结构
struct MaterialData {
    GLuint base_color_texture = 0;
    GLuint metallic_roughness_texture = 0;
    GLuint normal_texture = 0;
    GLuint occlusion_texture = 0;
    GLuint emissive_texture = 0;
    glm::vec4 base_color_factor = glm::vec4(1.0f);
    float metallic_factor = 1.0f;
    float roughness_factor = 1.0f;
    glm::vec3 emissive_factor = glm::vec3(0.0f);
    bool is_transparent = false;
};


// ============================================================================
// 纹理管理器
// ============================================================================

// ES 3.0 兼容的纹理管理器
class SimpleTextureManager {
private:
    std::unordered_map<std::string, GLuint> texture_cache_;

public:
    GLuint LoadTexture(const std::string& path) {
        if (path.empty()) return 0;

        auto it = texture_cache_.find(path);
        if (it != texture_cache_.end()) {
            return it->second;
        }

        std::cout << "Trying to load texture: " << path << std::endl;

        int width, height, channels;
        stbi_set_flip_vertically_on_load(false); // 不能设为true
        unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 0);

        if (!data) {
            std::cout << "Failed to load texture: " << path << std::endl;
            std::cout << "STB Error: " << stbi_failure_reason() << std::endl;
            return 0;
        }

        GLuint texture_id;
        glGenTextures(1, &texture_id);
        glBindTexture(GL_TEXTURE_2D, texture_id);

        // ES 3.0 兼容的纹理参数
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // ES 3.0 只使用基本格式，在着色器中处理sRGB转换
        GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;
        GLenum internal_format = (channels == 4) ? GL_RGBA : GL_RGB;

        glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        stbi_image_free(data);
        texture_cache_[path] = texture_id;

        std::cout << "Loaded texture: " << path << " (" << width << "x" << height << ")" << std::endl;
        return texture_id;
    }

    ~SimpleTextureManager() {
        for (auto& pair : texture_cache_) {
            glDeleteTextures(1, &pair.second);
        }
    }
};


// ============================================================================
// 辅助函数
// ============================================================================

// ozz矩阵到GLM矩阵的转换
inline glm::mat4 OzzToGlm(const ozz::math::Float4x4& ozz_matrix) {
    glm::mat4 result;
    std::memcpy(&result, &ozz_matrix, sizeof(glm::mat4));
    return result;
}

// 绑定材质的辅助函数
inline void BindMaterial(GLuint program, const MaterialData& mat) {
    glUniform4fv(glGetUniformLocation(program, "u_baseColorFactor"), 1, glm::value_ptr(mat.base_color_factor));
    glUniform1f(glGetUniformLocation(program, "u_metallicFactor"), mat.metallic_factor);
    glUniform1f(glGetUniformLocation(program, "u_roughnessFactor"), mat.roughness_factor);
    glUniform3fv(glGetUniformLocation(program, "u_emissiveFactor"), 1, glm::value_ptr(mat.emissive_factor));

    // 基础颜色纹理 (Texture Unit 0)
    glUniform1i(glGetUniformLocation(program, "u_hasBaseColorTexture"), 0);
    if (mat.base_color_texture > 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mat.base_color_texture);
        glUniform1i(glGetUniformLocation(program, "u_baseColorTexture"), 0);
        glUniform1i(glGetUniformLocation(program, "u_hasBaseColorTexture"), 1);
    }

    // 金属度粗糙度纹理 (Texture Unit 1)
    glUniform1i(glGetUniformLocation(program, "u_hasMetallicRoughnessTexture"), 0);
    if (mat.metallic_roughness_texture > 0) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, mat.metallic_roughness_texture);
        glUniform1i(glGetUniformLocation(program, "u_metallicRoughnessTexture"), 1);
        glUniform1i(glGetUniformLocation(program, "u_hasMetallicRoughnessTexture"), 1);
    }

    // 法线纹理 (Texture Unit 2)
    glUniform1i(glGetUniformLocation(program, "u_hasNormalTexture"), 0);
    if (mat.normal_texture > 0) {
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, mat.normal_texture);
        glUniform1i(glGetUniformLocation(program, "u_normalTexture"), 2);
        glUniform1i(glGetUniformLocation(program, "u_hasNormalTexture"), 1);
    }

    // [修复] 遮蔽纹理 (Texture Unit 3)
    glUniform1i(glGetUniformLocation(program, "u_hasOcclusionTexture"), 0);
    if (mat.occlusion_texture > 0) {
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, mat.occlusion_texture);
        glUniform1i(glGetUniformLocation(program, "u_occlusionTexture"), 3);
        glUniform1i(glGetUniformLocation(program, "u_hasOcclusionTexture"), 1);
    }

    // [修复] 自发光纹理 (Texture Unit 4)
    glUniform1i(glGetUniformLocation(program, "u_hasEmissiveTexture"), 0);
    if (mat.emissive_texture > 0) {
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, mat.emissive_texture);
        glUniform1i(glGetUniformLocation(program, "u_emissiveTexture"), 4);
        glUniform1i(glGetUniformLocation(program, "u_hasEmissiveTexture"), 1);
    }
}

// 着色器编译和链接函数
inline GLuint CompileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cerr << "Shader compilation failed: " << infoLog << std::endl;
    }
    return shader;
}

inline GLuint CreateShaderProgram(const char* vertexSource, const char* fragmentSource) {
    GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentSource);
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLchar infoLog[512];
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        std::cerr << "Shader program linking failed: " << infoLog << std::endl;
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return program;
}

#endif //EXAMPLE_UTIL_H

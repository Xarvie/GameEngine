#include "Renderer.h"
#include <iostream>
// =========================================================================
// BatchingState 实现
// =========================================================================

void Renderer::BatchingState::ensureBufferCreated() {
    if (tempInstanceBuffer == 0) {
        glGenBuffers(1, &tempInstanceBuffer);
    }
}

void Renderer::BatchingState::cleanup() {
    if (tempInstanceBuffer != 0) {
        glDeleteBuffers(1, &tempInstanceBuffer);
        tempInstanceBuffer = 0;
    }
}

// =========================================================================
// Renderer 实现
// =========================================================================

bool Renderer::initialize() {
    if (!device.initialize()) {
        return false;
    }
    return true;
}

void Renderer::cleanup() {
    if (isCleanedUp) {
        return;
    }
    isCleanedUp = true;

    std::cout << "正在清理渲染器资源..." << std::endl;

    if (!device.getGLContext()) {
        std::cout << "OpenGL上下文已无效，跳过GPU资源清理" << std::endl;
        return;
    }

    for (auto& [handle, mesh] : asset.meshes) {
        if (mesh.vao) {
            glDeleteVertexArrays(1, &mesh.vao);
            mesh.vao = 0;
        }
        if (mesh.vbo) {
            glDeleteBuffers(1, &mesh.vbo);
            mesh.vbo = 0;
        }
        if (mesh.ibo) {
            glDeleteBuffers(1, &mesh.ibo);
            mesh.ibo = 0;
        }
    }

    for (auto& [handle, texture] : asset.textures) {
        if (texture.gpu_texture_id) {
            glDeleteTextures(1, &texture.gpu_texture_id);
            texture.gpu_texture_id = 0;
        }
    }

    // 清理动态合批缓冲区
    batchingState.cleanup();

    std::cout << "渲染器资源清理完成" << std::endl;
}

bool Renderer::loadAsset(const char* gltfPath) {
    ProcessConfig config;
    config.generate_tangents = true;
    config.optimize_vertex_cache = true;

    GltfProcessor processor(config);
    processor.SetProgressCallback([](float progress, const char* stage) {
        std::cout << "加载进度: " << int(progress * 100) << "% - " << stage << std::endl;
    });

    if (!processor.ProcessFile(gltfPath, asset)) {
        std::cerr << "加载模型失败: " << processor.GetLastError() << std::endl;
        return false;
    }

    for (auto& [handle, mesh] : asset.meshes) {
        uploadMesh(mesh);
    }

    for (auto& [handle, texture] : asset.textures) {
        device.createDummyTexture(texture);
    }

    return true;
}

void Renderer::uploadMesh(MeshData& mesh) {
    glGenVertexArrays(1, &mesh.vao);
    glBindVertexArray(mesh.vao);

    glGenBuffers(1, &mesh.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, mesh.vertex_buffer.size(),
                 mesh.vertex_buffer.data(), GL_STATIC_DRAW);

    const auto& format = mesh.format;

    if (format.attributes & VertexFormat::POSITION) {
        auto it = format.attribute_map.find(VertexFormat::POSITION);
        if (it != format.attribute_map.end()) {
            const auto& info = it->second;
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, info.components, info.type, info.normalized,
                                  format.stride, (void*)(uintptr_t)info.offset);
        }
    }

    if (format.attributes & VertexFormat::NORMAL) {
        auto it = format.attribute_map.find(VertexFormat::NORMAL);
        if (it != format.attribute_map.end()) {
            const auto& info = it->second;
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, info.components, info.type, info.normalized,
                                  format.stride, (void*)(uintptr_t)info.offset);
        }
    }

    if (format.attributes & VertexFormat::UV0) {
        auto it = format.attribute_map.find(VertexFormat::UV0);
        if (it != format.attribute_map.end()) {
            const auto& info = it->second;
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, info.components, info.type, info.normalized,
                                  format.stride, (void*)(uintptr_t)info.offset);
        }
    }

    if (format.attributes & VertexFormat::JOINTS0) {
        auto it = format.attribute_map.find(VertexFormat::JOINTS0);
        if (it != format.attribute_map.end()) {
            const auto& info = it->second;
            glEnableVertexAttribArray(4);
            glVertexAttribIPointer(4, info.components, info.type,
                                   format.stride, (void*)(uintptr_t)info.offset);
        }
    }

    if (format.attributes & VertexFormat::WEIGHTS0) {
        auto it = format.attribute_map.find(VertexFormat::WEIGHTS0);
        if (it != format.attribute_map.end()) {
            const auto& info = it->second;
            glEnableVertexAttribArray(5);
            glVertexAttribPointer(5, info.components, info.type, info.normalized,
                                  format.stride, (void*)(uintptr_t)info.offset);
        }
    }

    if (!mesh.index_buffer.empty()) {
        glGenBuffers(1, &mesh.ibo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.index_buffer.size() * sizeof(uint32_t),
                     mesh.index_buffer.data(), GL_STATIC_DRAW);
    }

    glBindVertexArray(0);
    mesh.data_state = MeshData::SYNCED;
}

void Renderer::setupMaterial(const MaterialHandle& materialHandle) {
    GLuint shaderProgram = device.getShaderProgram();
    GLuint defaultTexture = device.getDefaultTexture();
    
    if (materialHandle.IsValid()) {
        auto matIt = asset.materials.find(materialHandle);
        if (matIt != asset.materials.end()) {
            const auto& material = matIt->second;
            glUniform4f(glGetUniformLocation(shaderProgram, "uBaseColorFactor"),
                        material.base_color_factor.x, material.base_color_factor.y,
                        material.base_color_factor.z, material.base_color_factor.w);

            glActiveTexture(GL_TEXTURE0);
            GLuint textureId = defaultTexture;
            if (material.base_color_texture.has_value()) {
                auto texIt = asset.textures.find(material.base_color_texture.value());
                if (texIt != asset.textures.end()) {
                    textureId = texIt->second.gpu_texture_id;
                }
            }
            glBindTexture(GL_TEXTURE_2D, textureId);
            glUniform1i(glGetUniformLocation(shaderProgram, "uBaseColorTexture"), 0);
        }
    } else {
        glUniform4f(glGetUniformLocation(shaderProgram, "uBaseColorFactor"), 1.0f, 1.0f, 1.0f, 1.0f);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, defaultTexture);
        glUniform1i(glGetUniformLocation(shaderProgram, "uBaseColorTexture"), 0);
    }
}

void Renderer::setupInstancedVAO(MeshData& mesh, uint32_t instanceBuffer) {
    if (!mesh.HasGPUData()) {
        std::cerr << "错误：网格没有GPU数据，无法设置实例化VAO" << std::endl;
        return;
    }

    glBindVertexArray(mesh.vao);

    glBindBuffer(GL_ARRAY_BUFFER, instanceBuffer);

    for (int i = 0; i < 4; i++) {
        glEnableVertexAttribArray(6 + i);
        glVertexAttribPointer(6 + i, 4, GL_FLOAT, GL_FALSE,
                              sizeof(glm::mat4),
                              (void*)(i * sizeof(glm::vec4)));
        glVertexAttribDivisor(6 + i, 1);
    }

    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBindVertexArray(0);
}

void Renderer::executeDrawMesh(const RenderCommand::DrawMeshData& data) {
    const auto& mesh = *data.mesh;
    const auto& submesh = *data.submesh;
    GLuint shaderProgram = device.getShaderProgram();

    glBindVertexArray(mesh.vao);

    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(data.modelMatrix));
    glUniform1i(glGetUniformLocation(shaderProgram, "uUseInstancing"), 0);

    bool isSkinned = (mesh.format.attributes & VertexFormat::JOINTS0) && mesh.skeleton.has_value();
    glUniform1i(glGetUniformLocation(shaderProgram, "uUseSkinning"), isSkinned ? 1 : 0);

    if (data.wireframe) {
        for (uint32_t i = 0; i < submesh.index_count; i += 3) {
            glDrawElements(GL_LINE_LOOP, 3, GL_UNSIGNED_INT, (void*)((submesh.index_offset + i) * sizeof(uint32_t)));
        }
    } else {
        glDrawElements(GL_TRIANGLES, submesh.index_count, GL_UNSIGNED_INT, (void*)(submesh.index_offset * sizeof(uint32_t)));
    }
}

void Renderer::executeDrawInstancedMesh(const RenderCommand::DrawInstancedMeshData& data) {
    const auto& mesh = *data.mesh;
    const auto& submesh = *data.submesh;
    GLuint shaderProgram = device.getShaderProgram();

    glBindVertexArray(mesh.vao);

    glUniform1i(glGetUniformLocation(shaderProgram, "uUseInstancing"), 1);

    bool isSkinned = (mesh.format.attributes & VertexFormat::JOINTS0) && mesh.skeleton.has_value();
    glUniform1i(glGetUniformLocation(shaderProgram, "uUseSkinning"), isSkinned ? 1 : 0);

    setupMaterial(data.material);

    uint32_t instanceCount = static_cast<uint32_t>(data.instanceMatrices->size());
    glDrawElementsInstanced(
            GL_TRIANGLES,
            submesh.index_count,
            GL_UNSIGNED_INT,
            (void*)(submesh.index_offset * sizeof(uint32_t)),
            instanceCount
    );
}

void Renderer::executeSetBones(const RenderCommand::SetBonesData& data) {
//    uploadSkinningMatrices(*data.boneMatrices);
    // 注意：骨骼矩阵现在由BoneTextureManager统一管理
    // 这个方法保留用于向后兼容，但实际上已经不再使用
    // 新的渲染管线通过uBoneOffset uniform直接访问纹理中的数据
}

void Renderer::executeSetUniform(const RenderCommand::SetUniformData& data) {
    GLuint shaderProgram = device.getShaderProgram();
    GLint location = glGetUniformLocation(shaderProgram, data.name.c_str());
    if (location == -1) return;

    switch (data.type) {
        case RenderCommand::SetUniformData::MAT4:
            glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(data.mat4Value));
            break;
        case RenderCommand::SetUniformData::VEC3:
            glUniform3fv(location, 1, glm::value_ptr(data.vec3Value));
            break;
        case RenderCommand::SetUniformData::VEC4:
            glUniform4fv(location, 1, glm::value_ptr(data.vec4Value));
            break;
        case RenderCommand::SetUniformData::FLOAT:
            glUniform1f(location, data.floatValue);
            break;
        case RenderCommand::SetUniformData::INT:
            glUniform1i(location, data.intValue);
            break;
    }
}

// 动态合批绘制函数
void Renderer::executeBatchedDraw(const RenderCommand::DrawMeshData& data, const std::vector<glm::mat4>& instanceMatrices) {
    const auto& mesh = *data.mesh;
    const auto& submesh = *data.submesh;
    GLuint shaderProgram = device.getShaderProgram();

    // 上传实例矩阵到临时缓冲区
    batchingState.ensureBufferCreated();
    glBindBuffer(GL_ARRAY_BUFFER, batchingState.tempInstanceBuffer);
    glBufferData(GL_ARRAY_BUFFER,
                 instanceMatrices.size() * sizeof(glm::mat4),
                 instanceMatrices.data(),
                 GL_STREAM_DRAW);

    // 设置VAO的实例化属性
    glBindVertexArray(mesh.vao);
    for (int i = 0; i < 4; i++) {
        glEnableVertexAttribArray(6 + i);
        glVertexAttribPointer(6 + i, 4, GL_FLOAT, GL_FALSE,
                              sizeof(glm::mat4),
                              (void*)(i * sizeof(glm::vec4)));
        glVertexAttribDivisor(6 + i, 1);
    }

    // 恢复顶点缓冲区绑定
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);

    glUniform1i(glGetUniformLocation(shaderProgram, "uUseInstancing"), 1);
    bool isSkinned = (mesh.format.attributes & VertexFormat::JOINTS0) && mesh.skeleton.has_value();
    glUniform1i(glGetUniformLocation(shaderProgram, "uUseSkinning"), isSkinned ? 1 : 0);

    glDrawElementsInstanced(
            GL_TRIANGLES,
            submesh.index_count,
            GL_UNSIGNED_INT,
            (void*)(submesh.index_offset * sizeof(uint32_t)),
            static_cast<GLsizei>(instanceMatrices.size())
    );

    // 清理实例化属性（避免影响后续非实例化绘制）
    for (int i = 0; i < 4; i++) {
        glVertexAttribDivisor(6 + i, 0);
        glDisableVertexAttribArray(6 + i);
    }
}
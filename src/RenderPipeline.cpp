#include "RenderPipeline.h"
#include "AnimationTask.h"

// =========================================================================
// RenderPipeline 实现
// =========================================================================

void RenderPipeline::clearRenderQueue() {
    renderQueue.clear();
}

void RenderPipeline::addRenderCommand(const RenderCommand& command) {
    renderQueue.push_back(command);
}

void RenderPipeline::processRenderQueue() {
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 排序渲染队列
    std::sort(renderQueue.begin(), renderQueue.end(), [](const RenderCommand& a, const RenderCommand& b) {
        return a.sortKey < b.sortKey;
    });

    GLuint shaderProgram = device.getShaderProgram();
    glUseProgram(shaderProgram);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, device.getBoneTexture());
    glUniform1i(glGetUniformLocation(shaderProgram, "uBoneTexture"), 1);

    processBatchedRendering();

    glBindVertexArray(0);
}

void RenderPipeline::processBatchedRendering() {
    // 动态合批统计
    MaterialHandle currentMaterial;
    currentMaterial.Invalidate();

    size_t i = 0;
    while (i < renderQueue.size()) {
        const auto& command = renderQueue[i];

        // 处理非绘制命令
        if (command.type != RenderCommandType::DRAW_MESH &&
            command.type != RenderCommandType::DRAW_INSTANCED_MESH) {
            switch (command.type) {
                case RenderCommandType::SET_BONES:
                    renderer.executeSetBones(command.setBones);
                    break;
                case RenderCommandType::SET_UNIFORM:
                    renderer.executeSetUniform(command.setUniform);
                    break;
                default:
                    break;
            }
            ++i;
            continue;
        }

        // 处理实例化绘制命令
        if (command.type == RenderCommandType::DRAW_INSTANCED_MESH) {
            if (command.drawInstancedMesh.material != currentMaterial) {
                renderer.setupMaterial(command.drawInstancedMesh.material);
                currentMaterial = command.drawInstancedMesh.material;
            }
            renderer.executeDrawInstancedMesh(command.drawInstancedMesh);
            ++i;
            batchingStats.totalDrawCalls++;
            continue;
        }

        // 处理 DRAW_MESH 命令，尝试合批
        if (command.type == RenderCommandType::DRAW_MESH) {
            const auto& firstDraw = command.drawMesh;

            if (firstDraw.material != currentMaterial) {
                renderer.setupMaterial(firstDraw.material);
                currentMaterial = firstDraw.material;
            }

            bool canBatch = !firstDraw.wireframe;

            if (!canBatch) {
                renderer.executeDrawMesh(firstDraw);
                ++i;
                batchingStats.totalDrawCalls++;
                continue;
            }

            std::vector<glm::mat4> instanceMatrices;
            instanceMatrices.push_back(firstDraw.modelMatrix);

            size_t batchStart = i;
            size_t batchCount = 1;
            const size_t maxInstances = 1000;

            // 查找后续可以合批的命令
            while (i + batchCount < renderQueue.size() && batchCount < maxInstances) {
                const auto& nextCommand = renderQueue[i + batchCount];
                if (nextCommand.type != RenderCommandType::DRAW_MESH ||
                    nextCommand.drawMesh.wireframe ||
                    nextCommand.drawMesh.material != firstDraw.material ||
                    nextCommand.drawMesh.mesh != firstDraw.mesh ||
                    nextCommand.drawMesh.submesh != firstDraw.submesh) {
                    break;
                }
                instanceMatrices.push_back(nextCommand.drawMesh.modelMatrix);
                ++batchCount;
            }

            // 执行合批绘制
            if (batchCount > 1) {
                renderer.executeBatchedDraw(firstDraw, instanceMatrices);
                batchingStats.batchedDrawCalls++;
                if (batchingStats.frameCount % 60 == 0) {
                    std::cout << "动态合批: 将 " << batchCount << " 个绘制调用合并为 1 个" << std::endl;
                }
            } else {
                renderer.executeDrawMesh(firstDraw);
            }
            batchingStats.totalDrawCalls++;
            i += batchCount;
        }
    }

    batchingStats.frameCount++;
    if (batchingStats.frameCount % 60 == 0) {
        std::cout << "绘制统计: 总调用 " << batchingStats.totalDrawCalls
                  << ", 合批后 " << batchingStats.batchedDrawCalls
                  << ", 节省 " << (batchingStats.totalDrawCalls - batchingStats.batchedDrawCalls)
                  << " 个调用" << std::endl;
        batchingStats.totalDrawCalls = 0;
        batchingStats.batchedDrawCalls = 0;
    }
}

void RenderPipeline::submitRenderCommands(entt::registry& registry) {
    clearRenderQueue();

    glm::mat4 viewMatrix = glm::mat4(1.0f);
    auto cameraView = registry.view<Transform, CameraComponent>();
    for (auto entity : cameraView) {
        auto& transform = cameraView.get<Transform>(entity);
        auto& camera = cameraView.get<CameraComponent>(entity);
        viewMatrix = glm::lookAt(transform.position, camera.target, glm::vec3(0, 1, 0));
        break;
    }

    submitGlobalUniforms(registry, viewMatrix);
    submitMeshes(registry, viewMatrix);
    submitInstancedMeshes(registry, viewMatrix);
}

void RenderPipeline::submitGlobalUniforms(entt::registry& registry, const glm::mat4& viewMatrix) {
    glm::mat4 projection = glm::perspective(glm::radians(45.0f),
                                            float(renderer.getWindowWidth()) / renderer.getWindowHeight(),
                                            0.1f, 100.0f);
    glm::vec3 viewPos = glm::inverse(viewMatrix)[3];

    glm::mat4 viewProjection = projection * viewMatrix;

    addRenderCommand(RenderCommand::SetUniformMat4("uViewProjection", viewProjection));
    addRenderCommand(RenderCommand::SetUniformVec3("uViewPos", viewPos));
    addRenderCommand(RenderCommand::SetUniformVec3("uLightDir", glm::vec3(-0.5f, -1.0f, -0.5f)));
}

// 通过任何一个子实体，向上遍历父子关系，找到模型的总根
entt::entity RenderPipeline::findModelRoot(entt::registry& registry, entt::entity start_entity) {
    entt::entity current = start_entity;
    // 循环向上查找，最多查找10层以防死循环
    for(int i = 0; i < 10 && registry.valid(current); ++i) {
        // 如果当前实体就是根，直接返回
        if (registry.all_of<ModelRootTag>(current)) {
            return current;
        }
        // 否则，移动到父实体
        auto* parent_comp = registry.try_get<ParentEntity>(current);
        if (parent_comp && registry.valid(parent_comp->parent)) {
            current = parent_comp->parent;
        } else {
            // 没有父实体了，查找结束
            break;
        }
    }
    return entt::null; // 表示没找到
}

void RenderPipeline::submitMeshes(entt::registry& registry, const glm::mat4& viewMatrix) {
    auto meshView = registry.view<Transform, LocalTransform, MeshComponent, RenderStateComponent>();

    for (auto entity : meshView) {
        auto& localTransform = meshView.get<LocalTransform>(entity);
        auto& meshComp = meshView.get<MeshComponent>(entity);
        auto& renderState = meshView.get<RenderStateComponent>(entity);

        if (registry.all_of<InstanceSourceComponent>(entity) || !renderState.visible) {
            continue;
        }

        auto& asset = renderer.getAsset();
        auto meshIt = asset.meshes.find(meshComp.handle);
        if (meshIt == asset.meshes.end() || !meshIt->second.HasGPUData()) {
            continue;
        }

        const auto& mesh = meshIt->second;

        // 默认情况下，模型矩阵就是实体自身的世界矩阵
        glm::mat4 modelMatrix = localTransform.matrix;
        bool isSkinned = mesh.skeleton.has_value();

        if (isSkinned) {
            // 这是一个蒙皮网格,必须找到这个实体所属的那个模型的总根
            entt::entity root = findModelRoot(registry, entity);
            if (registry.valid(root)) {
                // ...并用总根的世界矩阵来作为 uModel
                if (auto* rootLocalTransform = registry.try_get<LocalTransform>(root)) {
                    modelMatrix = rootLocalTransform->matrix;
                }

                // 设置骨骼偏移uniform（新的GPU纹理管线）
                auto* skeleton = registry.try_get<SkeletonComponent>(root);
                if (skeleton) {
                    auto& boneManager = BoneTextureManager::getInstance();
                    int boneOffset = boneManager.getSkeletonOffset(skeleton->handle);
                    if (boneOffset >= 0) {
                        // 添加骨骼偏移uniform设置指令
                        addRenderCommand(RenderCommand::SetUniformInt("uBoneOffset", boneOffset));
                    }
                }
            }
        } else {
            // 非蒙皮网格，骨骼偏移设为0
            addRenderCommand(RenderCommand::SetUniformInt("uBoneOffset", 0));
        }

        // 后续的排序键和提交逻辑使用修正后的 modelMatrix
        glm::vec4 viewSpacePos = viewMatrix * modelMatrix * glm::vec4(0.0, 0.0, 0.0, 1.0);
        float depth = viewSpacePos.z;

        for (const auto& submesh : mesh.submeshes) {
            MaterialHandle material = submesh.material;
            RenderCommand cmd = RenderCommand::DrawMesh(&mesh, &submesh, modelMatrix, material, depth, renderState.wireframe);

            // 排序键的计算逻辑
            uint64_t layer = static_cast<uint64_t>(renderState.renderLayer) << 56;
            bool isTransparent = false;
            auto matIt = asset.materials.find(material);
            if (matIt != asset.materials.end() && matIt->second.alpha_mode == MaterialData::MODE_BLEND) {
                isTransparent = true;
            }
            uint64_t transparentFlag = static_cast<uint64_t>(isTransparent) << 55;
            uint32_t depth_u32;
            if (isTransparent) {
                depth_u32 = static_cast<uint32_t>(glm::max(0.0f, -depth) * 100.0f);
            } else {
                depth_u32 = static_cast<uint32_t>(glm::max(0.0f, depth) * 100.0f);
            }
            uint64_t depthBits = static_cast<uint64_t>(depth_u32) << 24;
            uint64_t materialID = static_cast<uint64_t>(material.id) & 0xFFFFFF;

            cmd.sortKey = layer | transparentFlag | depthBits | materialID;
            addRenderCommand(cmd);
        }
    }
}

void RenderPipeline::submitInstancedMeshes(entt::registry& registry, const glm::mat4& viewMatrix) {
    auto& asset = renderer.getAsset();
    auto instancedView = registry.view<InstancedMeshComponent>();
    for (auto entity : instancedView) {
        auto& instancedMesh = instancedView.get<InstancedMeshComponent>(entity);
        if (instancedMesh.getInstanceCount() == 0) continue;
        auto meshIt = asset.meshes.find(instancedMesh.handle);
        if (meshIt == asset.meshes.end() || !meshIt->second.HasGPUData()) continue;

        const auto& mesh = meshIt->second;
        for (const auto& submesh : mesh.submeshes) {
            RenderCommand cmd = RenderCommand::DrawInstancedMesh(&mesh, &submesh,
                                                                 &instancedMesh.instanceMatrices, submesh.material);
            cmd.sortKey = (static_cast<uint64_t>(1) << 56) | (static_cast<uint64_t>(submesh.material.id) & 0xFFFFFF);
            addRenderCommand(cmd);
        }
    }
}
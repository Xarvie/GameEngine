////////////////////////////////////////////////////////////////////////////////
//
// fbx_tools.h
//
// Ozz-Animation Modern Asset Exporter
//
// Copyright (c) 2024
//
// This tool is designed to extend the ozz-animation pipeline by exporting
// meshes with multi-material and multi-UV support, suitable for a modern
// rendering pipeline.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef OZZ_SAMPLES_FRAMEWORK_FBX_TOOLS_H_
#define OZZ_SAMPLES_FRAMEWORK_FBX_TOOLS_H_

#include <fbxsdk.h>
#include <map>
#include <string>
#include <vector>

#include "ozz/animation/offline/raw_animation.h"
#include "ozz/animation/offline/raw_skeleton.h"
#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/base/containers/string.h"
#include "ozz/base/containers/vector.h"
#include "ozz/base/maths/vec_float.h"
#include "ozz/base/memory/unique_ptr.h"
#include "mesh.h"
#include "render_material.h"

// Forward-declarations
namespace ozz {
    namespace io {
        class OArchive;
    }
}  // namespace ozz

class FbxTools {
public:
    // Defines the command-line options for the exporter.
    struct Options {
        ozz::string fbx_filename;
        ozz::string skeleton_out;
        ozz::string meshes_out;
        ozz::string materials_out;
        ozz::string animation_out_pattern;  // e.g., "anims/character_{}.ozz"
        int max_influences = 4;
        bool verbose = false;

        // Skeleton import options
        ozz::string skeleton_root_name;
        bool import_skeletons = true;
    };

    FbxTools();
    ~FbxTools();

    // The main entry point for the exporter. Executes the entire pipeline based
    // on the provided options.
    bool Run(const Options& _options);

private:
    // Disallow copy and assignment.
    FbxTools(const FbxTools&);
    void operator=(const FbxTools&);

    //
    // Pipeline stages
    //

    // Stage 1: Initialization
    bool InitializeSdk();
    bool LoadScene(const char* _filename);

    // Stage 2: Asset Extraction
    bool ProcessScene();
    bool BuildMaterialSet();
    bool ExportSkeleton();
    bool ExportMeshes();
    bool ExportAnimations();

    // Stage 3: Serialization
    bool SaveMaterialSet() const;
    bool SaveMeshes() const;
    bool SaveSkeleton(const ozz::animation::Skeleton& _skeleton) const;
    bool SaveAnimation(const ozz::animation::Animation& _animation,
                       const ozz::string& _path) const;

    //
    // Internal data structures for processing
    //

    // Represents a unique vertex during the mesh processing phase.
    // An instance of this struct is used as a key for vertex welding/deduplication.
    struct Vertex {
        ozz::math::Float3 position;
        ozz::math::Float3 normal;
        ozz::math::Float3 tangent;
        ozz::math::Float2 uvs[2];
        ozz::math::Float4 color;
        uint32_t material_id;

        // Skinning data.
        struct Influence {
            int16_t joint_index;
            float weight;

            bool operator<(const Influence& _other) const {
                // Sort by weight (descending), then by index.
                if (weight != _other.weight) return weight > _other.weight;
                return joint_index < _other.joint_index;
            }
        };
        ozz::vector<Influence> influences;

        // Comparison operator for std::map key.
        bool operator<(const Vertex& _other) const;
    };

    // Maps a unique Vertex definition to its final index in the vertex buffer.
    typedef std::map<Vertex, uint16_t> VertexMap;

    //
    // Helper functions
    //

    // Extracts PBR properties and textures from an FbxSurfaceMaterial.
    void ExtractMaterialProperties(FbxSurfaceMaterial* _fbx_material,
                                   ozz::sample::RenderMaterial* _out_material);

    // Gets a texture's file path from an FbxProperty.
    ozz::string GetTexturePath(const FbxProperty& _property);

    // Traverses the FBX node hierarchy to find skeleton roots.
    void CollectSkeletonRoots(FbxNode* _node,
                              ozz::vector<FbxNode*>* _roots) const;

    // Recursively builds a RawSkeleton from an FBX node hierarchy.
    bool BuildRawSkeleton(FbxNode* _root,
                          ozz::animation::offline::RawSkeleton* _skeleton) const;

    // Processes a single FbxMesh node and converts it to ozz::sample::Mesh.
    bool ProcessMesh(FbxMesh* _fbx_mesh, FbxNode* _fbx_node);

    // Helper for ProcessMesh: gets a single element from an FbxLayerElementTemplate.
    template <typename T, typename U>
    bool GetLayerElement(const FbxLayerElementTemplate<T>* _layer, int _polygon_index,
                         int _polygon_vertex_index, int _control_point_index,
                         U* _out) const;

    // Helper for ProcessMesh: collects all influences for each control point.
    bool CollectInfluences(
            FbxMesh* _fbx_mesh,
            ozz::vector<ozz::vector<Vertex::Influence>>* _out_influences) const;

    // Helper for ProcessMesh: splits mesh parts based on number of influences.
    void SplitMeshParts(ozz::sample::Mesh* _mesh, const VertexMap& _vertex_map, const ozz::vector<uint16_t>& _indices) const;

    // Helper for ExportAnimations: samples a single animation curve.
    // FIXED: Overloaded for each track type.
    void SampleCurve(FbxAnimCurve* _curve, ozz::animation::offline::RawAnimation::JointTrack::Translations& _keys) const;
    void SampleCurve(FbxAnimCurve* _curve, ozz::animation::offline::RawAnimation::JointTrack::Rotations& _keys) const;
    void SampleCurve(FbxAnimCurve* _curve, ozz::animation::offline::RawAnimation::JointTrack::Scales& _keys) const;


    //
    // Member variables
    //

    // FBX SDK objects
    FbxManager* fbx_manager_ = nullptr;
    FbxScene* fbx_scene_ = nullptr;

    // Exporter options
    Options options_;

    // Extracted asset data
    ozz::sample::MaterialSet material_set_;
    ozz::vector<ozz::sample::Mesh> meshes_;
    ozz::unique_ptr<ozz::animation::Skeleton> skeleton_; // FIXED: Use unique_ptr as Skeleton is non-copyable.

    // Internal mapping tables for processing
    std::map<FbxSurfaceMaterial*, int> fbx_material_to_index_map_;
    std::map<const FbxNode*, int> fbx_node_to_joint_map_;
};

#endif  // OZZ_SAMPLES_FRAMEWORK_FBX_TOOLS_H_

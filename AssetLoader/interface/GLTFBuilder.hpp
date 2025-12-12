/*
 *  Copyright 2019-2025 Diligent Graphics LLC
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  In no event and under no legal theory, whether in tort (including negligence),
 *  contract, or otherwise, unless required by applicable law (such as deliberate
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental,
 *  or consequential damages of any character arising as a result of this License or
 *  out of the use or inability to use the software (including but not limited to damages
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and
 *  all other commercial damages or losses), even if such Contributor has been advised
 *  of the possibility of such damages.
 */

#pragma once

#include "GLTFBuilder.hpp"
#include "GLTFLoader.hpp"
#include "GraphicsAccessories.hpp"
#include "gltf_utils.hpp"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Diligent
{

namespace GLTF
{

struct Model;
struct ModelCreateInfo;

// {0BF00221-593F-40CE-B5BD-E47039D77F4A}
static constexpr INTERFACE_ID IID_BufferInitData = {0xbf00221, 0x593f, 0x40ce, {0xb5, 0xbd, 0xe4, 0x70, 0x39, 0xd7, 0x7f, 0x4a}};

struct BufferInitData : ObjectBase<IObject>
{
    BufferInitData(IReferenceCounters* pRefCounters) :
        ObjectBase{pRefCounters}
    {
    }

    static RefCntAutoPtr<BufferInitData> Create()
    {
        return RefCntAutoPtr<BufferInitData>{MakeNewRCObj<BufferInitData>()()};
    }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_BufferInitData, ObjectBase)

    std::vector<std::vector<Uint8>> Data;
};

class ModelBuilder
{
public:
    std::unique_ptr<TinyGltfModelWrapper> m_gltf_model = nullptr;

public:
    ModelBuilder(const ModelCreateInfo& _CI, Model& _Model);
    ~ModelBuilder();

    void Execute(tinygltf::Model* tiny_gltf_model, int SceneIndex, IRenderDevice* pDevice);

    static std::pair<FILTER_TYPE, FILTER_TYPE> GetFilterType(int32_t GltfFilterMode);

    static TEXTURE_ADDRESS_MODE GetAddressMode(int32_t GltfWrapMode);

private:
    struct PrimitiveKey
    {
        std::vector<int> AccessorIds;
        mutable size_t   Hash = 0;

        bool operator==(const PrimitiveKey& Rhs) const noexcept
        {
            return AccessorIds == Rhs.AccessorIds;
        }

        struct Hasher
        {
            size_t operator()(const PrimitiveKey& Key) const noexcept;
        };
    };

    // If SceneIndex >= 0, loads only the specified scene, otherwise loads all scenes.
    // Stores the GLTF node indices as the node pointers.
    void LoadScenes(int SceneIndex);
    // Recursively allocates nodes as well as meshes and cameras.
    void AllocateNode(int GltfNodeIndex);
    // Recursively loads nodes.
    spw::Node*   LoadNode(spw::Node* Parent, spw::Scene& scene, int GltfNodeIndex);
    spw::Mesh*   LoadMesh(int GltfMeshIndex);
    spw::Camera* LoadCamera(int GltfCameraIndex);
    spw::Light*  LoadLight(int GltfLightIndex);
    bool         LoadAnimationAndSkin();

    void InitIndexBuffer(IRenderDevice* pDevice);
    void InitVertexBuffers(IRenderDevice* pDevice);


    struct WriteGltfDataAttribs
    {
        const void*                  pSrc;
        VALUE_TYPE                   SrcType;
        Uint32                       NumSrcComponents;
        Uint32                       SrcElemStride;
        std::vector<Uint8>::iterator dst_it;
        VALUE_TYPE                   DstType;
        Uint32                       NumDstComponents;
        Uint32                       DstElementStride;
        Uint32                       NumElements;
        bool                         IsNormalized;
    };
    static void WriteGltfData(const WriteGltfDataAttribs& Attribs);

    static void WriteDefaultAttibuteValue(const void*                  pDefaultValue,
                                          std::vector<Uint8>::iterator dst_it,
                                          VALUE_TYPE                   DstType,
                                          Uint32                       NumDstComponents,
                                          Uint32                       DstElementStride,
                                          Uint32                       NumElements);

    void WriteDefaultAttibutes(Uint32 BufferId, size_t StartOffset, size_t EndOffset);

    Uint32 ConvertVertexData(const PrimitiveKey& Key,
                             Uint32              VertexCount);

    template <typename SrcType, typename DstType>
    inline static void WriteIndexData(const void*                  pSrc,
                                      size_t                       SrcStride,
                                      std::vector<Uint8>::iterator dst_it,
                                      Uint32                       NumElements,
                                      Uint32                       BaseVertex);

    Uint32 ConvertIndexData(int AccessorId, Uint32 BaseVertex);

    void LoadSkins();

    void LoadAnimations();

    auto GetGltfDataInfo(int AccessorId) const;

    // Returns the node pointer from the node index in the source GLTF model.
    spw::Node* NodeFromGltfIndex(int GltfIndex) const
    {
        auto it = m_NodeIndexRemapping.find(GltfIndex);
        return it != m_NodeIndexRemapping.end() ? &m_Model.Nodes[it->second] : nullptr;
    }

    template <typename GltfDataInfoType>
    bool ComputePrimitiveBoundingBox(const GltfDataInfoType& PosData, float3& Min, float3& Max) const;

private:
    const ModelCreateInfo& m_CI;
    Model&                 m_Model;

    // In a GLTF file, all objects are referenced by global index.
    // A model that is loaded may not contain all original objects though,
    // so we need to keep a mapping from the original index to the loaded
    // index.
    std::unordered_map<int, int> m_NodeIndexRemapping;
    std::unordered_map<int, int> m_MeshIndexRemapping;
    std::unordered_map<int, int> m_CameraIndexRemapping;
    std::unordered_map<int, int> m_LightIndexRemapping;

    std::unordered_set<int> m_LoadedNodes;
    std::unordered_set<int> m_LoadedMeshes;
    std::unordered_set<int> m_LoadedCameras;
    std::unordered_set<int> m_LoadedLights;

    std::unordered_map<int, int> m_NodeIdToSkinId;

    std::vector<Uint8>              m_IndexData;
    std::vector<std::vector<Uint8>> m_VertexData;

    std::unordered_map<PrimitiveKey, Uint32, PrimitiveKey::Hasher> m_PrimitiveOffsets;

    int m_DefaultMaterialId = -1;
};

template <typename GltfDataInfoType>
bool ModelBuilder::ComputePrimitiveBoundingBox(const GltfDataInfoType& PosData, float3& Min, float3& Max) const
{
    if (PosData.Accessor.GetComponentType() != VT_FLOAT32)
    {
        DEV_ERROR("Unexpected GLTF vertex position component type: ", GetValueTypeString(PosData.Accessor.GetComponentType()), ". float is expected.");
        return false;
    }
    if (PosData.Accessor.GetNumComponents() != 3)
    {
        DEV_ERROR("Unexpected GLTF vertex position component count: ", PosData.Accessor.GetNumComponents(), ". 3 is expected.");
        return false;
    }

    Max = float3{-FLT_MAX};
    Min = float3{+FLT_MAX};
    for (size_t i = 0; i < PosData.Count; ++i)
    {
        const auto& Pos{*reinterpret_cast<const float3*>(static_cast<const Uint8*>(PosData.pData) + PosData.ByteStride * i)};
        Max = max(Max, Pos);
        Min = min(Min, Pos);
    }
    return true;
}

class MaterialBuilder
{
public:
    MaterialBuilder(Material& Mat) noexcept :
        m_Material{Mat}
    {
        auto MaxActiveTexAttribIdx = Mat.GetMaxActiveTextureAttribIdx();
        if (MaxActiveTexAttribIdx != Material::InvalidTextureAttribIdx)
        {
            m_TextureAttribs.reserve(MaxActiveTexAttribIdx + 1);
            m_TextureIds.reserve(MaxActiveTexAttribIdx + 1);
            Mat.ProcessActiveTextureAttibs(
                [&](Uint32 Idx, const Material::TextureShaderAttribs& TexAttribs, int TextureId) //
                {
                    GetTextureAttrib(Idx) = TexAttribs;
                    SetTextureId(Idx, TextureId);
                    return true;
                });
        }
    }

    void SetTextureId(Uint32 Idx, int TextureId)
    {
        EnsureTextureAttribCount(Idx + 1);
        m_TextureIds[Idx] = TextureId;
    }

    Material::TextureShaderAttribs& GetTextureAttrib(Uint32 Idx)
    {
        EnsureTextureAttribCount(Idx + 1);
        return m_TextureAttribs[Idx];
    }

    void Finalize() const
    {
        m_Material.ActiveTextureAttribs |= m_ForcedActiveTextureAttribs;

        VERIFY_EXPR(m_TextureAttribs.size() == m_TextureIds.size());

        Uint32 NumActiveTextureAttribs = 0;
        for (Uint32 i = 0; i < m_TextureAttribs.size(); ++i)
        {
            static const Material::TextureShaderAttribs DefaultAttribs{};
            if (m_TextureIds[i] != -1 || memcmp(&m_TextureAttribs[i], &DefaultAttribs, sizeof(DefaultAttribs)) != 0)
            {
                m_Material.ActiveTextureAttribs |= (1u << i);
            }
            if (m_Material.IsTextureAttribActive(i))
                ++NumActiveTextureAttribs;
        }

        VERIFY_EXPR(NumActiveTextureAttribs == m_Material.GetNumActiveTextureAttribs());

        if (NumActiveTextureAttribs > 0)
        {
            m_Material.TextureAttribs = std::make_unique<Material::TextureShaderAttribs[]>(NumActiveTextureAttribs);
            m_Material.TextureIds     = std::make_unique<int[]>(NumActiveTextureAttribs);
            m_Material.ProcessActiveTextureAttibs(
                [&](Uint32 Idx, Material::TextureShaderAttribs& TexAttribs, int& TextureId) //
                {
                    TexAttribs = m_TextureAttribs[Idx];
                    TextureId  = m_TextureIds[Idx];
                    return true;
                });
        }
    }

    static void EnsureTextureAttribActive(Material& Mat, Uint32 Idx)
    {
        if (Mat.IsTextureAttribActive(Idx))
            return;

        MaterialBuilder Builder{Mat};
        Builder.EnsureTextureAttribCount(Idx + 1);
        Builder.m_ForcedActiveTextureAttribs |= (1u << Idx);
        Builder.Finalize();
    }

    Material::ShaderAttribs& GetShaderAttribs() { return m_Material.Attribs; }

private:
    void EnsureTextureAttribCount(size_t Count)
    {
        VERIFY_EXPR(m_TextureAttribs.size() == m_TextureIds.size());
        if (m_TextureAttribs.size() < Count)
            m_TextureAttribs.resize(Count);
        if (m_TextureIds.size() < Count)
            m_TextureIds.resize(Count, -1);
    }

private:
    Material& m_Material;

    decltype(Material::ActiveTextureAttribs) m_ForcedActiveTextureAttribs = 0;

    std::vector<int>                            m_TextureIds;
    std::vector<Material::TextureShaderAttribs> m_TextureAttribs;
};

} // namespace GLTF

} // namespace Diligent

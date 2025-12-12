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

#include "GLTFBuilder.hpp"
#include "GLTFLoader.hpp"
#include "GraphicsAccessories.hpp"
#include "gltf_utils.hpp"

namespace Diligent
{
namespace GLTF
{

auto ModelBuilder::GetGltfDataInfo(int AccessorId) const
{
    const auto  GltfAccessor  = m_gltf_model->GetAccessor(AccessorId);
    const auto  GltfView      = m_gltf_model->GetBufferView(GltfAccessor.GetBufferViewId());
    const auto  GltfBuffer    = m_gltf_model->GetBuffer(GltfView.GetBufferId());
    const auto  SrcCount      = GltfAccessor.GetCount();
    const auto  SrcByteStride = GltfAccessor.GetByteStride(GltfView);
    const auto* pSrcData      = SrcCount > 0 ?
             GltfBuffer.GetData(GltfAccessor.GetByteOffset() + GltfView.GetByteOffset()) :
             nullptr;

    struct GltfDataInfo
    {
        decltype(GltfAccessor) Accessor;

        const void* const             pData;
        const decltype(SrcCount)      Count;
        const decltype(SrcByteStride) ByteStride;
    };

    return GltfDataInfo{GltfAccessor, pSrcData, SrcCount, SrcByteStride};
}

ModelBuilder::ModelBuilder(const ModelCreateInfo& _CI, Model& _Model) :
    m_CI{_CI},
    m_Model{_Model}
{
    VERIFY_EXPR(!m_Model.VertexData.Strides.empty());
    m_VertexData.resize(m_Model.VertexData.Strides.size());
}

ModelBuilder::~ModelBuilder()
{
}

size_t ModelBuilder::PrimitiveKey::Hasher::operator()(const PrimitiveKey& Key) const noexcept
{
    if (Key.Hash == 0)
    {
        Key.Hash = ComputeHash(Key.AccessorIds.size());
        for (int Id : Key.AccessorIds)
            HashCombine(Key.Hash, Id);
    }
    return Key.Hash;
}

template <typename DstType, bool Normalize, typename SrcType>
inline DstType ConvertElement(SrcType Src)
{
    return static_cast<DstType>(Src);
}

// =========================== float -> Int8/Uint8 ============================
template <>
inline Uint8 ConvertElement<Uint8, true, float>(float Src)
{
    return static_cast<Uint8>(clamp(Src * 255.f + 0.5f, 0.f, 255.f));
}

template <>
inline Uint8 ConvertElement<Uint8, false, float>(float Src)
{
    return ConvertElement<Uint8, true>(Src);
}

template <>
inline Int8 ConvertElement<Int8, true, float>(float Src)
{
    float r = Src > 0.f ? +0.5f : -0.5f;
    return static_cast<Int8>(clamp(Src * 127.f + r, -127.f, 127.f));
}

template <>
inline Int8 ConvertElement<Int8, false, float>(float Src)
{
    return ConvertElement<Int8, true>(Src);
}


// =========================== Int8/Uint8 -> float ============================
template <>
inline float ConvertElement<float, true, Int8>(Int8 Src)
{
    return std::max(static_cast<float>(Src), -127.f) / 127.f;
}

template <>
inline float ConvertElement<float, true, Uint8>(Uint8 Src)
{
    return static_cast<float>(Src) / 255.f;
}


// ========================== Int16/Uint16 -> float ===========================
template <>
inline float ConvertElement<float, true, Int16>(Int16 Src)
{
    return std::max(static_cast<float>(Src), -32767.f) / 32767.f;
}

template <>
inline float ConvertElement<float, true, Uint16>(Uint16 Src)
{
    return static_cast<float>(Src) / 65535.f;
}


template <typename SrcType, typename DstType, bool IsNormalized>
inline void WriteGltfData(const void*                  pSrc,
                          Uint32                       NumComponents,
                          Uint32                       SrcElemStride,
                          std::vector<Uint8>::iterator dst_it,
                          Uint32                       DstElementStride,
                          Uint32                       NumElements)
{
    for (size_t elem = 0; elem < NumElements; ++elem)
    {
        const SrcType* pSrcCmp = reinterpret_cast<const SrcType*>(static_cast<const Uint8*>(pSrc) + SrcElemStride * elem);

        auto comp_it = dst_it + DstElementStride * elem;
        for (Uint32 cmp = 0; cmp < NumComponents; ++cmp, comp_it += sizeof(DstType))
        {
            reinterpret_cast<DstType&>(*comp_it) = ConvertElement<DstType, IsNormalized>(pSrcCmp[cmp]);
        }
    }
}

void ModelBuilder::WriteGltfData(const WriteGltfDataAttribs& Attribs)
{
    const Uint32 NumComponentsToCopy = std::min(Attribs.NumSrcComponents, Attribs.NumDstComponents);

#define INNER_CASE(SrcType, DstType)                                            \
    case DstType:                                                               \
        if (Attribs.IsNormalized)                                               \
        {                                                                       \
            GLTF::WriteGltfData<typename VALUE_TYPE2CType<SrcType>::CType,      \
                                typename VALUE_TYPE2CType<DstType>::CType,      \
                                true>(                                          \
                Attribs.pSrc, NumComponentsToCopy, Attribs.SrcElemStride,       \
                Attribs.dst_it, Attribs.DstElementStride, Attribs.NumElements); \
        }                                                                       \
        else                                                                    \
        {                                                                       \
            GLTF::WriteGltfData<typename VALUE_TYPE2CType<SrcType>::CType,      \
                                typename VALUE_TYPE2CType<DstType>::CType,      \
                                false>(                                         \
                Attribs.pSrc, NumComponentsToCopy, Attribs.SrcElemStride,       \
                Attribs.dst_it, Attribs.DstElementStride, Attribs.NumElements); \
        }                                                                       \
        break

#define CASE(SrcType)                                      \
    case SrcType:                                          \
        switch (Attribs.DstType)                           \
        {                                                  \
            INNER_CASE(SrcType, VT_INT8);                  \
            INNER_CASE(SrcType, VT_INT16);                 \
            INNER_CASE(SrcType, VT_INT32);                 \
            INNER_CASE(SrcType, VT_UINT8);                 \
            INNER_CASE(SrcType, VT_UINT16);                \
            INNER_CASE(SrcType, VT_UINT32);                \
            /*INNER_CASE(SrcType, VT_FLOAT16);*/           \
            INNER_CASE(SrcType, VT_FLOAT32);               \
            /*INNER_CASE(SrcType, VT_FLOAT64);*/           \
            default:                                       \
                UNEXPECTED("Unexpected destination type"); \
        }                                                  \
        break

    switch (Attribs.SrcType)
    {
        CASE(VT_INT8);
        CASE(VT_INT16);
        CASE(VT_INT32);
        CASE(VT_UINT8);
        CASE(VT_UINT16);
        CASE(VT_UINT32);
        //CASE(VT_FLOAT16);
        CASE(VT_FLOAT32);
        //CASE(VT_FLOAT64);
        default:
            UNEXPECTED("Unexpected source type");
    }
#undef CASE
#undef INNER_CASE
}

void ModelBuilder::WriteDefaultAttibuteValue(const void*                  pDefaultValue,
                                             std::vector<Uint8>::iterator dst_it,
                                             VALUE_TYPE                   DstType,
                                             Uint32                       NumDstComponents,
                                             Uint32                       DstElementStride,
                                             Uint32                       NumElements)
{
    Uint32 ElementSize = GetValueSize(DstType) * NumDstComponents;
    VERIFY(DstElementStride >= ElementSize, "Destination element stride is too small");
    for (size_t elem = 0; elem < NumElements; ++elem)
    {
        //  Note: MSVC asserts when moving iterator past the end of the vector
        memcpy(&*(dst_it + DstElementStride * elem), pDefaultValue, ElementSize);
    }
}

void ModelBuilder::WriteDefaultAttibutes(Uint32 BufferId, size_t StartOffset, size_t EndOffset)
{
    const Uint32 VertexStride = m_Model.VertexData.Strides[BufferId];
    VERIFY(StartOffset % VertexStride == 0, "Start offset is not aligned to vertex stride");
    VERIFY(EndOffset % VertexStride == 0, "End offset is not aligned to vertex stride");
    const Uint32 NumVertices = static_cast<Uint32>((EndOffset - StartOffset) / VertexStride);
    for (Uint32 i = 0; i < m_Model.GetNumVertexAttributes(); ++i)
    {
        const VertexAttributeDesc& Attrib = m_Model.VertexAttributes[i];
        if (BufferId != Attrib.BufferId || Attrib.pDefaultValue == nullptr)
            continue;

        WriteDefaultAttibuteValue(Attrib.pDefaultValue,
                                  m_VertexData[BufferId].begin() + StartOffset + Attrib.RelativeOffset,
                                  Attrib.ValueType,
                                  Attrib.NumComponents,
                                  VertexStride,
                                  NumVertices);
    }
}

void ModelBuilder::InitIndexBuffer(IRenderDevice* pDevice)
{
    if (m_IndexData.empty())
        return;

    VERIFY_EXPR(m_Model.IndexData.IndexSize > 0);
    VERIFY_EXPR((m_IndexData.size() % m_Model.IndexData.IndexSize) == 0);
    VERIFY(!m_Model.IndexData.pBuffer && !m_Model.IndexData.pAllocation, "Index buffer has already been initialized");

    const Uint32 DataSize = static_cast<Uint32>(m_IndexData.size());
    if (m_CI.pResourceManager != nullptr)
    {
        m_Model.IndexData.pAllocation = m_CI.pResourceManager->AllocateIndices(DataSize, 4);

        if (m_Model.IndexData.pAllocation)
        {
            RefCntAutoPtr<BufferInitData> pBuffInitData = BufferInitData::Create();
            pBuffInitData->Data.emplace_back(std::move(m_IndexData));
            m_Model.IndexData.pAllocation->SetUserData(pBuffInitData);

            m_Model.IndexData.AllocatorId = m_CI.pResourceManager->GetIndexAllocatorIndex(m_Model.IndexData.pAllocation->GetAllocator());
            VERIFY_EXPR(m_Model.IndexData.AllocatorId != ~0u);
        }
        else
        {
            UNEXPECTED("Failed to allocate indices from the pool.");
        }
    }
    else
    {
        const BIND_FLAGS BindFlags = m_CI.IndBufferBindFlags != BIND_NONE ? m_CI.IndBufferBindFlags : BIND_INDEX_BUFFER;
        BufferDesc       BuffDesc{"GLTF index buffer", DataSize, BindFlags, USAGE_IMMUTABLE};
        if (BuffDesc.BindFlags & (BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS))
        {
            BuffDesc.Mode              = BUFFER_MODE_FORMATTED;
            BuffDesc.ElementByteStride = m_Model.IndexData.IndexSize;
        }

        BufferData BuffData{m_IndexData.data(), BuffDesc.Size};
        pDevice->CreateBuffer(BuffDesc, &BuffData, &m_Model.IndexData.pBuffer);
    }
}

void ModelBuilder::InitVertexBuffers(IRenderDevice* pDevice)
{
    if (m_VertexData.empty())
    {
        UNEXPECTED("Vertex data can't be empty.");
        return;
    }

    const size_t VBCount = m_Model.GetVertexBufferCount();
    VERIFY_EXPR(m_VertexData.size() == VBCount);

    size_t NumVertices = 0;
    for (Uint32 i = 0; i < VBCount; ++i)
    {
        if (!m_VertexData[i].empty())
        {
            NumVertices = m_VertexData[i].size() / m_Model.VertexData.Strides[i];
            break;
        }
    }
#ifdef DILIGENT_DEBUG
    for (Uint32 i = 0; i < VBCount; ++i)
    {
        VERIFY(m_VertexData[i].empty() || NumVertices == m_VertexData[i].size() / m_Model.VertexData.Strides[i], "Inconsistent number of vertices in different buffers.");
    }
#endif

    if (NumVertices == 0)
    {
        m_Model.VertexData.Buffers.resize(VBCount);
        return;
    }

    if (m_CI.pResourceManager != nullptr)
    {
        ResourceManager::VertexLayoutKey LayoutKey;
        LayoutKey.Elements.reserve(VBCount);
        for (Uint32 i = 0; i < VBCount; ++i)
        {
            const BIND_FLAGS BindFlags = m_CI.VertBufferBindFlags[i] != BIND_NONE ? m_CI.VertBufferBindFlags[i] : BIND_VERTEX_BUFFER;
            LayoutKey.Elements.emplace_back(m_Model.VertexData.Strides[i], BindFlags);
        }

        VERIFY(!m_Model.VertexData.pAllocation, "This vertex buffer has already been initialized");
        m_Model.VertexData.pAllocation = m_CI.pResourceManager->AllocateVertices(LayoutKey, static_cast<Uint32>(NumVertices));
        if (m_Model.VertexData.pAllocation)
        {
            RefCntAutoPtr<BufferInitData> pBuffInitData = BufferInitData::Create();
            pBuffInitData->Data                         = std::move(m_VertexData);
            m_Model.VertexData.pAllocation->SetUserData(pBuffInitData);
            m_Model.VertexData.PoolId = m_CI.pResourceManager->GetVertexPoolIndex(LayoutKey, m_Model.VertexData.pAllocation->GetPool());
            VERIFY_EXPR(m_Model.VertexData.PoolId != ~0u);
        }
        else
        {
            UNEXPECTED("Failed to allocate vertices from the pool. Make sure that you proived the required layout when creating the pool.");
        }
    }
    else
    {
        VERIFY(m_Model.VertexData.Buffers.empty(), "Vertex buffers have already been initialized");
        m_Model.VertexData.Buffers.resize(VBCount);
        for (Uint32 i = 0; i < VBCount; ++i)
        {
            const std::vector<Uint8>& Data = m_VertexData[i];
            if (Data.empty())
                continue;

            const Uint32      DataSize  = static_cast<Uint32>(Data.size());
            const std::string Name      = std::string{"GLTF vertex buffer "} + std::to_string(i);
            const BIND_FLAGS  BindFlags = m_CI.VertBufferBindFlags[i] != BIND_NONE ? m_CI.VertBufferBindFlags[i] : BIND_VERTEX_BUFFER;
            BufferDesc        BuffDesc{Name.c_str(), DataSize, BindFlags, USAGE_IMMUTABLE};

            const Uint32 ElementStride = m_Model.VertexData.Strides[i];
            VERIFY_EXPR(ElementStride > 0);
            VERIFY_EXPR(Data.size() % ElementStride == 0);

            if (BuffDesc.BindFlags & (BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS))
            {
                BuffDesc.Mode              = BUFFER_MODE_STRUCTURED;
                BuffDesc.ElementByteStride = ElementStride;
            }

            VERIFY_EXPR(!m_Model.VertexData.Buffers[i]);
            BufferData BuffData{Data.data(), DataSize};
            pDevice->CreateBuffer(BuffDesc, &BuffData, &m_Model.VertexData.Buffers[i]);
        }
    }
}

std::pair<FILTER_TYPE, FILTER_TYPE> ModelBuilder::GetFilterType(int32_t GltfFilterMode)
{
    switch (GltfFilterMode)
    {
        case 9728: // NEAREST
            return {FILTER_TYPE_POINT, FILTER_TYPE_POINT};
        case 9729: // LINEAR
            return {FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR};
        case 9984: // NEAREST_MIPMAP_NEAREST
            return {FILTER_TYPE_POINT, FILTER_TYPE_POINT};
        case 9985: // LINEAR_MIPMAP_NEAREST
            return {FILTER_TYPE_LINEAR, FILTER_TYPE_POINT};
        case 9986: // NEAREST_MIPMAP_LINEAR
            return {FILTER_TYPE_POINT, FILTER_TYPE_LINEAR};
        case 9987: // LINEAR_MIPMAP_LINEAR
            return {FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR};
        default:
            LOG_WARNING_MESSAGE("Unknown gltf filter mode: ", GltfFilterMode, ". Defaulting to linear.");
            return {FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR};
    }
}

TEXTURE_ADDRESS_MODE ModelBuilder::GetAddressMode(int32_t GltfWrapMode)
{
    switch (GltfWrapMode)
    {
        case 10497:
            return TEXTURE_ADDRESS_WRAP;
        case 33071:
            return TEXTURE_ADDRESS_CLAMP;
        case 33648:
            return TEXTURE_ADDRESS_MIRROR;
        default:
            LOG_WARNING_MESSAGE("Unknown gltf address wrap mode: ", GltfWrapMode, ". Defaulting to WRAP.");
            return TEXTURE_ADDRESS_WRAP;
    }
}

spw::Mesh* ModelBuilder::LoadMesh(int GltfMeshIndex)
{
    if (GltfMeshIndex < 0)
        return nullptr;

    auto mesh_it = m_MeshIndexRemapping.find(GltfMeshIndex);
    VERIFY(mesh_it != m_MeshIndexRemapping.end(), "Mesh with GLTF index ", GltfMeshIndex, " is not present in the map. This appears to be a bug.");
    const auto LoadedMeshId = mesh_it->second;

    auto& NewMesh = m_Model.Meshes[LoadedMeshId];

    if (m_LoadedMeshes.find(LoadedMeshId) != m_LoadedMeshes.end())
    {
        // The mesh has already been loaded as it is referenced by
        // multiple nodes (e.g. '2CylinderEngine' test model).
        return &NewMesh;
    }
    m_LoadedMeshes.emplace(LoadedMeshId);

    const auto& GltfMesh = m_gltf_model->GetMesh(GltfMeshIndex);

    NewMesh.Name = GltfMesh.GetName();

    const size_t PrimitiveCount = GltfMesh.GetPrimitiveCount();
    NewMesh.Primitives.reserve(PrimitiveCount);
    for (size_t prim = 0; prim < PrimitiveCount; ++prim)
    {
        const auto& GltfPrimitive = GltfMesh.GetPrimitive(prim);

        const auto DstIndexSize = m_Model.IndexData.IndexSize;

        uint32_t IndexStart  = static_cast<uint32_t>(m_IndexData.size()) / DstIndexSize;
        uint32_t VertexStart = 0;
        uint32_t IndexCount  = 0;
        uint32_t VertexCount = 0;
        float3   PosMin;
        float3   PosMax;

        // Vertices
        {
            PrimitiveKey Key;

            Key.AccessorIds.resize(m_Model.GetNumVertexAttributes());
            for (Uint32 i = 0; i < m_Model.GetNumVertexAttributes(); ++i)
            {
                const auto& Attrib = m_Model.VertexAttributes[i];
                VERIFY_EXPR(Attrib.Name != nullptr);
                auto* pAttribId    = GltfPrimitive.GetAttribute(Attrib.Name);
                Key.AccessorIds[i] = pAttribId != nullptr ? *pAttribId : -1;
            }

            {
                auto* pPosAttribId = GltfPrimitive.GetAttribute("POSITION");
                VERIFY(pPosAttribId != nullptr, "Position attribute is required");

                const auto& PosAccessor = m_gltf_model->GetAccessor(*pPosAttribId);

                PosMin = PosAccessor.GetMinValues();
                PosMax = PosAccessor.GetMaxValues();
                if (m_CI.ComputeBoundingBoxes)
                {
                    ComputePrimitiveBoundingBox(GetGltfDataInfo(*pPosAttribId), PosMin, PosMax);
                }

                VertexCount = static_cast<uint32_t>(PosAccessor.GetCount());
            }

            auto offset_it = m_PrimitiveOffsets.find(Key);
            if (offset_it == m_PrimitiveOffsets.end())
            {
                auto Offset = ConvertVertexData(Key, VertexCount);
                VERIFY_EXPR(Offset != ~0u);
                offset_it = m_PrimitiveOffsets.emplace(Key, Offset).first;
            }
            VertexStart = offset_it->second;

#ifdef DILIGENT_DEBUG
            for (size_t i = 0; i < m_VertexData.size(); ++i)
            {
                VERIFY(m_Model.VertexData.Strides[i] == 0 || (m_VertexData[i].size() % m_Model.VertexData.Strides[i]) == 0, "Vertex data is misaligned");
            }
#endif
        }

        // Indices
        if (GltfPrimitive.GetIndicesId() >= 0)
        {
            IndexCount = ConvertIndexData(GltfPrimitive.GetIndicesId(), VertexStart);
            // Vertex offset is baked into the indices
            VertexStart = 0;
        }

        int MaterialId = GltfPrimitive.GetMaterialId();
        if (MaterialId < 0)
        {
            if (m_DefaultMaterialId < 0)
            {
                m_DefaultMaterialId = static_cast<int>(m_Model.Materials.size());
                m_Model.Materials.emplace_back();
            }
            MaterialId = m_DefaultMaterialId;
        }

        NewMesh.Primitives.emplace_back(
            IndexStart,
            IndexCount,
            VertexStart,
            VertexCount,
            static_cast<Uint32>(MaterialId),
            PosMin,
            PosMax //
        );

        if (m_CI.PrimitiveLoadCallback)
            m_CI.PrimitiveLoadCallback(&GltfPrimitive.Get(), NewMesh.Primitives.back());
    }

    NewMesh.UpdateBoundingBox();

    if (m_CI.MeshLoadCallback)
        m_CI.MeshLoadCallback(&GltfMesh.Get(), NewMesh);

    return &NewMesh;
}

spw::Camera* ModelBuilder::LoadCamera(int GltfCameraIndex)
{
    if (GltfCameraIndex < 0)
        return nullptr;

    auto camera_it = m_CameraIndexRemapping.find(GltfCameraIndex);
    VERIFY(camera_it != m_CameraIndexRemapping.end(), "Camera with GLTF index ", GltfCameraIndex, " is not present in the map. This appears to be a bug.");
    const auto LoadedCameraId = camera_it->second;

    auto& NewCamera = m_Model.Cameras[LoadedCameraId];

    if (m_LoadedCameras.find(LoadedCameraId) != m_LoadedCameras.end())
    {
        // The camera has already been loaded
        return &NewCamera;
    }
    m_LoadedCameras.emplace(LoadedCameraId);

    const auto& GltfCam = m_gltf_model->GetCamera(GltfCameraIndex);

    NewCamera.Name = GltfCam.GetName();

    if (GltfCam.GetType() == "perspective")
    {
        NewCamera.Type = spw::Camera::Projection::Perspective;

        const auto& PerspectiveCam{GltfCam.GetPerspective()};

        NewCamera.Perspective.AspectRatio = static_cast<float>(PerspectiveCam.GetAspectRatio());
        NewCamera.Perspective.YFov        = static_cast<float>(PerspectiveCam.GetYFov());
        NewCamera.Perspective.ZNear       = static_cast<float>(PerspectiveCam.GetZNear());
        NewCamera.Perspective.ZFar        = static_cast<float>(PerspectiveCam.GetZFar());
    }
    else if (GltfCam.GetType() == "orthographic")
    {
        NewCamera.Type = spw::Camera::Projection::Orthographic;

        const auto& OrthoCam{GltfCam.GetOrthographic()};
        NewCamera.Orthographic.XMag  = static_cast<float>(OrthoCam.GetXMag());
        NewCamera.Orthographic.YMag  = static_cast<float>(OrthoCam.GetYMag());
        NewCamera.Orthographic.ZNear = static_cast<float>(OrthoCam.GetZNear());
        NewCamera.Orthographic.ZFar  = static_cast<float>(OrthoCam.GetZFar());
    }
    else
    {
        UNEXPECTED("Unexpected camera type: ", GltfCam.GetType());
    }

    return &NewCamera;
}

spw::Light* ModelBuilder::LoadLight(int GltfLightIndex)
{
    // https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_lights_punctual

    if (GltfLightIndex < 0)
        return nullptr;

    auto light_it = m_LightIndexRemapping.find(GltfLightIndex);
    VERIFY(light_it != m_LightIndexRemapping.end(), "Light with GLTF index ", GltfLightIndex, " is not present in the map. This appears to be a bug.");
    const auto LoadedLightId = light_it->second;

    auto& NewLight = m_Model.Lights[LoadedLightId];

    if (m_LoadedLights.find(LoadedLightId) != m_LoadedLights.end())
    {
        // The Light has already been loaded
        return &NewLight;
    }
    m_LoadedLights.emplace(LoadedLightId);

    const auto& GltfLight = m_gltf_model->GetLight(GltfLightIndex);

    NewLight.Name = GltfLight.GetName();
    if (GltfLight.GetType() == "directional")
    {
        NewLight.Type = spw::Light::TYPE::DIRECTIONAL;
    }
    else if (GltfLight.GetType() == "point")
    {
        NewLight.Type = spw::Light::TYPE::POINT;
    }
    else if (GltfLight.GetType() == "spot")
    {
        NewLight.Type           = spw::Light::TYPE::SPOT;
        NewLight.InnerConeAngle = static_cast<float>(GltfLight.GetInnerConeAngle());
        NewLight.OuterConeAngle = static_cast<float>(GltfLight.GetOuterConeAngle());
    }
    else
    {
        UNEXPECTED("Unexpected light type: ", GltfLight.GetType());
    }

    const auto& Color = GltfLight.GetColor();
    for (size_t i = 0; i < std::min<size_t>(3, Color.size()); ++i)
        NewLight.Color[i] = static_cast<float>(Color[i]);

    NewLight.Intensity = static_cast<float>(GltfLight.GetIntensity());
    NewLight.Range     = static_cast<float>(GltfLight.GetRange());

    return &NewLight;
}

spw::Node* ModelBuilder::LoadNode(spw::Node* Parent, spw::Scene& scene, int GltfNodeIndex)
{
    auto node_it = m_NodeIndexRemapping.find(GltfNodeIndex);
    VERIFY(node_it != m_NodeIndexRemapping.end(), "Node with GLTF index ", GltfNodeIndex, " is not present in the map. This appears to be a bug.");
    const auto LoadedNodeId = node_it->second;

    auto& NewNode = m_Model.Nodes[LoadedNodeId];
    VERIFY_EXPR(NewNode.Index == LoadedNodeId);
    // Add the node to the scene's linear nodes array
    scene.LinearNodes.emplace_back(&NewNode);

    if (m_LoadedNodes.find(LoadedNodeId) != m_LoadedNodes.end())
        return &NewNode;
    m_LoadedNodes.emplace(LoadedNodeId);

    const auto& GltfNode = m_gltf_model->GetNode(GltfNodeIndex);

    NewNode.Name   = GltfNode.GetName();
    NewNode.Parent = Parent;

    m_NodeIdToSkinId[LoadedNodeId] = GltfNode.GetSkinId();

    // Any node can define a local space transformation either by supplying a matrix property,
    // or any of translation, rotation, and scale properties (also known as TRS properties).

    if (GltfNode.GetTranslation().size() == 3)
    {
        NewNode.Translation = float3::MakeVector(GltfNode.GetTranslation().data());
    }

    if (GltfNode.GetRotation().size() == 4)
    {
        NewNode.Rotation.q = float4::MakeVector(GltfNode.GetRotation().data());
    }

    if (GltfNode.GetScale().size() == 3)
    {
        NewNode.Scale = float3::MakeVector(GltfNode.GetScale().data());
    }

    if (GltfNode.GetMatrix().size() == 16)
    {
        NewNode.Matrix = float4x4::MakeMatrix(GltfNode.GetMatrix().data());
    }

    // Load children first
    NewNode.Children.reserve(GltfNode.GetChildrenIds().size());
    for (const auto ChildNodeIdx : GltfNode.GetChildrenIds())
    {
        NewNode.Children.push_back(LoadNode(&NewNode, scene, ChildNodeIdx));
    }

    // Node contains mesh data
    NewNode.pMesh   = LoadMesh(GltfNode.GetMeshId());
    NewNode.pCamera = LoadCamera(GltfNode.GetCameraId());
    NewNode.pLight  = LoadLight(GltfNode.GetLightId());

    if (m_CI.NodeLoadCallback)
    {
        m_CI.NodeLoadCallback(GltfNodeIndex, &GltfNode.Get(), NewNode);
    }

    return &NewNode;
}

Uint32 ModelBuilder::ConvertVertexData(const PrimitiveKey& Key, Uint32 VertexCount)
{
    Uint32 StartVertex = ~0u;

    // Note: different primitives may use different vertex attributes.
    //       Since all primitives share the same vertex buffers, we need to
    //       make sure that all buffers have consistently sizes.
    for (size_t i = 0; i < m_VertexData.size(); ++i)
    {
        const auto Stride = m_Model.VertexData.Strides[i];
        if (Stride == 0)
            continue; // Skip unused buffers

        VERIFY((m_VertexData[i].size() % Stride) == 0, "Buffer data size is not a multiple of the element stride");
        auto VertexOffset = static_cast<Uint32>(m_VertexData[i].size() / Stride);
        if (StartVertex == ~0u)
            StartVertex = VertexOffset;
        else
            VERIFY(m_VertexData[i].empty() || StartVertex == VertexOffset, "All vertex buffers must have the same number of vertices");
    }
    for (size_t i = 0; i < m_VertexData.size(); ++i)
    {
        const auto Stride = m_Model.VertexData.Strides[i];
        if (Stride == 0)
            continue;

        // Always resize non-empty buffers to ensure consistency
        if (m_CI.CreateStubVertexBuffers || !m_VertexData[i].empty())
        {
            m_VertexData[i].resize(size_t{StartVertex + VertexCount} * Stride);
        }
    }

    VERIFY_EXPR(Key.AccessorIds.size() == m_Model.GetNumVertexAttributes());
    for (Uint32 i = 0; i < m_Model.GetNumVertexAttributes(); ++i)
    {
        const auto& Attrib       = m_Model.VertexAttributes[i];
        const auto  BufferId     = Attrib.BufferId;
        const auto  VertexStride = m_Model.VertexData.Strides[BufferId];
        const auto  DataOffset   = size_t{StartVertex} * size_t{VertexStride};
        const auto  RequiredSize = DataOffset + size_t{VertexCount} * VertexStride;

        auto& VertexData = m_VertexData[BufferId];

        const auto AccessorId = Key.AccessorIds[i];
        if (AccessorId < 0)
        {
            if (Attrib.pDefaultValue != nullptr && VertexData.size() == RequiredSize)
            {
                auto dst_it = VertexData.begin() + DataOffset + Attrib.RelativeOffset;
                WriteDefaultAttibuteValue(Attrib.pDefaultValue, dst_it, Attrib.ValueType, Attrib.NumComponents, VertexStride, VertexCount);
            }
            continue;
        }

        if (VertexData.size() < RequiredSize)
        {
            size_t OriginalSize = VertexData.size();
            VertexData.resize(RequiredSize);
            if (OriginalSize < DataOffset)
            {
                // We have to write default values for all attributes in this buffer
                // up to the current offset.
                WriteDefaultAttibutes(Attrib.BufferId, OriginalSize, DataOffset);
            }
        }

        const auto GltfVerts     = GetGltfDataInfo(AccessorId);
        const auto ValueType     = GltfVerts.Accessor.GetComponentType();
        const auto NumComponents = GltfVerts.Accessor.GetNumComponents();
        const auto SrcStride     = GltfVerts.ByteStride;
        const bool IsNormalized  = GltfVerts.Accessor.IsNormalized();
        VERIFY_EXPR(SrcStride > 0);

        auto dst_it = VertexData.begin() + DataOffset + Attrib.RelativeOffset;

        VERIFY_EXPR(static_cast<Uint32>(GltfVerts.Count) == VertexCount);
        WriteGltfData({GltfVerts.pData,
                       ValueType,
                       static_cast<Uint32>(NumComponents),
                       static_cast<Uint32>(SrcStride),
                       dst_it,
                       Attrib.ValueType,
                       Attrib.NumComponents,
                       VertexStride,
                       VertexCount,
                       IsNormalized});

        m_Model.VertexData.EnabledAttributeFlags |= (1u << i);
    }

    return StartVertex;
}

template <typename SrcType, typename DstType>
inline void ModelBuilder::WriteIndexData(const void*                  pSrc,
                                         size_t                       SrcStride,
                                         std::vector<Uint8>::iterator dst_it,
                                         Uint32                       NumElements,
                                         Uint32                       BaseVertex)
{
    for (size_t i = 0; i < NumElements; ++i)
    {
        const auto& SrcInd = *reinterpret_cast<const SrcType*>(static_cast<const Uint8*>(pSrc) + i * SrcStride);
        auto&       DstInd = reinterpret_cast<DstType&>(*dst_it);

        DstInd = static_cast<DstType>(SrcInd + BaseVertex);
        dst_it += sizeof(DstType);
    }
}

Uint32 ModelBuilder::ConvertIndexData(int AccessorId, Uint32 BaseVertex)
{
    VERIFY_EXPR(AccessorId >= 0);

    const auto GltfIndices = GetGltfDataInfo(AccessorId);
    const auto IndexSize   = m_Model.IndexData.IndexSize;
    const auto IndexCount  = static_cast<uint32_t>(GltfIndices.Count);

    auto IndexDataStart = m_IndexData.size();
    VERIFY((IndexDataStart % IndexSize) == 0, "Current offset is not a multiple of index size");
    m_IndexData.resize(IndexDataStart + size_t{IndexCount} * size_t{IndexSize});
    auto index_it = m_IndexData.begin() + IndexDataStart;

    const auto ComponentType = GltfIndices.Accessor.GetComponentType();
    const auto SrcStride     = static_cast<size_t>(GltfIndices.ByteStride);
    VERIFY(SrcStride >= GetValueSize(ComponentType), "Byte stride (", SrcStride, ") is too small.");
    VERIFY_EXPR(IndexSize == 4 || IndexSize == 2);
    switch (ComponentType)
    {
        case VT_UINT32:
            if (IndexSize == 4)
                WriteIndexData<Uint32, Uint32>(GltfIndices.pData, SrcStride, index_it, IndexCount, BaseVertex);
            else
                WriteIndexData<Uint32, Uint16>(GltfIndices.pData, SrcStride, index_it, IndexCount, BaseVertex);
            break;

        case VT_UINT16:
            if (IndexSize == 4)
                WriteIndexData<Uint16, Uint32>(GltfIndices.pData, SrcStride, index_it, IndexCount, BaseVertex);
            else
                WriteIndexData<Uint16, Uint16>(GltfIndices.pData, SrcStride, index_it, IndexCount, BaseVertex);
            break;

        case VT_UINT8:
            if (IndexSize == 4)
                WriteIndexData<Uint8, Uint32>(GltfIndices.pData, SrcStride, index_it, IndexCount, BaseVertex);
            else
                WriteIndexData<Uint8, Uint16>(GltfIndices.pData, SrcStride, index_it, IndexCount, BaseVertex);
            break;

        default:
            UNEXPECTED("Index component type ", GetValueTypeString(ComponentType), " is not supported!");
            return 0;
    }

    return IndexCount;
}

void ModelBuilder::LoadSkins()
{
    m_Model.Skins.resize(m_gltf_model->GetSkinCount());
    for (size_t i = 0; i < m_gltf_model->GetSkinCount(); ++i)
    {
        const auto& GltfSkin = m_gltf_model->GetSkin(i);
        auto&       NewSkin  = m_Model.Skins[i];

        NewSkin.Name = GltfSkin.GetName();

        // Find skeleton root node
        if (GltfSkin.GetSkeletonId() >= 0)
        {
            NewSkin.pSkeletonRoot = NodeFromGltfIndex(GltfSkin.GetSkeletonId());
        }

        // Find joint nodes
        for (int JointIndex : GltfSkin.GetJointIds())
        {
            if (auto* node = NodeFromGltfIndex(JointIndex))
            {
                NewSkin.Joints.push_back(node);
            }
        }

        // Get inverse bind matrices from buffer
        if (GltfSkin.GetInverseBindMatricesId() >= 0)
        {
            const auto GltfSkins = GetGltfDataInfo(GltfSkin.GetInverseBindMatricesId());
            NewSkin.InverseBindMatrices.resize(GltfSkins.Count);
            VERIFY(GltfSkins.ByteStride == sizeof(float4x4), "Tightly packed skin data is expected.");
            memcpy(NewSkin.InverseBindMatrices.data(), GltfSkins.pData, GltfSkins.Count * sizeof(float4x4));
        }
    }
}

void ModelBuilder::LoadAnimations()
{
    const auto AnimationCount = m_gltf_model->GetAnimationCount();
    m_Model.Animations.resize(AnimationCount);
    for (size_t anim = 0; anim < AnimationCount; ++anim)
    {
        const auto& GltfAnim = m_gltf_model->GetAnimation(anim);
        auto&       Anim     = m_Model.Animations[anim];

        Anim.Name = GltfAnim.GetName();
        if (Anim.Name.empty())
        {
            Anim.Name = std::to_string(anim);
        }

        // Samplers
        const auto SamplerCount = GltfAnim.GetSamplerCount();
        Anim.Samplers.reserve(SamplerCount);
        for (size_t sam = 0; sam < SamplerCount; ++sam)
        {
            const auto& GltfSam = GltfAnim.GetSampler(sam);

            Anim.Samplers.emplace_back(GltfSam.GetInterpolation());
            auto& AnimSampler = Anim.Samplers.back();

            // Read sampler input time values
            {
                const auto GltfInputs = GetGltfDataInfo(GltfSam.GetInputId());
                VERIFY(GltfInputs.Accessor.GetComponentType() == VT_FLOAT32, "Float32 data is expected.");
                VERIFY(GltfInputs.ByteStride == sizeof(float), "Tightly packed data is expected.");

                AnimSampler.Inputs.resize(GltfInputs.Count);
                memcpy(AnimSampler.Inputs.data(), GltfInputs.pData, sizeof(float) * GltfInputs.Count);

                // Note that different samplers may have different time ranges.
                // We need to find the overall animation time range.
                Anim.Start = std::min(Anim.Start, AnimSampler.Inputs.front());
                Anim.End   = std::max(Anim.End, AnimSampler.Inputs.back());
#ifdef DILIGENT_DEVELOPMENT
                for (size_t i = 0; i + 1 < AnimSampler.Inputs.size(); ++i)
                {
                    if (AnimSampler.Inputs[i] >= AnimSampler.Inputs[i + 1])
                    {
                        LOG_ERROR_MESSAGE("Animation '", Anim.Name, "' sampler ", sam, " input time values are not monotonic at index ", i);
                    }
                }
#endif
            }


            // Read sampler output T/R/S values
            {
                const auto GltfOutputs = GetGltfDataInfo(GltfSam.GetOutputId());
                VERIFY(GltfOutputs.Accessor.GetComponentType() == VT_FLOAT32, "Float32 data is expected.");
                VERIFY(GltfOutputs.ByteStride >= static_cast<int>(GltfOutputs.Accessor.GetNumComponents() * sizeof(float)), "Byte stide is too small.");

                AnimSampler.OutputsVec4.reserve(GltfOutputs.Count);
                const auto NumComponents = GltfOutputs.Accessor.GetNumComponents();
                switch (NumComponents)
                {
                    case 3:
                    {
                        for (size_t i = 0; i < GltfOutputs.Count; ++i)
                        {
                            const auto& SrcVec3 = *reinterpret_cast<const float3*>(static_cast<const Uint8*>(GltfOutputs.pData) + GltfOutputs.ByteStride * i);
                            AnimSampler.OutputsVec4.push_back(float4{SrcVec3, 0.0f});
                        }
                        break;
                    }

                    case 4:
                    {
                        for (size_t i = 0; i < GltfOutputs.Count; ++i)
                        {
                            const auto& SrcVec4 = *reinterpret_cast<const float4*>(static_cast<const Uint8*>(GltfOutputs.pData) + GltfOutputs.ByteStride * i);
                            AnimSampler.OutputsVec4.push_back(SrcVec4);
                        }
                        break;
                    }

                    default:
                    {
                        LOG_WARNING_MESSAGE("Unsupported component count: ", NumComponents);
                        break;
                    }
                }
            }
        }

        const auto ChannelCount = GltfAnim.GetChannelCount();
        Anim.Channels.reserve(ChannelCount);
        for (size_t chnl = 0; chnl < ChannelCount; ++chnl)
        {
            const auto& GltfChannel = GltfAnim.GetChannel(chnl);

            const auto PathType = GltfChannel.GetPathType();
            if (PathType == spw::AnimationChannel::PATH_TYPE::WEIGHTS)
            {
                LOG_WARNING_MESSAGE("Weights are not yet supported, skipping channel");
                continue;
            }

            const auto SamplerIndex = GltfChannel.GetSamplerId();
            if (SamplerIndex < 0)
                continue;

            const auto NodeId = GltfChannel.GetTargetNodeId();
            if (NodeId < 0)
                continue;

            auto* pNode = NodeFromGltfIndex(NodeId);
            if (pNode == nullptr)
                continue;

            Anim.Channels.emplace_back(PathType, pNode, SamplerIndex);
        }
    }
}

bool ModelBuilder::LoadAnimationAndSkin()
{
    bool UsesAnimation = false;
    for (size_t i = 0; i < m_Model.GetNumVertexAttributes(); ++i)
    {
        const auto& Attrib = m_Model.GetVertexAttribute(i);

        if (strncmp(Attrib.Name, "WEIGHTS", 7) == 0 ||
            strncmp(Attrib.Name, "JOINTS", 6) == 0)
        {
            UsesAnimation = true;
            break;
        }
    }

    if (!UsesAnimation)
        return false;

    LoadAnimations();
    LoadSkins();

    // Assign skins
    for (int i = 0; i < static_cast<int>(m_Model.Nodes.size()); ++i)
    {
        VERIFY_EXPR(m_Model.Nodes[i].Index == i);
        auto skin_it = m_NodeIdToSkinId.find(i);
        if (skin_it != m_NodeIdToSkinId.end())
        {
            const auto SkinIndex = skin_it->second;
            if (SkinIndex >= 0)
            {
                auto& N               = m_Model.Nodes[i];
                N.pSkin               = &m_Model.Skins[SkinIndex];
                N.SkinTransformsIndex = m_Model.SkinTransformsCount++;
            }
        }
        else
        {
            UNEXPECTED("Node ", i, " has no assigned skin id. This appears to be a bug.");
        }
    }

    return true;
}

void ModelBuilder::Execute(tinygltf::Model* tiny_gltf_model, int SceneIndex, IRenderDevice* pDevice)
{
    LoadScenes(SceneIndex);

    for (const auto& scene : m_Model.Scenes)
    {
        for (const auto* pNode : scene.RootNodes)
        {
            // We temporarily store GLTF node index in the pointer
            const auto GltfNodeId = static_cast<int>(reinterpret_cast<size_t>(pNode));
            AllocateNode(GltfNodeId);
        }
    }

    m_Model.Nodes.shrink_to_fit();
    m_Model.Meshes.shrink_to_fit();
    m_Model.Cameras.shrink_to_fit();

    for (auto& scene : m_Model.Scenes)
    {
        for (size_t i = 0; i < scene.RootNodes.size(); ++i)
        {
            auto&      pNode      = scene.RootNodes[i];
            const auto GltfNodeId = static_cast<int>(reinterpret_cast<size_t>(pNode));
            pNode                 = LoadNode(nullptr, scene, GltfNodeId);
        }
        scene.LinearNodes.shrink_to_fit();
    }
    m_Model.Materials.shrink_to_fit();
    VERIFY_EXPR(m_LoadedNodes.size() == m_Model.Nodes.size());
    VERIFY_EXPR(m_LoadedMeshes.size() == m_Model.Meshes.size());
    VERIFY_EXPR(m_LoadedCameras.size() == m_Model.Cameras.size());
    VERIFY_EXPR(m_LoadedLights.size() == m_Model.Lights.size());

    LoadAnimationAndSkin();

    InitIndexBuffer(pDevice);
    InitVertexBuffers(pDevice);
}
void ModelBuilder::LoadScenes(int SceneIndex)
{
    auto AddScene = [&](int GltfSceneId) {
        const auto& GltfScene = m_gltf_model->GetScene(GltfSceneId);

        m_Model.Scenes.emplace_back();
        auto& scene     = m_Model.Scenes.back();
        scene.Name      = GltfScene.GetName();
        auto& RootNodes = scene.RootNodes;
        RootNodes.resize(GltfScene.GetNodeCount());

        // Temporarily store node ids as pointers
        for (size_t i = 0; i < RootNodes.size(); ++i)
            RootNodes[i] = reinterpret_cast<spw::Node*>(static_cast<size_t>(GltfScene.GetNodeId(i)));
    };

    if (const auto SceneCount = static_cast<int>(m_gltf_model->GetSceneCount()))
    {
        auto SceneId = SceneIndex;
        if (SceneId >= SceneCount)
        {
            DEV_ERROR("Scene id ", SceneIndex, " is invalid: GLTF model only contains ", SceneCount, " scenes.");
            SceneId = -1;
        }

        if (SceneId >= 0)
        {
            // Load only the selected scene
            m_Model.Scenes.reserve(1);
            AddScene(SceneId);
            m_Model.DefaultSceneId = 0;
        }
        else
        {
            // Load all scenes
            m_Model.Scenes.reserve(SceneCount);
            for (int i = 0; i < SceneCount; ++i)
                AddScene(i);

            m_Model.DefaultSceneId = m_gltf_model->GetDefaultSceneId();
            if (m_Model.DefaultSceneId < 0)
                m_Model.DefaultSceneId = 0;

            if (m_Model.DefaultSceneId >= SceneCount)
            {
                LOG_ERROR_MESSAGE("Default scene id ", m_Model.DefaultSceneId, " is invalid: GLTF model only contains ", SceneCount, " scenes. Using scene 0 as default.");
                m_Model.DefaultSceneId = 0;
            }
        }
    }
    else
    {
        m_Model.Scenes.resize(1);
        auto& RootNodes = m_Model.Scenes[0].RootNodes;
        RootNodes.resize(m_gltf_model->GetNodeCount());

        // Load all nodes if there are no scenes
        for (size_t node_idx = 0; node_idx < RootNodes.size(); ++node_idx)
            RootNodes[node_idx] = reinterpret_cast<spw::Node*>(node_idx);
    }

    m_Model.Scenes.shrink_to_fit();
}

void ModelBuilder::AllocateNode(int GltfNodeIndex)
{
    {
        const auto NodeId = static_cast<int>(m_Model.Nodes.size());
        if (!m_NodeIndexRemapping.emplace(GltfNodeIndex, NodeId).second)
        {
            // The node has already been allocated.
            // Note: we iterate through the list of nodes and recursively allocate
            //       all child nodes. As a result, we may encounter a node that
            //       has already been allocated as a child of another.
            //       Besides, same node may be present in multiple scenes.
            return;
        }

        m_Model.Nodes.emplace_back(NodeId);
    }

    const auto& GltfNode = m_gltf_model->GetNode(GltfNodeIndex);
    for (const auto ChildNodeIdx : GltfNode.GetChildrenIds())
    {
        AllocateNode(ChildNodeIdx);
    }

    auto AllocateNodeComponent = [](int GltfIndex, auto& Components, auto& IndexRemapping) {
        if (GltfIndex < 0)
            return;

        const auto Id = static_cast<int>(Components.size());
        if (IndexRemapping.emplace(GltfIndex, Id).second)
            Components.emplace_back();
    };

    AllocateNodeComponent(GltfNode.GetMeshId(), m_Model.Meshes, m_MeshIndexRemapping);
    AllocateNodeComponent(GltfNode.GetCameraId(), m_Model.Cameras, m_CameraIndexRemapping);
    AllocateNodeComponent(GltfNode.GetLightId(), m_Model.Lights, m_LightIndexRemapping);
}
} // namespace GLTF
} // namespace Diligent

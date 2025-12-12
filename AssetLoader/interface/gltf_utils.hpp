#pragma once

#include "BasicMath.hpp"

#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tiny_gltf.h"

inline Diligent::VALUE_TYPE TinyGltfComponentTypeToValueType(int GltfCompType)
{
    switch (GltfCompType)
    {
            // clang-format off
        case TINYGLTF_COMPONENT_TYPE_BYTE:           return Diligent::VT_INT8;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:  return Diligent::VT_UINT8;
        case TINYGLTF_COMPONENT_TYPE_SHORT:          return Diligent::VT_INT16;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: return Diligent::VT_UINT16;
        case TINYGLTF_COMPONENT_TYPE_INT:            return Diligent::VT_INT32;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:   return Diligent::VT_UINT32;
        case TINYGLTF_COMPONENT_TYPE_FLOAT:          return Diligent::VT_FLOAT32;
        case TINYGLTF_COMPONENT_TYPE_DOUBLE:         return Diligent::VT_FLOAT64;
            // clang-format on
        default:
            UNEXPECTED("Unknown GLTF component type");
            return Diligent::VT_UNDEFINED;
    }
}

struct TinyGltfNodeWrapper
{
    const tinygltf::Node& Node;

    const tinygltf::Node& Get() const { return Node; }

    // clang-format off
    const std::string&         GetName()        const { return Node.name; }
    const std::vector<double>& GetTranslation() const { return Node.translation; }
    const std::vector<double>& GetRotation()    const { return Node.rotation; }
    const std::vector<double>& GetScale()       const { return Node.scale; }
    const std::vector<double>& GetMatrix()      const { return Node.matrix; }
    const std::vector<int>&    GetChildrenIds() const { return Node.children; }

    int GetMeshId()   const { return Node.mesh; }
    int GetCameraId() const { return Node.camera; }
    int GetLightId()  const { return Node.light; }
    int GetSkinId()   const { return Node.skin; }
    // clang-format on
};

struct TinyGltfPrimitiveWrapper
{
    const tinygltf::Primitive& Primitive;

    const int* GetAttribute(const char* Name) const
    {
        auto attrib_it = Primitive.attributes.find(Name);
        return attrib_it != Primitive.attributes.end() ?
            &attrib_it->second :
            nullptr;
    }

    const tinygltf::Primitive& Get() const { return Primitive; }

    int GetIndicesId() const { return Primitive.indices; }
    int GetMaterialId() const { return Primitive.material; }
};

struct TinyGltfMeshWrapper
{
    const tinygltf::Mesh& Mesh;

    const tinygltf::Mesh& Get() const { return Mesh; }
    const std::string&    GetName() const { return Mesh.name; }

    size_t                   GetPrimitiveCount() const { return Mesh.primitives.size(); }
    TinyGltfPrimitiveWrapper GetPrimitive(size_t Idx) const { return TinyGltfPrimitiveWrapper{Mesh.primitives[Idx]}; };
};

struct TinyGltfBufferViewWrapper;
struct TinyGltfAccessorWrapper
{
    const tinygltf::Accessor& Accessor;

    size_t           GetCount() const { return Accessor.count; }
    Diligent::float3 GetMinValues() const
    {
        return Diligent::float3{
            static_cast<float>(Accessor.minValues[0]),
            static_cast<float>(Accessor.minValues[1]),
            static_cast<float>(Accessor.minValues[2]),
        };
    }
    Diligent::float3 GetMaxValues() const
    {
        return Diligent::float3{
            static_cast<float>(Accessor.maxValues[0]),
            static_cast<float>(Accessor.maxValues[1]),
            static_cast<float>(Accessor.maxValues[2]),
        };
    }

    // clang-format off
    int        GetBufferViewId()  const { return Accessor.bufferView; }
    size_t     GetByteOffset()    const { return Accessor.byteOffset; }
    Diligent::VALUE_TYPE GetComponentType() const { return TinyGltfComponentTypeToValueType(Accessor.componentType); }
    int32_t    GetNumComponents() const { return tinygltf::GetNumComponentsInType(Accessor.type); }
    bool       IsNormalized()     const { return Accessor.normalized; }
    // clang-format on
    int GetByteStride(const TinyGltfBufferViewWrapper& View) const;
};

struct TinyGltfPerspectiveCameraWrapper
{
    const tinygltf::PerspectiveCamera& Camera;

    // clang-format off
    double GetAspectRatio() const { return Camera.aspectRatio; }
    double GetYFov()        const { return Camera.yfov; }
    double GetZNear()       const { return Camera.znear; }
    double GetZFar()        const { return Camera.zfar; }
    // clang-format on
};

struct TinyGltfOrthoCameraWrapper
{
    const tinygltf::OrthographicCamera& Camera;

    // clang-format off
    double GetXMag()  const { return Camera.xmag; }
    double GetYMag()  const { return Camera.ymag; }
    double GetZNear() const { return Camera.znear; }
    double GetZFar()  const { return Camera.zfar; }
    // clang-format on
};

struct TinyGltfCameraWrapper
{
    const tinygltf::Camera& Camera;

    const std::string&               GetName() const { return Camera.name; }
    const std::string&               GetType() const { return Camera.type; }
    TinyGltfPerspectiveCameraWrapper GetPerspective() const { return TinyGltfPerspectiveCameraWrapper{Camera.perspective}; }
    TinyGltfOrthoCameraWrapper       GetOrthographic() const { return TinyGltfOrthoCameraWrapper{Camera.orthographic}; }
};

struct TinyGltfLightWrapper
{
    const tinygltf::Light& Light;

    // clang-format off
    const std::string&         GetName()  const { return Light.name; }
    const std::string&         GetType()  const { return Light.type; }
    const std::vector<double>& GetColor() const { return Light.color; }

    const double& GetIntensity()      const { return Light.intensity; }
    const double& GetRange()          const { return Light.range; }
    const double& GetInnerConeAngle() const { return Light.spot.innerConeAngle; }
    const double& GetOuterConeAngle() const { return Light.spot.outerConeAngle; }
    // clang-format on
};

struct TinyGltfBufferViewWrapper
{
    const tinygltf::BufferView& View;

    int    GetBufferId() const { return View.buffer; }
    size_t GetByteOffset() const { return View.byteOffset; }
};

struct TinyGltfBufferWrapper
{
    const tinygltf::Buffer& Buffer;

    const Diligent::Uint8* GetData(size_t Offset) const { return &Buffer.data[Offset]; }
};

struct TinyGltfSkinWrapper
{
    const tinygltf::Skin& Skin;

    const std::string&      GetName() const { return Skin.name; }
    int                     GetSkeletonId() const { return Skin.skeleton; }
    int                     GetInverseBindMatricesId() const { return Skin.inverseBindMatrices; }
    const std::vector<int>& GetJointIds() const { return Skin.joints; }
};

struct TinyGltfAnimationSamplerWrapper
{
    const tinygltf::AnimationSampler& Sam;

    spw::AnimationSampler::INTERPOLATION_TYPE GetInterpolation() const
    {
        if (Sam.interpolation == "LINEAR")
            return spw::AnimationSampler::INTERPOLATION_TYPE::LINEAR;
        if (Sam.interpolation == "STEP")
            return spw::AnimationSampler::INTERPOLATION_TYPE::STEP;
        if (Sam.interpolation == "CUBICSPLINE")
            return spw::AnimationSampler::INTERPOLATION_TYPE::CUBICSPLINE;

        UNEXPECTED("Unexpected animation interpolation type: ", Sam.interpolation);
        return spw::AnimationSampler::INTERPOLATION_TYPE::LINEAR;
    }

    int GetInputId() const { return Sam.input; }
    int GetOutputId() const { return Sam.output; }
};


struct TinyGltfAnimationChannelWrapper
{
    const tinygltf::AnimationChannel& Channel;

    spw::AnimationChannel::PATH_TYPE GetPathType() const
    {
        if (Channel.target_path == "rotation")
            return spw::AnimationChannel::PATH_TYPE::ROTATION;
        else if (Channel.target_path == "translation")
            return spw::AnimationChannel::PATH_TYPE::TRANSLATION;
        else if (Channel.target_path == "scale")
            return spw::AnimationChannel::PATH_TYPE::SCALE;
        else if (Channel.target_path == "weights")
            return spw::AnimationChannel::PATH_TYPE::WEIGHTS;
        else
        {
            UNEXPECTED("Unsupported animation channel path ", Channel.target_path);
            return spw::AnimationChannel::PATH_TYPE::ROTATION;
        }
    }

    int GetSamplerId() const { return Channel.sampler; }
    int GetTargetNodeId() const { return Channel.target_node; }
};

struct TinyGltfAnimationWrapper
{
    const tinygltf::Animation& Anim;

    const std::string& GetName() const { return Anim.name; }

    size_t GetSamplerCount() const { return Anim.samplers.size(); }
    size_t GetChannelCount() const { return Anim.channels.size(); }

    TinyGltfAnimationSamplerWrapper GetSampler(size_t Id) const { return TinyGltfAnimationSamplerWrapper{Anim.samplers[Id]}; }
    TinyGltfAnimationChannelWrapper GetChannel(size_t Id) const { return TinyGltfAnimationChannelWrapper{Anim.channels[Id]}; }
};

struct TinyGltfSceneWrapper
{
    const tinygltf::Scene& Scene;

    const std::string& GetName() const { return Scene.name; }
    size_t             GetNodeCount() const { return Scene.nodes.size(); }
    int                GetNodeId(size_t Idx) const { return Scene.nodes[Idx]; }
};

struct TinyGltfModelWrapper
{
    explicit TinyGltfModelWrapper(tinygltf::Model* gltf_model)
    {
        Model = gltf_model;
    }

    tinygltf::Model* Model;

    const tinygltf::Model& Get() const { return *Model; }

    // clang-format off
    TinyGltfNodeWrapper       GetNode      (int idx) const { return TinyGltfNodeWrapper      {Get().nodes      [idx]}; }
    TinyGltfSceneWrapper      GetScene     (int idx) const { return TinyGltfSceneWrapper     {Get().scenes     [idx]}; }
    TinyGltfMeshWrapper       GetMesh      (int idx) const { return TinyGltfMeshWrapper      {Get().meshes     [idx]}; }
    TinyGltfAccessorWrapper   GetAccessor  (int idx) const { return TinyGltfAccessorWrapper  {Get().accessors  [idx]}; }
    TinyGltfCameraWrapper     GetCamera    (int idx) const { return TinyGltfCameraWrapper    {Get().cameras    [idx]}; }
    TinyGltfLightWrapper      GetLight     (int idx) const { return TinyGltfLightWrapper     {Get().lights     [idx]}; }
    TinyGltfBufferViewWrapper GetBufferView(int idx) const { return TinyGltfBufferViewWrapper{Get().bufferViews[idx]}; }
    TinyGltfBufferWrapper     GetBuffer    (int idx) const { return TinyGltfBufferWrapper    {Get().buffers    [idx]}; }

    TinyGltfSkinWrapper      GetSkin      (size_t idx) const { return TinyGltfSkinWrapper      {Get().skins      [idx]}; }
    TinyGltfAnimationWrapper GetAnimation (size_t idx) const { return TinyGltfAnimationWrapper {Get().animations [idx]}; }

    size_t GetNodeCount()      const { return Get().nodes.size();      }
    size_t GetSceneCount()     const { return Get().scenes.size();     }
    size_t GetMeshCount()      const { return Get().meshes.size();     }
    size_t GetSkinCount()      const { return Get().skins.size();      }
    size_t GetAnimationCount() const { return Get().animations.size(); }

    int GetDefaultSceneId() const { return Get().defaultScene; }
    // clang-format on
};

inline int TinyGltfAccessorWrapper::GetByteStride(const TinyGltfBufferViewWrapper& View) const
{
    return Accessor.ByteStride(View.View);
}
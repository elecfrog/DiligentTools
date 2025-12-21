#pragma once

/// \file
/// Defines texture utilities

#include "../../../DiligentCore/Graphics/GraphicsEngine/interface/GraphicsTypes.h"
#include "../../../DiligentCore/Graphics/GraphicsEngine/interface/Texture.h"
#include "../../../DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "TextureLoader.h"

namespace Diligent
{

/// Parameters of the CopyPixels function.
struct CopyPixelsAttribs
{
    /// Texture width.
    UInt32 Width = 0;

    /// Texture height.
    UInt32 Height = 0;

    /// Source component size in bytes.
    UInt32 SrcComponentSize = 0;

    /// A pointer to source pixels.
    const void* pSrcPixels DEFAULT_INITIALIZER(nullptr);

    /// Source stride in bytes.
    UInt32 SrcStride DEFAULT_INITIALIZER(0);

    /// Source component count.
    UInt32 SrcCompCount DEFAULT_INITIALIZER(0);

    /// A pointer to destination pixels.
    void* pDstPixels DEFAULT_INITIALIZER(nullptr);

    /// Destination component size in bytes.
    UInt32 DstComponentSize DEFAULT_INITIALIZER(0);

    /// Destination stride in bytes.
    UInt32 DstStride DEFAULT_INITIALIZER(0);

    /// Destination component count.
    UInt32 DstCompCount DEFAULT_INITIALIZER(0);

    /// If true, flip the image vertically.
    bool FlipVertically DEFAULT_INITIALIZER(false);

    /// Texture component swizzle.
    TextureComponentMapping Swizzle DEFAULT_INITIALIZER(TextureComponentMapping::Identity());
};
typedef struct CopyPixelsAttribs CopyPixelsAttribs;

/// Copies texture pixels allowing changing the number of components.
void CopyPixels(const CopyPixelsAttribs& Attribs);


/// Parameters of the ExpandPixels function.
struct ExpandPixelsAttribs
{
    /// Source texture width.
    UInt32 SrcWidth DEFAULT_INITIALIZER(0);

    /// Source texture height.
    UInt32 SrcHeight DEFAULT_INITIALIZER(0);

    /// Texture component size in bytes.
    UInt32 ComponentSize DEFAULT_INITIALIZER(0);

    /// Component count.
    UInt32 ComponentCount DEFAULT_INITIALIZER(0);

    /// A pointer to source pixels.
    const void* pSrcPixels DEFAULT_INITIALIZER(nullptr);

    /// Source stride in bytes.
    UInt32 SrcStride DEFAULT_INITIALIZER(0);

    /// Destination texture width.
    UInt32 DstWidth DEFAULT_INITIALIZER(0);

    /// Destination texture height.
    UInt32 DstHeight DEFAULT_INITIALIZER(0);

    /// A pointer to destination pixels.
    void* pDstPixels DEFAULT_INITIALIZER(nullptr);

    /// Destination stride in bytes.
    UInt32 DstStride DEFAULT_INITIALIZER(0);
};
typedef struct ExpandPixelsAttribs ExpandPixelsAttribs;

/// Expands the texture pixels by repeating the last row and column.
void ExpandPixels(const ExpandPixelsAttribs& Attribs);


/// Parameters of the PremultiplyAlpha function.
struct PremultiplyAlphaAttribs
{
    /// Texture width.
    UInt32 Width DEFAULT_INITIALIZER(0);

    /// Texture height.
    UInt32 Height DEFAULT_INITIALIZER(0);

    /// A pointer to pixels.
    void* pPixels DEFAULT_INITIALIZER(nullptr);

    /// Stride in bytes.
    UInt32 Stride DEFAULT_INITIALIZER(0);

    /// Component count.
    UInt32 ComponentCount DEFAULT_INITIALIZER(0);

    /// Component type.
    VALUE_TYPE ComponentType DEFAULT_INITIALIZER(VT_UINT8);

    /// If true, the texture is in sRGB format.
    bool IsSRGB DEFAULT_INITIALIZER(false);
};
typedef struct PremultiplyAlphaAttribs PremultiplyAlphaAttribs;

/// Premultiplies image components with alpha in place.
/// \note Alpha is assumed to be the last component.
void PremultiplyAlpha(const PremultiplyAlphaAttribs& Attribs);


/// Creates a texture from file.

/// \param [in] FilePath    - Source file path.
/// \param [in] TexLoadInfo - Texture loading information.
/// \param [in] pDevice     - Render device that will be used to create the texture.
/// \param [out] ppTexture  - Memory location where pointer to the created texture will be written.
///
/// \note The function is thread-safe.
void CreateTextureFromFile(const Char* FilePath, const TextureLoadInfo& TexLoadInfo, IRenderDevice* pDevice, ITexture** ppTexture);


} // namespace Diligent

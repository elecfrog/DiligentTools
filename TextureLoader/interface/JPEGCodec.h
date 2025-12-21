#pragma once

/// \file
/// JPEG image loading and encoding functions.

#include "Image.h"
#include <types/base_types.hpp>

/// JPEG image decoding result.
DILIGENT_TYPED_ENUM(DECODE_JPEG_RESULT, UInt)
{
    /// JPEG image was decoded successfully.
    DECODE_JPEG_RESULT_OK = 0,

    /// Invalid arguments (e.g. null pointer).
    DECODE_JPEG_RESULT_INVALID_ARGUMENTS,

    /// Failed to initialize the decoder.
    DECODE_JPEG_RESULT_INITIALIZATION_FAILED,

    /// An unexpected error occurred while decoding the file.
    DECODE_JPEG_RESULT_DECODING_ERROR
};

/// JPEG image encoding result.
DILIGENT_TYPED_ENUM(ENCODE_JPEG_RESULT, UInt)
{
    /// Encoding finished successfully.
    ENCODE_JPEG_RESULT_OK = 0,

    /// Invalid arguments (e.g. null pointer).
    ENCODE_JPEG_RESULT_INVALID_ARGUMENTS,

    /// Failed to initialize the encoder.
    ENCODE_JPEG_RESULT_INITIALIZATION_FAILED
};
// clang-format on


/// Decodes jpeg image.

/// \param [in]  pSrcJpegBits - JPEG image encoded bits.
/// \param [in]  JpegDataSize - Size of the encoded JPEG image data.
/// \param [out] pDstPixels   - Decoded pixels data blob. The pixels are always tightly packed
///                             (for instance, components of 3-channel image will be written as |r|g|b|r|g|b|r|g|b|...).
/// \param [out] pDstImgDesc  - Decoded image description.
/// \return                     Decoding result, see Diligent::DECODE_JPEG_RESULT.
///
/// If `pDstPixels` is null, the function will only decode the image description and return DECODE_JPEG_RESULT_OK.
DECODE_JPEG_RESULT DecodeJpeg(const void* pSrcJpegBits,
                                                        size_t      JpegDataSize,
                                                        Diligent::IDataBlob*  pDstPixels,
                                                        Diligent::ImageDesc*  pDstImgDesc);


/// Encodes an image jpeg PNG format.

/// \param [in] pSrcPixels    - Source pixels. The pixels must be tightly packed
///                             (for instance, components of 3-channel image must be stored as `|r|g|b|r|g|b|r|g|b|...`).
///                             At the moment, only RGB images are supported.
/// \param [in] Width         - Image width.
/// \param [in] Height        - Image height.
/// \param [in] quality       - JPEG encoding qulaity, in the range from 0 to 100.
/// \param [out] pDstJpegBits - Encoded JPEG image bits.
/// \return                     Encoding result, see Diligent::ENCODE_JPEG_RESULT.
ENCODE_JPEG_RESULT EncodeJpeg(UInt8*     pSrcRGBPixels,
                                                        UInt     Width,
                                                        UInt     Height,
                                                        int        quality,
                                                        Diligent::IDataBlob* pDstJpegBits);


#include "rhi/utils.hpp"

namespace goma {
namespace utils {

uint32_t ComputeMipLevels(uint32_t width, uint32_t height) {
    auto mip_levels = 1U;
    auto min_wh = std::min(width, height);
    while (min_wh >> mip_levels) {
        mip_levels++;
    }
    return mip_levels;
}

FormatInfo GetFormatInfo(VkFormat format) {
    static const std::map<VkFormat, FormatInfo> vk_format_info = {
        {VK_FORMAT_UNDEFINED, {0, 0}},
        {VK_FORMAT_R4G4_UNORM_PACK8, {1, 2}},
        {VK_FORMAT_R4G4B4A4_UNORM_PACK16, {2, 4}},
        {VK_FORMAT_B4G4R4A4_UNORM_PACK16, {2, 4}},
        {VK_FORMAT_R5G6B5_UNORM_PACK16, {2, 3}},
        {VK_FORMAT_B5G6R5_UNORM_PACK16, {2, 3}},
        {VK_FORMAT_R5G5B5A1_UNORM_PACK16, {2, 4}},
        {VK_FORMAT_B5G5R5A1_UNORM_PACK16, {2, 4}},
        {VK_FORMAT_A1R5G5B5_UNORM_PACK16, {2, 4}},
        {VK_FORMAT_R8_UNORM, {1, 1}},
        {VK_FORMAT_R8_SNORM, {1, 1}},
        {VK_FORMAT_R8_USCALED, {1, 1}},
        {VK_FORMAT_R8_SSCALED, {1, 1}},
        {VK_FORMAT_R8_UINT, {1, 1}},
        {VK_FORMAT_R8_SINT, {1, 1}},
        {VK_FORMAT_R8_SRGB, {1, 1}},
        {VK_FORMAT_R8G8_UNORM, {2, 2}},
        {VK_FORMAT_R8G8_SNORM, {2, 2}},
        {VK_FORMAT_R8G8_USCALED, {2, 2}},
        {VK_FORMAT_R8G8_SSCALED, {2, 2}},
        {VK_FORMAT_R8G8_UINT, {2, 2}},
        {VK_FORMAT_R8G8_SINT, {2, 2}},
        {VK_FORMAT_R8G8_SRGB, {2, 2}},
        {VK_FORMAT_R8G8B8_UNORM, {3, 3}},
        {VK_FORMAT_R8G8B8_SNORM, {3, 3}},
        {VK_FORMAT_R8G8B8_USCALED, {3, 3}},
        {VK_FORMAT_R8G8B8_SSCALED, {3, 3}},
        {VK_FORMAT_R8G8B8_UINT, {3, 3}},
        {VK_FORMAT_R8G8B8_SINT, {3, 3}},
        {VK_FORMAT_R8G8B8_SRGB, {3, 3}},
        {VK_FORMAT_B8G8R8_UNORM, {3, 3}},
        {VK_FORMAT_B8G8R8_SNORM, {3, 3}},
        {VK_FORMAT_B8G8R8_USCALED, {3, 3}},
        {VK_FORMAT_B8G8R8_SSCALED, {3, 3}},
        {VK_FORMAT_B8G8R8_UINT, {3, 3}},
        {VK_FORMAT_B8G8R8_SINT, {3, 3}},
        {VK_FORMAT_B8G8R8_SRGB, {3, 3}},
        {VK_FORMAT_R8G8B8A8_UNORM, {4, 4}},
        {VK_FORMAT_R8G8B8A8_SNORM, {4, 4}},
        {VK_FORMAT_R8G8B8A8_USCALED, {4, 4}},
        {VK_FORMAT_R8G8B8A8_SSCALED, {4, 4}},
        {VK_FORMAT_R8G8B8A8_UINT, {4, 4}},
        {VK_FORMAT_R8G8B8A8_SINT, {4, 4}},
        {VK_FORMAT_R8G8B8A8_SRGB, {4, 4}},
        {VK_FORMAT_B8G8R8A8_UNORM, {4, 4}},
        {VK_FORMAT_B8G8R8A8_SNORM, {4, 4}},
        {VK_FORMAT_B8G8R8A8_USCALED, {4, 4}},
        {VK_FORMAT_B8G8R8A8_SSCALED, {4, 4}},
        {VK_FORMAT_B8G8R8A8_UINT, {4, 4}},
        {VK_FORMAT_B8G8R8A8_SINT, {4, 4}},
        {VK_FORMAT_B8G8R8A8_SRGB, {4, 4}},
        {VK_FORMAT_A8B8G8R8_UNORM_PACK32, {4, 4}},
        {VK_FORMAT_A8B8G8R8_SNORM_PACK32, {4, 4}},
        {VK_FORMAT_A8B8G8R8_USCALED_PACK32, {4, 4}},
        {VK_FORMAT_A8B8G8R8_SSCALED_PACK32, {4, 4}},
        {VK_FORMAT_A8B8G8R8_UINT_PACK32, {4, 4}},
        {VK_FORMAT_A8B8G8R8_SINT_PACK32, {4, 4}},
        {VK_FORMAT_A8B8G8R8_SRGB_PACK32, {4, 4}},
        {VK_FORMAT_A2R10G10B10_UNORM_PACK32, {4, 4}},
        {VK_FORMAT_A2R10G10B10_SNORM_PACK32, {4, 4}},
        {VK_FORMAT_A2R10G10B10_USCALED_PACK32, {4, 4}},
        {VK_FORMAT_A2R10G10B10_SSCALED_PACK32, {4, 4}},
        {VK_FORMAT_A2R10G10B10_UINT_PACK32, {4, 4}},
        {VK_FORMAT_A2R10G10B10_SINT_PACK32, {4, 4}},
        {VK_FORMAT_A2B10G10R10_UNORM_PACK32, {4, 4}},
        {VK_FORMAT_A2B10G10R10_SNORM_PACK32, {4, 4}},
        {VK_FORMAT_A2B10G10R10_USCALED_PACK32, {4, 4}},
        {VK_FORMAT_A2B10G10R10_SSCALED_PACK32, {4, 4}},
        {VK_FORMAT_A2B10G10R10_UINT_PACK32, {4, 4}},
        {VK_FORMAT_A2B10G10R10_SINT_PACK32, {4, 4}},
        {VK_FORMAT_R16_UNORM, {2, 1}},
        {VK_FORMAT_R16_SNORM, {2, 1}},
        {VK_FORMAT_R16_USCALED, {2, 1}},
        {VK_FORMAT_R16_SSCALED, {2, 1}},
        {VK_FORMAT_R16_UINT, {2, 1}},
        {VK_FORMAT_R16_SINT, {2, 1}},
        {VK_FORMAT_R16_SFLOAT, {2, 1}},
        {VK_FORMAT_R16G16_UNORM, {4, 2}},
        {VK_FORMAT_R16G16_SNORM, {4, 2}},
        {VK_FORMAT_R16G16_USCALED, {4, 2}},
        {VK_FORMAT_R16G16_SSCALED, {4, 2}},
        {VK_FORMAT_R16G16_UINT, {4, 2}},
        {VK_FORMAT_R16G16_SINT, {4, 2}},
        {VK_FORMAT_R16G16_SFLOAT, {4, 2}},
        {VK_FORMAT_R16G16B16_UNORM, {6, 3}},
        {VK_FORMAT_R16G16B16_SNORM, {6, 3}},
        {VK_FORMAT_R16G16B16_USCALED, {6, 3}},
        {VK_FORMAT_R16G16B16_SSCALED, {6, 3}},
        {VK_FORMAT_R16G16B16_UINT, {6, 3}},
        {VK_FORMAT_R16G16B16_SINT, {6, 3}},
        {VK_FORMAT_R16G16B16_SFLOAT, {6, 3}},
        {VK_FORMAT_R16G16B16A16_UNORM, {8, 4}},
        {VK_FORMAT_R16G16B16A16_SNORM, {8, 4}},
        {VK_FORMAT_R16G16B16A16_USCALED, {8, 4}},
        {VK_FORMAT_R16G16B16A16_SSCALED, {8, 4}},
        {VK_FORMAT_R16G16B16A16_UINT, {8, 4}},
        {VK_FORMAT_R16G16B16A16_SINT, {8, 4}},
        {VK_FORMAT_R16G16B16A16_SFLOAT, {8, 4}},
        {VK_FORMAT_R32_UINT, {4, 1}},
        {VK_FORMAT_R32_SINT, {4, 1}},
        {VK_FORMAT_R32_SFLOAT, {4, 1}},
        {VK_FORMAT_R32G32_UINT, {8, 2}},
        {VK_FORMAT_R32G32_SINT, {8, 2}},
        {VK_FORMAT_R32G32_SFLOAT, {8, 2}},
        {VK_FORMAT_R32G32B32_UINT, {12, 3}},
        {VK_FORMAT_R32G32B32_SINT, {12, 3}},
        {VK_FORMAT_R32G32B32_SFLOAT, {12, 3}},
        {VK_FORMAT_R32G32B32A32_UINT, {16, 4}},
        {VK_FORMAT_R32G32B32A32_SINT, {16, 4}},
        {VK_FORMAT_R32G32B32A32_SFLOAT, {16, 4}},
        {VK_FORMAT_R64_UINT, {8, 1}},
        {VK_FORMAT_R64_SINT, {8, 1}},
        {VK_FORMAT_R64_SFLOAT, {8, 1}},
        {VK_FORMAT_R64G64_UINT, {16, 2}},
        {VK_FORMAT_R64G64_SINT, {16, 2}},
        {VK_FORMAT_R64G64_SFLOAT, {16, 2}},
        {VK_FORMAT_R64G64B64_UINT, {24, 3}},
        {VK_FORMAT_R64G64B64_SINT, {24, 3}},
        {VK_FORMAT_R64G64B64_SFLOAT, {24, 3}},
        {VK_FORMAT_R64G64B64A64_UINT, {32, 4}},
        {VK_FORMAT_R64G64B64A64_SINT, {32, 4}},
        {VK_FORMAT_R64G64B64A64_SFLOAT, {32, 4}},
        {VK_FORMAT_B10G11R11_UFLOAT_PACK32, {4, 3}},
        {VK_FORMAT_E5B9G9R9_UFLOAT_PACK32, {4, 3}},
        {VK_FORMAT_D16_UNORM, {2, 1}},
        {VK_FORMAT_X8_D24_UNORM_PACK32, {4, 1}},
        {VK_FORMAT_D32_SFLOAT, {4, 1}},
        {VK_FORMAT_S8_UINT, {1, 1}},
        {VK_FORMAT_D16_UNORM_S8_UINT, {3, 2}},
        {VK_FORMAT_D24_UNORM_S8_UINT, {4, 2}},
        {VK_FORMAT_D32_SFLOAT_S8_UINT, {8, 2}},
        {VK_FORMAT_BC1_RGB_UNORM_BLOCK, {8, 4}},
        {VK_FORMAT_BC1_RGB_SRGB_BLOCK, {8, 4}},
        {VK_FORMAT_BC1_RGBA_UNORM_BLOCK, {8, 4}},
        {VK_FORMAT_BC1_RGBA_SRGB_BLOCK, {8, 4}},
        {VK_FORMAT_BC2_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_BC2_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_BC3_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_BC3_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_BC4_UNORM_BLOCK, {8, 4}},
        {VK_FORMAT_BC4_SNORM_BLOCK, {8, 4}},
        {VK_FORMAT_BC5_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_BC5_SNORM_BLOCK, {16, 4}},
        {VK_FORMAT_BC6H_UFLOAT_BLOCK, {16, 4}},
        {VK_FORMAT_BC6H_SFLOAT_BLOCK, {16, 4}},
        {VK_FORMAT_BC7_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_BC7_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK, {8, 3}},
        {VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK, {8, 3}},
        {VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK, {8, 4}},
        {VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK, {8, 4}},
        {VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_EAC_R11_UNORM_BLOCK, {8, 1}},
        {VK_FORMAT_EAC_R11_SNORM_BLOCK, {8, 1}},
        {VK_FORMAT_EAC_R11G11_UNORM_BLOCK, {16, 2}},
        {VK_FORMAT_EAC_R11G11_SNORM_BLOCK, {16, 2}},
        {VK_FORMAT_ASTC_4x4_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_4x4_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_5x4_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_5x4_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_5x5_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_5x5_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_6x5_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_6x5_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_6x6_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_6x6_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_8x5_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_8x5_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_8x6_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_8x6_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_8x8_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_8x8_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_10x5_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_10x5_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_10x6_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_10x6_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_10x8_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_10x8_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_10x10_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_10x10_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_12x10_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_12x10_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_12x12_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_12x12_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG, {8, 4}},
        {VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG, {8, 4}},
        {VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG, {8, 4}},
        {VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG, {8, 4}},
        {VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG, {8, 4}},
        {VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG, {8, 4}},
        {VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG, {8, 4}},
        {VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG, {8, 4}},
    };

    return vk_format_info.at(format);
}

VkExtent3D GetFormatBlockSize(VkFormat format) {
    VkExtent3D block_size = {1, 1, 1};
    switch (format) {
        case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
        case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
        case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
        case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
        case VK_FORMAT_BC2_UNORM_BLOCK:
        case VK_FORMAT_BC2_SRGB_BLOCK:
        case VK_FORMAT_BC3_UNORM_BLOCK:
        case VK_FORMAT_BC3_SRGB_BLOCK:
        case VK_FORMAT_BC4_UNORM_BLOCK:
        case VK_FORMAT_BC4_SNORM_BLOCK:
        case VK_FORMAT_BC5_UNORM_BLOCK:
        case VK_FORMAT_BC5_SNORM_BLOCK:
        case VK_FORMAT_BC6H_UFLOAT_BLOCK:
        case VK_FORMAT_BC6H_SFLOAT_BLOCK:
        case VK_FORMAT_BC7_UNORM_BLOCK:
        case VK_FORMAT_BC7_SRGB_BLOCK:
        case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
        case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
        case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
        case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
        case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
        case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
        case VK_FORMAT_EAC_R11_UNORM_BLOCK:
        case VK_FORMAT_EAC_R11_SNORM_BLOCK:
        case VK_FORMAT_EAC_R11G11_UNORM_BLOCK:
        case VK_FORMAT_EAC_R11G11_SNORM_BLOCK:
        case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
        case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
            block_size = {4, 4, 1};
            break;
        case VK_FORMAT_ASTC_5x4_UNORM_BLOCK:
        case VK_FORMAT_ASTC_5x4_SRGB_BLOCK:
            block_size = {5, 4, 1};
            break;
        case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:
        case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:
            block_size = {5, 5, 1};
            break;
        case VK_FORMAT_ASTC_6x5_UNORM_BLOCK:
        case VK_FORMAT_ASTC_6x5_SRGB_BLOCK:
            block_size = {6, 5, 1};
            break;
        case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:
        case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:
            block_size = {6, 6, 1};
            break;
        case VK_FORMAT_ASTC_8x5_UNORM_BLOCK:
        case VK_FORMAT_ASTC_8x5_SRGB_BLOCK:
            block_size = {8, 5, 1};
            break;
        case VK_FORMAT_ASTC_8x6_UNORM_BLOCK:
        case VK_FORMAT_ASTC_8x6_SRGB_BLOCK:
            block_size = {8, 6, 1};
            break;
        case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:
        case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:
            block_size = {8, 8, 1};
            break;
        case VK_FORMAT_ASTC_10x5_UNORM_BLOCK:
        case VK_FORMAT_ASTC_10x5_SRGB_BLOCK:
            block_size = {10, 5, 1};
            break;
        case VK_FORMAT_ASTC_10x6_UNORM_BLOCK:
        case VK_FORMAT_ASTC_10x6_SRGB_BLOCK:
            block_size = {10, 6, 1};
            break;
        case VK_FORMAT_ASTC_10x8_UNORM_BLOCK:
        case VK_FORMAT_ASTC_10x8_SRGB_BLOCK:
            block_size = {10, 8, 1};
            break;
        case VK_FORMAT_ASTC_10x10_UNORM_BLOCK:
        case VK_FORMAT_ASTC_10x10_SRGB_BLOCK:
            block_size = {10, 10, 1};
            break;
        case VK_FORMAT_ASTC_12x10_UNORM_BLOCK:
        case VK_FORMAT_ASTC_12x10_SRGB_BLOCK:
            block_size = {12, 10, 1};
            break;
        case VK_FORMAT_ASTC_12x12_UNORM_BLOCK:
        case VK_FORMAT_ASTC_12x12_SRGB_BLOCK:
            block_size = {12, 12, 1};
            break;
        case VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG:
        case VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG:
        case VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG:
        case VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG:
            block_size = {8, 4, 1};
            break;
        case VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG:
        case VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG:
        case VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG:
        case VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG:
            block_size = {4, 4, 1};
            break;
        // (KHR_sampler_ycbcr_conversion) _422 single-plane formats are treated
        // as 2x1 compressed (for copies)
        case VK_FORMAT_G8B8G8R8_422_UNORM:
        case VK_FORMAT_B8G8R8G8_422_UNORM:
        case VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16:
        case VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16:
        case VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16:
        case VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16:
        case VK_FORMAT_G16B16G16R16_422_UNORM:
        case VK_FORMAT_B16G16R16G16_422_UNORM:
            block_size = {2, 1, 1};
            break;
        // _422 multi-plane formats are not considered compressed, but shared
        // components form a logical 2x1 block
        case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM:
        case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM:
        case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16:
        case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:
        case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16:
        case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:
        case VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM:
        case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM:
            block_size = {2, 1, 1};
            break;
        // _420 formats are not considered compressed, but shared components
        // form a logical 2x2 block
        case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
        case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
        case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
        case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
        case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:
        case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
        case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM:
        case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM:
            block_size = {2, 2, 1};
            break;
        // _444 multi-plane formats do not share components, default to 1x1
        case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16:
        case VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM:
        case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16:
        case VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM:
        default:
            break;
    }
    return block_size;
}

FormatCompression GetFormatCompression(VkFormat format) {
    switch (format) {
        case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
        case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
        case VK_FORMAT_ASTC_5x4_UNORM_BLOCK:
        case VK_FORMAT_ASTC_5x4_SRGB_BLOCK:
        case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:
        case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:
        case VK_FORMAT_ASTC_6x5_UNORM_BLOCK:
        case VK_FORMAT_ASTC_6x5_SRGB_BLOCK:
        case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:
        case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:
        case VK_FORMAT_ASTC_8x5_UNORM_BLOCK:
        case VK_FORMAT_ASTC_8x5_SRGB_BLOCK:
        case VK_FORMAT_ASTC_8x6_UNORM_BLOCK:
        case VK_FORMAT_ASTC_8x6_SRGB_BLOCK:
        case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:
        case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:
        case VK_FORMAT_ASTC_10x5_UNORM_BLOCK:
        case VK_FORMAT_ASTC_10x5_SRGB_BLOCK:
        case VK_FORMAT_ASTC_10x6_UNORM_BLOCK:
        case VK_FORMAT_ASTC_10x6_SRGB_BLOCK:
        case VK_FORMAT_ASTC_10x8_UNORM_BLOCK:
        case VK_FORMAT_ASTC_10x8_SRGB_BLOCK:
        case VK_FORMAT_ASTC_10x10_UNORM_BLOCK:
        case VK_FORMAT_ASTC_10x10_SRGB_BLOCK:
        case VK_FORMAT_ASTC_12x10_UNORM_BLOCK:
        case VK_FORMAT_ASTC_12x10_SRGB_BLOCK:
        case VK_FORMAT_ASTC_12x12_UNORM_BLOCK:
        case VK_FORMAT_ASTC_12x12_SRGB_BLOCK:
            return FormatCompression::ASTC;
        case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
        case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
        case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
        case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
        case VK_FORMAT_BC2_UNORM_BLOCK:
        case VK_FORMAT_BC2_SRGB_BLOCK:
        case VK_FORMAT_BC3_UNORM_BLOCK:
        case VK_FORMAT_BC3_SRGB_BLOCK:
        case VK_FORMAT_BC4_UNORM_BLOCK:
        case VK_FORMAT_BC4_SNORM_BLOCK:
        case VK_FORMAT_BC5_UNORM_BLOCK:
        case VK_FORMAT_BC5_SNORM_BLOCK:
        case VK_FORMAT_BC6H_UFLOAT_BLOCK:
        case VK_FORMAT_BC6H_SFLOAT_BLOCK:
        case VK_FORMAT_BC7_UNORM_BLOCK:
        case VK_FORMAT_BC7_SRGB_BLOCK:
            return FormatCompression::BC;
        case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
        case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
        case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
        case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
        case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
        case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
        case VK_FORMAT_EAC_R11_UNORM_BLOCK:
        case VK_FORMAT_EAC_R11_SNORM_BLOCK:
        case VK_FORMAT_EAC_R11G11_UNORM_BLOCK:
        case VK_FORMAT_EAC_R11G11_SNORM_BLOCK:
            return FormatCompression::ETC2;
        case VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG:
        case VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG:
        case VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG:
        case VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG:
        case VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG:
        case VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG:
        case VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG:
        case VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG:
            return FormatCompression::PVRTC;
        default:
            return FormatCompression::Uncompressed;
    }
}

}  // namespace utils
}  // namespace goma

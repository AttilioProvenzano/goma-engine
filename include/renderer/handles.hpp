#pragma once

#define VK_NO_PROTOTYPES
#include "VEZ.h"

#include <array>
#include <vector>

namespace goma {

struct VulkanImage {
    VkImage image = VK_NULL_HANDLE;
    VkImageView image_view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
};

struct Image {
    const VulkanImage vez = {};

    Image(VulkanImage vez_) : vez(vez_) {}
};

struct Framebuffer {
    const VezFramebuffer vez = {};

    Framebuffer(VezFramebuffer vez_) : vez(vez_) {}
};

struct Pipeline {
    const VezPipeline vez = VK_NULL_HANDLE;

    Pipeline(VezPipeline vez_) : vez(vez_) {}
};

struct Buffer {
    const VkBuffer vez = VK_NULL_HANDLE;

    Buffer(VkBuffer vez_) : vez(vez_) {}
};

enum class Format {
    Undefined,
    SwapchainFormat,
    UnsignedNormRGBA,
    UnsignedNormBGRA,
};

enum class FilterType { Nearest, Linear };

enum class AddressingMode { Repeat, MirroredRepeat, ClampToEdge };

struct SamplerDesc {
    FilterType filter_type = FilterType::Nearest;
    FilterType mipmap_mode = FilterType::Nearest;

    float min_lod = 0.0f;
    float max_lod = 1.0f;
    float lod_bias = 0.0f;

    AddressingMode addressing_mode = AddressingMode::Repeat;
    float anisotropy = 0.0f;
};

struct TextureDesc {
    uint32_t width;
    uint32_t height;
    Format format = Format::UnsignedNormRGBA;

    uint32_t mip_levels = 1;
    uint32_t array_layers = 1;
    uint32_t samples = 1;

    SamplerDesc sampler = {};
};

struct FramebufferColorImageDesc {
    std::string name = "color";
    Format format = Format::SwapchainFormat;
    uint32_t samples = 1;

    SamplerDesc sampler = {};
};

enum class DepthImageType { None, Depth, DepthStencil };

struct FramebufferDepthImageDesc {
    std::string name = "depth";
    DepthImageType depth_type = DepthImageType::DepthStencil;
};

struct FramebufferDesc {
    uint32_t width;
    uint32_t height;
    std::vector<FramebufferColorImageDesc> color_images = {
        FramebufferColorImageDesc()};
    FramebufferDepthImageDesc depth_image = {};
};

struct ColorAttachmentDesc {
    bool clear = true;
    bool store = true;
    std::array<float, 4> clear_color = {0.1f, 0.1f, 0.1f, 1.0f};
};

struct DepthAttachmentDesc {
    bool active = true;
    bool clear = true;
    bool store = false;
    float clear_depth = 0.0f;
    uint32_t clear_stencil = 0;
};

struct RenderPassDesc {
    std::vector<ColorAttachmentDesc> color_attachments = {{}};
    DepthAttachmentDesc depth_attachment = {};
};

}  // namespace goma

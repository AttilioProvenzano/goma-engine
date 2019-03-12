#pragma once

#include "scene/attachments/material.hpp"

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
    bool valid = false;

    Image(VulkanImage vez_) : vez(vez_), valid(true) {}
};

struct Framebuffer {
    const VezFramebuffer vez = {};
    bool valid = false;

    Framebuffer(VezFramebuffer vez_) : vez(vez_), valid(true) {}
};

struct Pipeline {
    const VezPipeline vez = VK_NULL_HANDLE;
    bool valid = false;

    Pipeline(VezPipeline vez_) : vez(vez_), valid(true) {}
};

struct Buffer {
    const VkBuffer vez = VK_NULL_HANDLE;
    bool valid = false;

    Buffer(VkBuffer vez_) : vez(vez_), valid(true) {}
};

struct VertexInputFormat {
    const VezVertexInputFormat vez = VK_NULL_HANDLE;
    bool valid = false;

    VertexInputFormat(VezVertexInputFormat vez_) : vez(vez_), valid(true) {}
};

enum class Format {
    Undefined,
    SwapchainFormat,
    UNormRGBA8,
    UNormBGRA8,
    SFloatRGB32,
    SFloatRG32,
    SFloatR32,
};

enum class FilterType { Nearest, Linear };

struct SamplerDesc {
    FilterType filter_type = FilterType::Nearest;
    FilterType mipmap_mode = FilterType::Nearest;

    float min_lod = 0.0f;
    float max_lod = 1.0f;
    float lod_bias = 0.0f;

    TextureWrappingMode addressing_mode = TextureWrappingMode::Repeat;
    float anisotropy = 0.0f;
};

struct TextureDesc {
    uint32_t width;
    uint32_t height;
    Format format = Format::UNormRGBA8;

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
    float clear_depth = 1.0f;
    uint32_t clear_stencil = 0;
};

struct RenderPassDesc {
    std::vector<ColorAttachmentDesc> color_attachments = {{}};
    DepthAttachmentDesc depth_attachment = {};
};

struct VertexInputBindingDesc {
    uint32_t binding;
    uint32_t stride;
    bool per_instance = false;
};

struct VertexInputAttributeDesc {
    uint32_t location;
    uint32_t binding;
    Format format;
    uint32_t offset;
};

struct VertexInputFormatDesc {
    std::vector<VertexInputBindingDesc> bindings;
    std::vector<VertexInputAttributeDesc> attributes;
};

struct ShaderDesc {
    std::string source;
    bool is_filename = false;
    std::string preamble = "";
    std::string entry_point = "main";
};

}  // namespace goma

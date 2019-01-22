#pragma once

#define VK_NO_PROTOTYPES
#include "VEZ.h"

#include <vector>

namespace goma {

struct VulkanImage {
    VkImage image = VK_NULL_HANDLE;
    VkImageView image_view = VK_NULL_HANDLE;
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
    UnsignedNormRGBA,
};

struct TextureDesc {
    uint32_t width;
    uint32_t height;
    Format format;
    uint32_t mip_levels;
    uint32_t array_layers;
    uint32_t samples;

    TextureDesc(uint32_t width_ = 0, uint32_t height_ = 0,
                Format format_ = Format::UnsignedNormRGBA,
                uint32_t mip_levels_ = 1, uint32_t array_layers_ = 1,
                uint32_t samples_ = 1)
        : width(width_),
          height(height_),
          format(format_),
          mip_levels(mip_levels_),
          array_layers(array_layers_),
          samples(samples_) {}
};

struct FramebufferColorImageDesc {
    std::string name;
    Format format;
    uint32_t samples;

    FramebufferColorImageDesc(std::string name_,
                              Format format_ = Format::UnsignedNormRGBA,
                              uint32_t samples_ = 1)
        : name(std::move(name_)), format(format_), samples(samples_) {}
};

enum class DepthImageType { None, Depth, DepthStencil };

struct FramebufferDepthImageDesc {
    std::string name;
    DepthImageType depth_type;

    FramebufferDepthImageDesc(
        std::string name_,
        DepthImageType depth_type_ = DepthImageType::DepthStencil)
        : name(std::move(name_)), depth_type(depth_type_) {}
};

struct FramebufferDesc {
    uint32_t width;
    uint32_t height;
    std::vector<FramebufferColorImageDesc> color_images = {{"color"}};
    FramebufferDepthImageDesc depth_image = {"depth"};
};

struct ColorAttachmentDesc {
    bool clear = true;
    bool store = true;
    std::array<float, 4> clear_color = {0.1, 0.1, 0.1, 1};
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

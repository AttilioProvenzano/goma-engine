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
    std::vector<FramebufferColorImageDesc> color_images;
    FramebufferDepthImageDesc depth_image;

    FramebufferDesc(uint32_t width_, uint32_t height_,
                    const std::vector<FramebufferColorImageDesc>&
                        color_images_ = {{"color"}},
                    FramebufferDepthImageDesc depth_image_ = {"depth"})
        : width(width_),
          height(height_),
          color_images(color_images_),
          depth_image(depth_image_) {}
};

struct Pipeline {
    const VezPipeline vez = VK_NULL_HANDLE;

    Pipeline(VezPipeline vez_) : vez(vez_) {}
};

}  // namespace goma

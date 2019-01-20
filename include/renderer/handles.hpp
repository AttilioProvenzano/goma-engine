#pragma once

#define VK_NO_PROTOTYPES
#include "VEZ.h"

#include <vector>

namespace goma {

struct VulkanImage {
    VkImage image = VK_NULL_HANDLE;
    VkImageView image_view = VK_NULL_HANDLE;

    VulkanImage(VkImage image_ = VK_NULL_HANDLE,
                VkImageView image_view_ = VK_NULL_HANDLE)
        : image(image_), image_view(image_view_) {}
};

struct Image {
    VulkanImage vez = {};

    Image(VulkanImage vez_) : vez(vez_) {}
};

struct Pipeline {
    VezPipeline vez = VK_NULL_HANDLE;

    Pipeline(VezPipeline vez_) : vez(vez_) {}
};

}  // namespace goma

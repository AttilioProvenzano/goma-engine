#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>
#include <volk.h>

#include "common/error_codes.hpp"

#ifndef VK_CHECK
#define VK_CHECK(fn)                                                         \
    {                                                                        \
        VkResult _r = fn;                                                    \
        if (_r != VK_SUCCESS) {                                              \
            spdlog::error(                                                   \
                "{}, line {}: In function {}, a Vulkan error occurred when " \
                "running {}.",                                               \
                __FILE__, __LINE__, __func__, #fn);                          \
        };                                                                   \
        switch (_r) {                                                        \
            case VK_SUCCESS:                                                 \
                break;                                                       \
            case VK_ERROR_INITIALIZATION_FAILED:                             \
                return Error::VulkanInitializationFailed;                    \
            case VK_ERROR_OUT_OF_HOST_MEMORY:                                \
                return Error::OutOfCPUMemory;                                \
            case VK_ERROR_OUT_OF_DEVICE_MEMORY:                              \
                return Error::OutOfGPUMemory;                                \
            case VK_ERROR_LAYER_NOT_PRESENT:                                 \
                return Error::VulkanLayerNotPresent;                         \
            case VK_ERROR_EXTENSION_NOT_PRESENT:                             \
                return Error::VulkanExtensionNotPresent;                     \
            default:                                                         \
                return Error::GenericVulkanError;                            \
        }                                                                    \
    }
#endif
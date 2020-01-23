#pragma once

#include <string>
#include <system_error>

namespace goma {

enum class Error {
    Success = 0,
    OutOfCPUMemory = 1,
    OutOfGPUMemory = 2,
    DeviceLost = 3,
    VulkanLayerNotPresent = 4,
    VulkanExtensionNotPresent = 5,
    VulkanInitializationFailed = 6,
    GenericVulkanError = 7,
    GlfwError = 8,
    GlfwWindowCreationFailed = 9,
    NotFound = 10,
    InvalidNode = 11,
    InvalidParentNode = 12,
    RootNodeCannotBeDeleted = 13,
    SceneImportFailed = 14,
    DecompressionFailed = 15,
    LoadingFailed = 16,
    KeyAlreadyExists = 17,
    InvalidAttachment = 18,
    BufferCannotBeMapped = 19,
    NoSceneLoaded = 20,
    NoMainCamera = 21,
    ConfigNotSupported = 22,
    DimensionsNotMatching = 23,
    NoRenderPlan = 24,
};

}

namespace std {

// Tell the C++ 11 STL metaprogramming that our error code
// is registered with the standard error code system
template <>
struct is_error_code_enum<goma::Error> : std::true_type {};

}  // namespace std

namespace detail {
// Define a custom error code category derived from std::error_category
class GomaError_category : public std::error_category {
  public:
    // Return a short descriptive name for the category
    virtual const char *name() const noexcept override final {
        return "GomaError";
    }

    // Return what each enum means in text
    virtual std::string message(int c) const override final {
        switch (static_cast<goma::Error>(c)) {
            case goma::Error::Success:
                return "success";
            case goma::Error::OutOfCPUMemory:
                return "CPU is out of memory";
            case goma::Error::OutOfGPUMemory:
                return "GPU is out of memory";
            case goma::Error::DeviceLost:
                return "GPU failure (device lost)";
            case goma::Error::VulkanLayerNotPresent:
                return "a Vulkan layer was not found";
            case goma::Error::VulkanExtensionNotPresent:
                return "a Vulkan extension was not found";
            case goma::Error::VulkanInitializationFailed:
                return "Vulkan initialization failed";
            case goma::Error::GenericVulkanError:
                return "Vulkan error";
            case goma::Error::GlfwError:
                return "GLFW error";
            case goma::Error::GlfwWindowCreationFailed:
                return "GLFW could not create a window";
            case goma::Error::NotFound:
                return "element not found";
            case goma::Error::InvalidNode:
                return "node is invalid";
            case goma::Error::InvalidParentNode:
                return "parent node is invalid";
            case goma::Error::RootNodeCannotBeDeleted:
                return "root node cannot be deleted";
            case goma::Error::SceneImportFailed:
                return "scene could not be imported";
            case goma::Error::DecompressionFailed:
                return "decompression failed";
            case goma::Error::LoadingFailed:
                return "loading failed";
            case goma::Error::KeyAlreadyExists:
                return "the key already exists";
            case goma::Error::InvalidAttachment:
                return "invalid attachment";
            case goma::Error::BufferCannotBeMapped:
                return "buffer cannot be mapped";
            case goma::Error::NoSceneLoaded:
                return "no scene loaded";
            case goma::Error::NoMainCamera:
                return "no main camera";
            case goma::Error::ConfigNotSupported:
                return "configuration item not supported";
            case goma::Error::DimensionsNotMatching:
                return "dimensions not matching";
            case goma::Error::NoRenderPlan:
                return "no render plan";
            default:
                return "unknown error";
        }
    }
};
}  // namespace detail

// Declare a global function returning a static instance of the custom category
extern inline const detail::GomaError_category &GomaError_category() {
    static detail::GomaError_category c;
    return c;
}

namespace std {

// Overload the global make_error_code() free function with our
// custom enum. It will be found via ADL by the compiler if needed.
inline std::error_code make_error_code(goma::Error e) {
    return {static_cast<int>(e), GomaError_category()};
}

}  // namespace std

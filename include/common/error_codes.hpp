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

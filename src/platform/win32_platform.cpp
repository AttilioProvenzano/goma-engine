#include "platform/win32_platform.hpp"

#include "common/error_codes.hpp"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

namespace goma {

Win32Platform::~Win32Platform() {
    // glfwTerminate will destroy any remaining windows
    glfwTerminate();
}

result<void> Win32Platform::InitWindow() {
    if (!glfwInit()) {
        return Error::GlfwError;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window_ = glfwCreateWindow(800, 600, "Goma Engine", nullptr, nullptr);
    if (!window_) {
        glfwTerminate();
        return Error::GlfwWindowCreationFailed;
    }

    return outcome::success();
}

result<VkSurfaceKHR> Win32Platform::CreateVulkanSurface(
    VkInstance instance) const {
    assert(window_ && "Window must be initialized before creating surface");

    VkWin32SurfaceCreateInfoKHR surface_info = {};
    surface_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surface_info.hinstance = GetModuleHandle(NULL);
    surface_info.hwnd = glfwGetWin32Window(window_);

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    vkCreateWin32SurfaceKHR(instance, &surface_info, nullptr, &surface);
    return surface;
}

}  // namespace goma

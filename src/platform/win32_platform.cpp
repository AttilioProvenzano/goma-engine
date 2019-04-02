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
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window_ = glfwCreateWindow(800, 600, "Goma Engine", nullptr, nullptr);
    glfwSetInputMode(window_, GLFW_STICKY_KEYS, 1);
    glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
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

uint32_t Win32Platform::GetWidth() {
    int width;
    glfwGetWindowSize(window_, &width, nullptr);
    return static_cast<uint32_t>(width);
};

uint32_t Win32Platform::GetHeight() {
    int height;
    glfwGetWindowSize(window_, nullptr, &height);
    return static_cast<uint32_t>(height);
};

InputState Win32Platform::GetInputState() {
    // TODO move to a main loop with a lambda for the inner loop
    // main loop will be platform-specific
    glfwPollEvents();

    InputState state;

    static const std::unordered_map<int, KeyInput> glfw_to_key{
        {GLFW_KEY_UP, KeyInput::Up},     {GLFW_KEY_DOWN, KeyInput::Down},
        {GLFW_KEY_LEFT, KeyInput::Left}, {GLFW_KEY_RIGHT, KeyInput::Right},
        {GLFW_KEY_W, KeyInput::W},       {GLFW_KEY_A, KeyInput::A},
        {GLFW_KEY_S, KeyInput::S},       {GLFW_KEY_D, KeyInput::D},
    };

    for (const auto& entry : glfw_to_key) {
        if (glfwGetKey(window_, entry.first) == GLFW_PRESS) {
            state.keypresses.insert(entry.second);
        }
    }

    return state;
}

}  // namespace goma

#include "engine/engine.hpp"
#include "platform/win32_platform.hpp"

struct Application {
    std::unique_ptr<goma::Platform> platform;
    std::unique_ptr<goma::Engine> engine;
};

int main() {
    auto app = Application{};

    app.platform = std::make_unique<goma::Win32Platform>();
    app.engine = std::make_unique<goma::Engine>();
}
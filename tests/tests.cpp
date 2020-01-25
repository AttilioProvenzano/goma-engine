#include "gtest/gtest.h"

#include "platform/win32_platform.hpp"
#include "renderer/device.hpp"

using namespace goma;

#ifndef GOMA_ASSETS_DIR
#define GOMA_ASSETS_DIR "assets/"
#endif

namespace {

TEST(RendererTest, CanCreateDevice) {
    Win32Platform platform;
    platform.InitWindow();

    Device device;
    device.InitWindow(platform);
}

}  // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}

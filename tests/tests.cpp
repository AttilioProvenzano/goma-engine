#include "gtest/gtest.h"

#include "glslang/Public/ShaderLang.h"
#include "SPIRV/GlslangToSpv.h"

#include "platform/win32_platform.hpp"
#include "renderer/device.hpp"

using namespace goma;

#ifndef GOMA_ASSETS_DIR
#define GOMA_ASSETS_DIR "assets/"
#endif

namespace {

TEST(GlslangTest, CanCompileShader) {
    using namespace glslang;

    InitializeProcess();

    static const char* vtx = R"(
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUVs;

layout(location = 0) out vec2 outUVs;

void main() {
    gl_Position = vec4(inPosition, 1.0);
    outUVs = inUVs;
}
)";

    TShader vtx_shader{EShLangVertex};
    vtx_shader.setStrings(&vtx, 1);
    vtx_shader.setEnvInput(EShSourceGlsl, EShLangVertex, EShClientVulkan, 100);
    vtx_shader.setEnvClient(EShClientVulkan, EShTargetVulkan_1_1);
    vtx_shader.setEnvTarget(EShTargetSpv, EShTargetSpv_1_3);

    TBuiltInResource builtin_resources = {};
    vtx_shader.parse(&builtin_resources, 0, false, EShMsgDefault);

    auto info_log = vtx_shader.getInfoLog();
    spdlog::info("Shader compilation log: {}", info_log);

    TProgram program;
    program.addShader(&vtx_shader);
    program.link(EShMsgDefault);

    info_log = program.getInfoLog();
    spdlog::info("Shader linking log: {}", info_log);

    std::vector<unsigned int> spirv;
    spv::SpvBuildLogger logger;
    SpvOptions spvOptions = {};
    GlslangToSpv(*program.getIntermediate(EShLangVertex), spirv, &logger,
                 &spvOptions);
    spdlog::info("SPIR-V conversion log: {}", logger.getAllMessages());

    FinalizeProcess();
    ASSERT_GT(spirv.size(), 0U);
}

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

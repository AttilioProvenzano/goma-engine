#include "gtest/gtest.h"

#include <glslang/Public/ShaderLang.h>
#include <SPIRV/GlslangToSpv.h>
#include <spirv_glsl.hpp>

#include "platform/win32_platform.hpp"
#include "renderer/context.hpp"
#include "renderer/device.hpp"

using namespace goma;

#ifndef GOMA_ASSETS_DIR
#define GOMA_ASSETS_DIR "assets/"
#endif

namespace {

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

struct Vertex {
    glm::vec3 pos;
    glm::vec2 uv;
};

std::vector<Vertex> buf_data = {{{1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
                                {{0.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}};

constexpr int kWindowWidth = 1024;
constexpr int kWindowHeight = 768;

TEST(GlslangTest, CanCompileShader) {
    using namespace glslang;

    InitializeProcess();

    TShader vtx_shader{EShLangVertex};
    vtx_shader.setStrings(&vtx, 1);
    vtx_shader.setEnvInput(EShSourceGlsl, EShLangVertex, EShClientVulkan, 100);
    vtx_shader.setEnvClient(EShClientVulkan, EShTargetVulkan_1_1);
    vtx_shader.setEnvTarget(EShTargetSpv, EShTargetSpv_1_0);

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

    // SPIRV-Cross reflection
    spirv_cross::CompilerGLSL glsl(std::move(spirv));
    spirv_cross::ShaderResources resources = glsl.get_shader_resources();

    auto& uv_resource = resources.stage_inputs[1];
    ASSERT_EQ(uv_resource.name, "inUVs");

    auto uv_type = glsl.get_type(uv_resource.base_type_id);
    ASSERT_EQ(uv_type.basetype, spirv_cross::SPIRType::Float);
    ASSERT_EQ(uv_type.vecsize, 2);
}

class RendererTest : public ::testing::Test {
  protected:
    std::unique_ptr<Device> device;

    virtual void SetUp() override {
        try {
            device = std::make_unique<Device>();
        } catch (const std::exception& ex) {
            std::cerr << "RendererTest exception: " << ex.what() << std::endl;
            GTEST_SKIP();
        }
    }
};

TEST_F(RendererTest, CanCreateDevice) {
    ASSERT_NE(device->GetHandle(), VkDevice{VK_NULL_HANDLE});
}

TEST_F(RendererTest, CanCreateCPUBuffer) {
    std::vector<glm::vec3> colors = {
        {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}};

    BufferDesc desc = {};
    desc.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    desc.num_elements = colors.size();
    desc.stride = sizeof(colors[0]);
    desc.size = desc.num_elements * desc.stride;
    desc.storage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    auto buffer_res = device->CreateBuffer(desc);
    ASSERT_FALSE(buffer_res.has_error()) << buffer_res.error();
    auto buffer = buffer_res.value();

    auto map_res = device->MapBuffer(buffer);
    ASSERT_FALSE(map_res.has_error()) << map_res.error();
    auto data = map_res.value();

    memcpy(data, colors.data(), sizeof(colors[0]) * colors.size());
    ASSERT_EQ(*static_cast<float*>(data), 1.0f);
    device->UnmapBuffer(buffer);
}

TEST_F(RendererTest, CanCreateGPUBuffer) {
    BufferDesc desc = {};
    desc.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    desc.num_elements = 100;
    desc.stride = 16;
    desc.size = desc.num_elements * desc.stride;
    desc.storage = VMA_MEMORY_USAGE_GPU_ONLY;

    device->CreateBuffer(desc);
}

TEST_F(RendererTest, CanCreateImage) {
    // ImageDesc desc = {};
    // device->CreateImage();
    GTEST_SKIP();
}

TEST_F(RendererTest, CanCreateShaderAndPipeline) {
    ShaderDesc shader_desc = {};
    shader_desc.name = "vtx";
    shader_desc.stage = VK_SHADER_STAGE_VERTEX_BIT;
    shader_desc.source = vtx;

    auto shader_res = device->CreateShader(std::move(shader_desc));
    EXPECT_FALSE(shader_res.has_error())
        << "Shader error: " << shader_res.error().message();

    auto shader = shader_res.value();
    ASSERT_NE(shader->GetHandle(), VkShaderModule{VK_NULL_HANDLE});

    auto& uv_resource = shader->GetInputs().at(1);
    ASSERT_EQ(uv_resource.name, "inUVs");

    auto pipeline_res = device->CreatePipeline({{shader}}, FramebufferDesc{});
    EXPECT_FALSE(pipeline_res.has_error())
        << "Pipeline error: " << pipeline_res.error().message();

    auto pipeline = pipeline_res.value();
    ASSERT_NE(pipeline->GetHandle(), VkPipeline{VK_NULL_HANDLE});
}

TEST_F(RendererTest, CanCreateGraphicsContext) {
    GraphicsContext context(*device);
    context.Begin();
    context.End();
}

class RendererGraphicalTest : public ::testing::Test {
  protected:
    std::unique_ptr<Device> device;
    std::unique_ptr<Platform> platform;

    void SetUp() override {
        try {
            device = std::make_unique<Device>();
            platform = std::make_unique<Win32Platform>();
            auto res = platform->InitWindow(kWindowWidth, kWindowHeight);

            if (res.has_error()) {
                std::cerr << "Window initialization failed: "
                          << res.error().message() << std::endl;
            } else {
                device->InitWindow(*platform);
            }
        } catch (const std::exception& ex) {
            std::cerr << "RendererGraphicalTest exception: " << ex.what()
                      << std::endl;
            GTEST_SKIP();
        }
    }
};

TEST_F(RendererGraphicalTest, CanCreateWindow) {
    ASSERT_NE(device->GetHandle(), VkDevice{VK_NULL_HANDLE});
}

// TEST_F(RendererTest, CanCreatePipeline) {}
// TEST_F(RendererTest, SpinningCube) {}
// TEST_F(RendererTest, OffscreenRendering) {}
// TEST_F(RendererTest, Screenshot) {}
// TEST_F(RendererTest, GUI) {}

}  // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}

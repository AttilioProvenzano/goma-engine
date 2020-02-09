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

#ifndef GOMA_TEST_TRY
#define GOMA_TEST_TRY(HANDLE, FN)                                             \
    auto HANDLE##_res = FN;                                                   \
    ASSERT_FALSE(HANDLE##_res.has_error()) << HANDLE##_res.error().message(); \
    auto& HANDLE = HANDLE##_res.value();
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

result<Buffer*> CreateBufferTest(Device& device, VmaMemoryUsage storage) {
    BufferDesc desc = {};
    desc.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    desc.num_elements = static_cast<uint32_t>(buf_data.size());
    desc.stride = sizeof(buf_data[0]);
    desc.size = buf_data.size() * sizeof(buf_data[0]);
    desc.storage = storage;

    OUTCOME_TRY(buffer, device.CreateBuffer(desc));

    UploadContext ctx(device);
    OUTCOME_TRY(ctx.Begin());
    OUTCOME_TRY(ctx.UploadBuffer(
        *buffer, {buf_data.size() * sizeof(buf_data[0]), buf_data.data()}));
    ctx.End();

    OUTCOME_TRY(receipt, device.Submit(ctx));
    OUTCOME_TRY(device.WaitOnWork(std::move(receipt)));

    return buffer;
}

TEST_F(RendererTest, CanCreateCPUBuffer) {
    auto buffer_res = CreateBufferTest(*device, VMA_MEMORY_USAGE_CPU_TO_GPU);
    ASSERT_FALSE(buffer_res.has_error()) << buffer_res.error().message();
    auto& buffer = buffer_res.value();

    // Let's check that the data was properly copied.
    auto data_res = device->MapBuffer(*buffer);
    ASSERT_FALSE(data_res.has_error()) << data_res.error().message();
    auto& data = data_res.value();

    ASSERT_EQ(*static_cast<float*>(data), 1.0f);
    device->UnmapBuffer(*buffer);
}

TEST_F(RendererTest, CanCreateGPUBuffer) {
    auto buffer_res = CreateBufferTest(*device, VMA_MEMORY_USAGE_GPU_ONLY);
    ASSERT_FALSE(buffer_res.has_error()) << buffer_res.error().message();
}

TEST_F(RendererTest, CanCreateImage) {
    auto desc = ImageDesc::ColorAttachmentDesc;
    desc.size = {800, 600, 1};

    GOMA_TEST_TRY(image, device->CreateImage(desc));

    ASSERT_NE(image->GetHandle(), VkImage{VK_NULL_HANDLE});
    ASSERT_NE(image->GetView(), VkImageView{VK_NULL_HANDLE});
    ASSERT_NE(image->GetAllocation().allocation, VmaAllocation{VK_NULL_HANDLE});

    // TODO: upload data
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
    auto res = context.Begin();
    ASSERT_FALSE(res.has_error()) << res.error().message();
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
            ASSERT_FALSE(res.has_error()) << res.error().message();

            res = device->InitWindow(*platform);
            ASSERT_FALSE(res.has_error()) << res.error().message();
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

TEST_F(RendererGraphicalTest, HelloTriangle) {
    ShaderDesc shader_desc = {};
    shader_desc.name = "vtx";
    shader_desc.stage = VK_SHADER_STAGE_VERTEX_BIT;
    shader_desc.source = vtx;

    auto shader_res = device->CreateShader(std::move(shader_desc));
    EXPECT_FALSE(shader_res.has_error())
        << "Shader error: " << shader_res.error().message();
    auto shader = shader_res.value();

    // TODO get swapchain image
    Image* swapchain_image = nullptr;

    FramebufferDesc fb_desc = {};
    // fb_desc.color_attachments.push_back({swapchain_image});

    auto pipeline_res = device->CreatePipeline({{shader}}, fb_desc);
    ASSERT_FALSE(pipeline_res.has_error())
        << "Pipeline error: " << pipeline_res.error().message();
    auto pipeline = pipeline_res.value();

    auto res = [&]() -> result<void> {
        GraphicsContext context(*device);

        OUTCOME_TRY(context.Begin());
        OUTCOME_TRY(context.BindFramebuffer(fb_desc));

        /*
        context.BindPipeline(pipeline);
        context.Draw();
        */

        context.End();

        return outcome::success();
    }();
    ASSERT_FALSE(res.has_error()) << res.error().message();
}

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

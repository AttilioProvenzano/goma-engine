#include "gtest/gtest.h"

#include <glslang/Public/ShaderLang.h>
#include <SPIRV/GlslangToSpv.h>
#include <spirv_glsl.hpp>
#include <stb/stb_image.h>

#include "platform/win32_platform.hpp"
#include "renderer/context.hpp"
#include "renderer/device.hpp"
#include "renderer/vulkan/utils.hpp"

using namespace goma;

#ifndef GOMA_ASSETS_DIR
#define GOMA_ASSETS_DIR "assets/"
#endif

#ifndef GOMA_TEST_TRY
#define GOMA_TEST_TRYV(fn)                                      \
    {                                                           \
        auto res = fn;                                          \
        ASSERT_FALSE(res.has_error()) << res.error().message(); \
    }

#define GOMA_TEST_TRY(handle, fn)                                             \
    auto handle##_res = fn;                                                   \
    ASSERT_FALSE(handle##_res.has_error()) << handle##_res.error().message(); \
    auto& handle = handle##_res.value();
#endif

namespace {

static const char* triangle_vtx = R"(
#version 450

layout(location = 0) out vec3 outColor;

vec3 triangle[3] = vec3[](
    vec3(1.0, -1.0, 0.0),
    vec3(-1.0, 1.0, 0.0),
    vec3(1.0, 1.0, 0.0)
);

void main() {
    gl_Position = vec4(triangle[gl_VertexIndex], 1.0);

    outColor = vec3(0.0, 0.0, 0.0);
    outColor[gl_VertexIndex] = 1.0;
}
)";

static const char* triangle_frag = R"(
#version 450

layout(location = 0) in vec3 inColor;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(inColor, 1.0);
}
)";

static const char* vtx = R"(
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUVs;

layout(location = 0) out vec3 outColor;
layout(location = 1) out vec2 outUVs;

layout(binding = 0, std140) uniform UBO {
    mat4 mvp;
} ubo;

void main() {
    gl_Position = ubo.mvp * vec4(inPosition, 1.0);
    outUVs = inUVs;
    outColor = inColor;
}
)";

static const char* frag = R"(
#version 450

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec2 inUVs;

layout(location = 0) out vec4 outColor;

#ifdef HAS_DIFFUSE_TEXTURE
layout(binding = 1) uniform texture2D diffuseTex;
layout(binding = 2) uniform sampler linearSampler;
#endif

void main() {
#ifdef HAS_DIFFUSE_TEXTURE
    vec4 albedo = texture(sampler2D(diffuseTex, linearSampler), inUVs);
    outColor = mix(albedo, vec4(inColor, 1.0), 0.5);
#else
    outColor = vec4(inColor, 1.0);
#endif
}
)";

using Color = glm::vec3;

struct Vertex {
    glm::vec3 pos;
    Color color;
    glm::vec2 uv;
};

const glm::vec3 kFrontTopLeft = {-1.0f, 1.0f, -1.0f};
const glm::vec3 kFrontBottomLeft = {-1.0f, -1.0f, -1.0f};
const glm::vec3 kFrontTopRight = {1.0f, 1.0f, -1.0f};
const glm::vec3 kFrontBottomRight = {1.0f, -1.0f, -1.0f};
const glm::vec3 kBackTopLeft = {-1.0f, 1.0f, 1.0f};
const glm::vec3 kBackBottomLeft = {-1.0f, -1.0f, 1.0f};
const glm::vec3 kBackTopRight = {1.0f, 1.0f, 1.0f};
const glm::vec3 kBackBottomRight = {1.0f, -1.0f, 1.0f};

const Color kRed = {1.0f, 0.0f, 0.0f};
const Color kBlue = {0.0f, 0.0f, 1.0f};
const Color kGreen = {0.0f, 1.0f, 0.0f};
const Color kYellow = {1.0f, 1.0f, 0.0f};
const Color kWhite = {1.0f, 1.0f, 1.0f};
const Color kPurple = {1.0f, 0.0f, 1.0f};

std::vector<Vertex> cube_vtx_data = {
    // Front
    {kFrontTopLeft, kRed, {0.0f, 0.0f}},
    {kFrontTopRight, kRed, {1.0f, 0.0f}},
    {kFrontBottomLeft, kRed, {0.0f, 1.0f}},
    {kFrontBottomRight, kRed, {1.0f, 1.0f}},

    // Back
    {kBackTopLeft, kGreen, {1.0f, 0.0f}},
    {kBackTopRight, kGreen, {0.0f, 0.0f}},
    {kBackBottomLeft, kGreen, {1.0f, 1.0f}},
    {kBackBottomRight, kGreen, {0.0f, 1.0f}},

    // Left
    {kFrontTopLeft, kBlue, {1.0f, 0.0f}},
    {kFrontBottomLeft, kBlue, {1.0f, 1.0f}},
    {kBackTopLeft, kBlue, {0.0f, 0.0f}},
    {kBackBottomLeft, kBlue, {0.0f, 1.0f}},

    // Right
    {kFrontTopRight, kYellow, {0.0f, 0.0f}},
    {kFrontBottomRight, kYellow, {0.0f, 1.0f}},
    {kBackTopRight, kYellow, {1.0f, 0.0f}},
    {kBackBottomRight, kYellow, {1.0f, 1.0f}},

    // Top
    {kFrontTopLeft, kWhite, {0.0f, 1.0f}},
    {kFrontTopRight, kWhite, {1.0f, 1.0f}},
    {kBackTopLeft, kWhite, {0.0f, 0.0f}},
    {kBackTopRight, kWhite, {1.0f, 0.0f}},

    // Bottom
    {kFrontBottomLeft, kPurple, {1.0f, 1.0f}},
    {kFrontBottomRight, kPurple, {0.0f, 1.0f}},
    {kBackBottomLeft, kPurple, {1.0f, 0.0f}},
    {kBackBottomRight, kPurple, {0.0f, 0.0f}},
};

std::vector<uint32_t> cube_index_data = {
    0,  2,  3,  3,  1,  0,   // Front
    4,  5,  6,  5,  7,  6,   // Back
    8,  10, 11, 11, 9,  8,   // Left
    12, 13, 14, 15, 14, 13,  // Right
    16, 17, 18, 18, 17, 19,  // Top
    20, 22, 21, 22, 23, 21,  // Bottom
};

constexpr int kWindowWidth = 1024;
constexpr int kWindowHeight = 768;
constexpr int kTimeoutSeconds = 2;

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
    if (strlen(info_log) > 0) {
        spdlog::info("Shader compilation log: {}", info_log);
    }

    TProgram program;
    program.addShader(&vtx_shader);
    program.link(EShMsgDefault);

    info_log = program.getInfoLog();
    if (strlen(info_log) > 0) {
        spdlog::info("Shader linking log: {}", info_log);
    }

    std::vector<unsigned int> spirv;
    spv::SpvBuildLogger logger;
    SpvOptions spvOptions = {};
    GlslangToSpv(*program.getIntermediate(EShLangVertex), spirv, &logger,
                 &spvOptions);
    if (!logger.getAllMessages().empty()) {
        spdlog::info("SPIR-V conversion log: {}", logger.getAllMessages());
    }

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
    desc.num_elements = static_cast<uint32_t>(cube_vtx_data.size());
    desc.stride = sizeof(cube_vtx_data[0]);
    desc.size = cube_vtx_data.size() * sizeof(cube_vtx_data[0]);
    desc.storage = storage;

    OUTCOME_TRY(buffer, device.CreateBuffer(desc));

    UploadContext ctx(device);
    OUTCOME_TRY(ctx.Begin());
    OUTCOME_TRY(ctx.UploadBuffer(
        *buffer, {cube_vtx_data.size() * sizeof(cube_vtx_data[0]),
                  cube_vtx_data.data()}));
    ctx.End();

    OUTCOME_TRY(receipt, device.Submit(ctx));
    OUTCOME_TRY(device.WaitOnWork(std::move(receipt)));

    return buffer;
}

TEST_F(RendererTest, CanCreateCPUBuffer) {
    GOMA_TEST_TRY(buffer,
                  CreateBufferTest(*device, VMA_MEMORY_USAGE_CPU_TO_GPU));

    // Let's check that the data was properly copied.
    GOMA_TEST_TRY(data, device->MapBuffer(*buffer));
    auto buf_data = static_cast<Vertex*>(data);
    ASSERT_EQ(buf_data[2].pos[2], cube_vtx_data[2].pos[2]);
    ASSERT_EQ(buf_data[5].color[1], cube_vtx_data[5].color[1]);
    device->UnmapBuffer(*buffer);
}

TEST_F(RendererTest, CanCreateGPUBuffer) {
    GOMA_TEST_TRY(buffer, CreateBufferTest(*device, VMA_MEMORY_USAGE_GPU_ONLY));
}

TEST_F(RendererTest, CanCreateColorAttachment) {
    auto desc = ImageDesc::ColorAttachmentDesc;
    desc.size = {800, 600, 1};

    GOMA_TEST_TRY(image, device->CreateImage(desc));

    ASSERT_NE(image->GetHandle(), VkImage{VK_NULL_HANDLE});
    ASSERT_NE(image->GetView(), VkImageView{VK_NULL_HANDLE});
    ASSERT_NE(image->GetAllocation().allocation, VmaAllocation{VK_NULL_HANDLE});
}

TEST_F(RendererTest, CanCreateTexture) {
    auto width = 64U;
    auto height = 64U;

    auto desc = ImageDesc::TextureDesc;
    desc.size = {width, height, 1};
    desc.mip_levels = utils::ComputeMipLevels(width, height);

    std::vector<uint8_t> image_data(width * height *
                                    utils::GetFormatInfo(desc.format).size);
    std::generate(image_data.begin(), image_data.end(),
                  [n = 0, size = 4 * width * height]() mutable {
                      auto mix = 0xFF * n / size;
                      auto ch = n++ % 4;
                      switch (ch) {
                          case 0:
                              return mix;
                          case 1:
                              return 0x00U;
                          case 2:
                              return 0xFFU - mix;
                          case 3:
                              return 0xFFU;
                      }
                      return 0x00U;
                  });

    GOMA_TEST_TRY(image, device->CreateImage(desc));

    ASSERT_NE(image->GetHandle(), VkImage{VK_NULL_HANDLE});
    ASSERT_NE(image->GetView(), VkImageView{VK_NULL_HANDLE});
    ASSERT_NE(image->GetAllocation().allocation, VmaAllocation{VK_NULL_HANDLE});

    UploadContext ctx(*device);
    GOMA_TEST_TRYV(ctx.Begin());

    ImageData d = {{{0, image_data.data()}}};
    GOMA_TEST_TRYV(ctx.UploadImage(*image, std::move(d)));
    ctx.GenerateMipmaps(*image);
    ctx.End();

    GOMA_TEST_TRY(receipt, device->Submit(ctx));
    GOMA_TEST_TRYV(device->WaitOnWork(std::move(receipt)));
}

TEST_F(RendererTest, FramebufferCompatibility) {
    auto desc = ImageDesc::ColorAttachmentDesc;
    desc.size = {64, 64, 1};
    GOMA_TEST_TRY(image, device->CreateImage(desc));
    GOMA_TEST_TRY(other_image, device->CreateImage(desc));

    desc.samples = VK_SAMPLE_COUNT_4_BIT;
    GOMA_TEST_TRY(ms_image, device->CreateImage(desc));

    FramebufferDesc fb_1 = {};
    FramebufferDesc fb_2 = {};
    FramebufferDesc fb_3 = {};

    fb_1.color_attachments = {{image}, {other_image}};
    fb_2.color_attachments = {{other_image}, {image}};
    fb_3.color_attachments = {{image}, {ms_image}};

    ASSERT_TRUE(IsCompatible(fb_1, fb_2));
    ASSERT_FALSE(IsCompatible(fb_1, fb_3));
}

TEST_F(RendererTest, CanCreateShaderAndPipeline) {
    ShaderDesc shader_desc = {};
    shader_desc.name = "vtx";
    shader_desc.stage = VK_SHADER_STAGE_VERTEX_BIT;
    shader_desc.source = vtx;

    GOMA_TEST_TRY(shader, device->CreateShader(std::move(shader_desc)));
    ASSERT_NE(shader->GetHandle(), VkShaderModule{VK_NULL_HANDLE});

    auto& uv_resource = shader->GetInputs().at(2);
    ASSERT_EQ(uv_resource.name, "inUVs");

    auto& uniform_buffer = shader->GetBindings().at(0);
    ASSERT_EQ(uniform_buffer.name, "UBO");

    GOMA_TEST_TRY(pipeline,
                  device->CreatePipeline({{shader}, FramebufferDesc{}}));
    ASSERT_NE(pipeline->GetHandle(), VkPipeline{VK_NULL_HANDLE});
}

TEST_F(RendererTest, CanBindDescriptorSet) {
    // Create a buffer
    BufferDesc desc = {};
    desc.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    desc.num_elements = static_cast<uint32_t>(cube_vtx_data.size());
    desc.stride = sizeof(cube_vtx_data[0]);
    desc.size = cube_vtx_data.size() * sizeof(cube_vtx_data[0]);
    desc.storage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    GOMA_TEST_TRY(buffer, device->CreateBuffer(desc));

    UploadContext ctx(*device);
    GOMA_TEST_TRYV(ctx.Begin());
    GOMA_TEST_TRYV(ctx.UploadBuffer(
        *buffer, {cube_vtx_data.size() * sizeof(cube_vtx_data[0]),
                  cube_vtx_data.data()}));
    ctx.End();

    GOMA_TEST_TRY(receipt, device->Submit(ctx));
    GOMA_TEST_TRYV(device->WaitOnWork(std::move(receipt)));

    // Create a pipeline
    ShaderDesc shader_desc = {};
    shader_desc.name = "vtx";
    shader_desc.stage = VK_SHADER_STAGE_VERTEX_BIT;
    shader_desc.source = vtx;

    GOMA_TEST_TRY(shader, device->CreateShader(std::move(shader_desc)));
    GOMA_TEST_TRY(pipeline,
                  device->CreatePipeline({{shader}, FramebufferDesc{}}));

    GraphicsContext context(*device);
    GOMA_TEST_TRYV(context.Begin());
    context.BindGraphicsPipeline(*pipeline);
    context.BindDescriptorSet({{0, *buffer}});
    context.End();
}

TEST_F(RendererTest, CanCreateGraphicsContext) {
    GraphicsContext context(*device);
    GOMA_TEST_TRYV(context.Begin());
    context.End();
}

void HelloTriangle(Device* device, bool offscreen = false) {
    ShaderDesc vtx_desc = {"triangle_vtx", VK_SHADER_STAGE_VERTEX_BIT,
                           triangle_vtx};
    ShaderDesc frag_desc = {"triangle_frag", VK_SHADER_STAGE_FRAGMENT_BIT,
                            triangle_frag};

    GOMA_TEST_TRY(vtx, device->CreateShader(std::move(vtx_desc)));
    GOMA_TEST_TRY(frag, device->CreateShader(std::move(frag_desc)));

    Image* render_target = nullptr;

    if (!offscreen) {
        GOMA_TEST_TRY(swapchain_image, device->AcquireSwapchainImage());
        render_target = swapchain_image;
    } else {
        auto desc = ImageDesc::ColorAttachmentDesc;
        desc.size = {800, 600, 1};
        GOMA_TEST_TRY(image, device->CreateImage(desc));
        render_target = image;
    }

    FramebufferDesc fb_desc = {};
    fb_desc.color_attachments.push_back({render_target});

    GOMA_TEST_TRY(pipeline, device->CreatePipeline({{vtx, frag}, fb_desc}));

    GraphicsContext context(*device);

    GOMA_TEST_TRYV(context.Begin());
    GOMA_TEST_TRYV(context.BindFramebuffer(fb_desc));

    context.BindGraphicsPipeline(*pipeline);
    context.Draw(3);

    context.End();

    GOMA_TEST_TRY(receipt, device->Submit(context));

    if (!offscreen) {
        GOMA_TEST_TRYV(device->Present());
    }

    GOMA_TEST_TRYV(device->WaitOnWork(std::move(receipt)));
}

TEST_F(RendererTest, HelloTriangleOffscreen) {
    HelloTriangle(device.get(), true);
}

class RendererGraphicalTest : public ::testing::Test {
  protected:
    std::unique_ptr<Device> device;
    std::unique_ptr<Platform> platform;

    void SetUp() override {
        try {
            device = std::make_unique<Device>();
            platform = std::make_unique<Win32Platform>();

            GOMA_TEST_TRYV(platform->InitWindow(kWindowWidth, kWindowHeight));
            GOMA_TEST_TRYV(device->InitWindow(*platform));
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
    HelloTriangle(device.get());
    Sleep(kTimeoutSeconds * 1000);
}

void SpinningCube(Device* device, Platform* platform, bool textured = false) {
    BufferDesc desc = {};
    desc.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    desc.num_elements = static_cast<uint32_t>(cube_vtx_data.size());
    desc.stride = sizeof(cube_vtx_data[0]);
    desc.size = cube_vtx_data.size() * sizeof(cube_vtx_data[0]);
    desc.storage = VMA_MEMORY_USAGE_GPU_ONLY;
    GOMA_TEST_TRY(vtx_buf, device->CreateBuffer(desc));

    desc.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    desc.num_elements = static_cast<uint32_t>(cube_index_data.size());
    desc.stride = sizeof(cube_index_data[0]);
    desc.size = cube_index_data.size() * sizeof(cube_index_data[0]);
    desc.storage = VMA_MEMORY_USAGE_GPU_ONLY;
    GOMA_TEST_TRY(index_buf, device->CreateBuffer(desc));

    UploadContext upload_ctx(*device);
    GOMA_TEST_TRYV(upload_ctx.Begin());
    GOMA_TEST_TRYV(upload_ctx.UploadBuffer(
        *vtx_buf, {cube_vtx_data.size() * sizeof(cube_vtx_data[0]),
                   cube_vtx_data.data()}));
    GOMA_TEST_TRYV(upload_ctx.UploadBuffer(
        *index_buf, {cube_index_data.size() * sizeof(cube_index_data[0]),
                     cube_index_data.data()}));

    Image* diffuse_tex = nullptr;
    Sampler* sampler = nullptr;
    if (textured) {
        int x, y, channels;
        auto tex_data = stbi_load(GOMA_ASSETS_DIR "models/Duck/glTF/DuckCM.png",
                                  &x, &y, &channels, 4);
        ASSERT_NE(tex_data, nullptr) << stbi_failure_reason();

        ImageDesc image_desc = ImageDesc::TextureDesc;
        image_desc.size.width = static_cast<uint32_t>(x);
        image_desc.size.height = static_cast<uint32_t>(y);
        image_desc.mip_levels = utils::ComputeMipLevels(x, y);
        GOMA_TEST_TRY(tex, device->CreateImage(image_desc));
        diffuse_tex = tex;

        ImageData d = {{{0, tex_data}}};
        GOMA_TEST_TRYV(upload_ctx.UploadImage(*tex, std::move(d)));
        upload_ctx.GenerateMipmaps(*tex);

        stbi_image_free(tex_data);  // already copied in the staging buffer

        GOMA_TEST_TRY(s, device->CreateSampler({}));
        sampler = s;
    }

    upload_ctx.End();

    GOMA_TEST_TRY(upload_receipt, device->Submit(upload_ctx));
    GOMA_TEST_TRYV(device->WaitOnWork(std::move(upload_receipt)));

    ShaderDesc vtx_desc = {"vtx", VK_SHADER_STAGE_VERTEX_BIT, vtx};
    ShaderDesc frag_desc = {"frag", VK_SHADER_STAGE_FRAGMENT_BIT, frag};

    if (textured) {
        frag_desc.preamble = "#define HAS_DIFFUSE_TEXTURE";
    }

    GOMA_TEST_TRY(vtx, device->CreateShader(std::move(vtx_desc)));
    GOMA_TEST_TRY(frag, device->CreateShader(std::move(frag_desc)));

    auto depth_desc = ImageDesc::DepthAttachmentDesc;
    depth_desc.size = {platform->GetWidth(), platform->GetHeight(), 1};
    GOMA_TEST_TRY(depth_image, device->CreateImage(depth_desc));

    int frame_index;
    std::vector<ReceiptPtr> receipts(3);

    // Triple-buffered MVPs
    desc.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    desc.num_elements = 3;
    desc.stride = sizeof(glm::mat4);
    desc.size = 3 * sizeof(glm::mat4);
    desc.storage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    GOMA_TEST_TRY(mvp_buf, device->CreateBuffer(desc));

    std::unordered_map<Image*, FramebufferDesc> fb_desc;

    GraphicsContext context(*device);
    auto start_time = std::chrono::steady_clock::now();
    auto elapsed_time = std::chrono::steady_clock::now() - start_time;

    int frame = 0;
    const int kMaxFrames = 5000;
    for (frame = 0; frame < kMaxFrames; frame++) {
        elapsed_time = std::chrono::steady_clock::now() - start_time;
        if (elapsed_time > std::chrono::seconds(kTimeoutSeconds)) {
            break;
        }

        frame_index = frame % 3;

        if (receipts[frame_index]) {
            GOMA_TEST_TRYV(
                device->WaitOnWork(std::move(receipts[frame_index])));
        }
        context.NextFrame();

        GOMA_TEST_TRY(swapchain_image, device->AcquireSwapchainImage());

        auto fb_res = fb_desc.find(swapchain_image);
        if (fb_res == fb_desc.end()) {
            FramebufferDesc fb = {};
            fb.color_attachments.push_back({swapchain_image});
            fb.depth_attachment = {depth_image,
                                   VK_ATTACHMENT_LOAD_OP_CLEAR,
                                   VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                   {1.0f, 0}};

            fb_desc[swapchain_image] = fb;
            fb_res = fb_desc.find(swapchain_image);
        }

        PipelineDesc pipe_desc = {{vtx, frag}, fb_res->second};
        pipe_desc.depth_test = true;
        pipe_desc.cull_mode = VK_CULL_MODE_BACK_BIT;
        GOMA_TEST_TRY(pipeline, device->CreatePipeline(std::move(pipe_desc)));

        auto eye = glm::vec3(0.0f, -5.0f, 0.0f);
        auto center = glm::vec3(0.0f);
        auto up = glm::vec3{0.0f, 0.0f, 1.0f};

        auto rot_speed = glm::vec3{glm::radians(0.05f), glm::radians(0.1f),
                                   glm::radians(0.15f)};
        auto rot = glm::quat(static_cast<float>(frame) * rot_speed);

        auto mvp = glm::perspective(glm::radians(60.0f),
                                    static_cast<float>(platform->GetWidth()) /
                                        platform->GetHeight(),
                                    0.1f, 100.0f) *
                   glm::lookAt(eye, center, up) * glm::mat4_cast(rot);

        GOMA_TEST_TRY(mvp_data, device->MapBuffer(*mvp_buf));
        memcpy(static_cast<decltype(mvp)*>(mvp_data) + frame_index, &mvp,
               sizeof(mvp));
        device->UnmapBuffer(*mvp_buf);

        GOMA_TEST_TRYV(context.Begin());
        GOMA_TEST_TRYV(context.BindFramebuffer(fb_res->second));

        auto ds = DescriptorSet{{
            {0, {*mvp_buf, frame_index * sizeof(mvp), sizeof(mvp)}},
        }};

        if (textured) {
            ds[1] = {*diffuse_tex};
            ds[2] = {*sampler};
        }

        context.BindGraphicsPipeline(*pipeline);
        context.BindDescriptorSet(std::move(ds));

        context.BindVertexBuffer(*vtx_buf);
        context.BindIndexBuffer(*index_buf);
        context.DrawIndexed(static_cast<uint32_t>(cube_index_data.size()));

        context.End();

        GOMA_TEST_TRY(receipt, device->Submit(context));
        receipts[frame_index] = std::move(receipt);

        GOMA_TEST_TRYV(device->Present());
    }

    spdlog::info("Average frame time: {} ms",
                 elapsed_time.count() / (1e6 * frame));

    for (auto& receipt : receipts) {
        if (receipt) {
            GOMA_TEST_TRYV(device->WaitOnWork(std::move(receipt)));
        }
    }
}

TEST_F(RendererGraphicalTest, SpinningCube) {
    SpinningCube(device.get(), platform.get());
}

TEST_F(RendererGraphicalTest, SpinningTexturedCube) {
    SpinningCube(device.get(), platform.get(), true);
}

// TEST_F(RendererTest, GUI) {}

}  // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}

#include <glslang/Public/ShaderLang.h>
#include <SPIRV/GlslangToSpv.h>
#include <spirv_glsl.hpp>
#include <stb/stb_image.h>
#include <imgui.h>

#include "platform/win32_platform.hpp"
#include "rhi/context.hpp"
#include "rhi/device.hpp"
#include "rhi/vulkan/utils.hpp"

#include "goma_tests.hpp"

using namespace goma;

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

SCENARIO("can compile a GLSL shader", "[shader][glslang][spirv-cross]") {
    GIVEN("a Glslang process") {
        using namespace glslang;
        REQUIRE(InitializeProcess());

        WHEN("a shader is parsed") {
            TShader vtx_shader{EShLangVertex};
            vtx_shader.setStrings(&vtx, 1);
            vtx_shader.setEnvInput(EShSourceGlsl, EShLangVertex,
                                   EShClientVulkan, 100);
            vtx_shader.setEnvClient(EShClientVulkan, EShTargetVulkan_1_1);
            vtx_shader.setEnvTarget(EShTargetSpv, EShTargetSpv_1_0);

            TBuiltInResource builtin_resources = {};
            REQUIRE(
                vtx_shader.parse(&builtin_resources, 0, false, EShMsgDefault));

            THEN("no errors are reported") {
                auto info_log = vtx_shader.getInfoLog();
                INFO("Shader compilation log: " << info_log);
                CHECK(strlen(info_log) == 0);
            }

            AND_WHEN("a shader program is linked") {
                TProgram program;
                program.addShader(&vtx_shader);
                program.link(EShMsgDefault);

                THEN("no errors are reported") {
                    auto info_log = vtx_shader.getInfoLog();
                    INFO("Shader linking log: " << info_log);
                    CHECK(strlen(info_log) == 0);
                }

                AND_WHEN("the shader is converted to SPIR-V") {
                    std::vector<unsigned int> spirv;
                    spv::SpvBuildLogger logger;
                    SpvOptions spvOptions = {};
                    GlslangToSpv(*program.getIntermediate(EShLangVertex), spirv,
                                 &logger, &spvOptions);
                    FinalizeProcess();

                    THEN("no errors are reported and the code has size > 0")
                    if (!logger.getAllMessages().empty()) {
                        INFO("SPIR-V conversion log: "
                             << logger.getAllMessages());
                        CHECK(spirv.size() > 0U);
                    }

                    AND_WHEN("reflection is performed") {
                        // SPIRV-Cross reflection
                        spirv_cross::CompilerGLSL glsl(std::move(spirv));

                        THEN("resources are loaded correctly") {
                            spirv_cross::ShaderResources resources =
                                glsl.get_shader_resources();

                            auto& uv_resource = resources.stage_inputs[1];
                            CHECK(uv_resource.name == "inUVs");

                            auto uv_type =
                                glsl.get_type(uv_resource.base_type_id);
                            CHECK(uv_type.basetype ==
                                  spirv_cross::SPIRType::Float);
                            CHECK(uv_type.vecsize == 2);
                        }
                    }
                }
            }
        }
    }
}

result<Buffer*> CreateVtxBuffer(Device& device, VmaMemoryUsage storage) {
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

void HelloTriangle(Device& device, bool offscreen = false) {
    ShaderDesc vtx_desc = {"triangle_vtx", VK_SHADER_STAGE_VERTEX_BIT,
                           triangle_vtx};
    ShaderDesc frag_desc = {"triangle_frag", VK_SHADER_STAGE_FRAGMENT_BIT,
                            triangle_frag};

    GOMA_TEST_TRY(vtx, device.CreateShader(std::move(vtx_desc)));
    GOMA_TEST_TRY(frag, device.CreateShader(std::move(frag_desc)));

    Image* render_target = nullptr;

    if (!offscreen) {
        GOMA_TEST_TRY(swapchain_image, device.AcquireSwapchainImage());
        render_target = swapchain_image;
    } else {
        auto desc = ImageDesc::ColorAttachmentDesc;
        desc.size = {800, 600, 1};
        GOMA_TEST_TRY(image, device.CreateImage(desc));
        render_target = image;
    }

    FramebufferDesc fb_desc = {};
    fb_desc.color_attachments.push_back({render_target});

    GOMA_TEST_TRY(pipeline, device.GetPipeline({{vtx, frag}, fb_desc}));

    GraphicsContext context(device);

    GOMA_TEST_TRYV(context.Begin());
    GOMA_TEST_TRYV(context.BindFramebuffer(fb_desc));

    context.BindGraphicsPipeline(*pipeline);
    context.Draw(3);

    context.End();

    GOMA_TEST_TRY(receipt, device.Submit(context));

    if (!offscreen) {
        GOMA_TEST_TRYV(device.Present());
    }

    GOMA_TEST_TRYV(device.WaitOnWork(std::move(receipt)));
}

SCENARIO("the rendering abstraction can create objects and submit commands",
         "[rendering-abstraction][buffer][image][texture][shader]") {
    GIVEN("a valid device") {
        Device device;

        WHEN("a CPU-side buffer is created and filled with data") {
            GOMA_TEST_TRY(buffer,
                          CreateVtxBuffer(device, VMA_MEMORY_USAGE_CPU_TO_GPU));

            THEN("handle is valid and data are properly copied") {
                CHECK(buffer->GetHandle() != VK_NULL_HANDLE);
                CHECK(buffer->GetNumElements() == cube_vtx_data.size());

                GOMA_TEST_TRY(data, device.MapBuffer(*buffer));
                auto buf_data = static_cast<Vertex*>(data);
                CHECK(buf_data[2].pos[2] == cube_vtx_data[2].pos[2]);
                CHECK(buf_data[5].color[1] == cube_vtx_data[5].color[1]);
                device.UnmapBuffer(*buffer);
            }
        }

        WHEN("a GPU-side buffer is created and filled with data") {
            GOMA_TEST_TRY(buffer,
                          CreateVtxBuffer(device, VMA_MEMORY_USAGE_GPU_ONLY));

            THEN("handle and number of elements are valid") {
                CHECK(buffer->GetHandle() != VK_NULL_HANDLE);
                CHECK(buffer->GetNumElements() == cube_vtx_data.size());
            }
        }

        WHEN("a color attachment is created") {
            auto desc = ImageDesc::ColorAttachmentDesc;
            desc.size = {800, 600, 1};

            GOMA_TEST_TRY(image, device.CreateImage(desc));

            THEN("the handles and allocation are valid") {
                CHECK(image->GetHandle() != VkImage{VK_NULL_HANDLE});
                CHECK(image->GetView() != VkImageView{VK_NULL_HANDLE});
                CHECK(image->GetAllocation().allocation !=
                      VmaAllocation{VK_NULL_HANDLE});
            }
        }

        WHEN("a texture is created") {
            auto width = 64U;
            auto height = 64U;

            auto desc = ImageDesc::TextureDesc;
            desc.size = {width, height, 1};
            desc.mip_levels = utils::ComputeMipLevels(width, height);

            GOMA_TEST_TRY(image, device.CreateImage(desc));

            THEN("the handles and allocation are valid") {
                CHECK(image->GetHandle() != VkImage{VK_NULL_HANDLE});
                CHECK(image->GetView() != VkImageView{VK_NULL_HANDLE});
                CHECK(image->GetAllocation().allocation !=
                      VmaAllocation{VK_NULL_HANDLE});

                AND_THEN("it can be filled with data and mipmapped") {
                    std::vector<uint8_t> image_data(
                        width * height *
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

                    UploadContext ctx(device);

                    GOMA_TEST_TRYV(ctx.Begin());
                    GOMA_TEST_TRYV(
                        ctx.UploadImage(*image, {image_data.data()}));
                    ctx.GenerateMipmaps(*image);
                    ctx.End();

                    GOMA_TEST_TRY(receipt, device.Submit(ctx));
                    GOMA_TEST_TRYV(device.WaitOnWork(std::move(receipt)));
                }
            }
        }

        WHEN("three framebuffers are created") {
            auto desc = ImageDesc::ColorAttachmentDesc;
            desc.size = {64, 64, 1};
            GOMA_TEST_TRY(image, device.CreateImage(desc));
            GOMA_TEST_TRY(other_image, device.CreateImage(desc));

            desc.samples = VK_SAMPLE_COUNT_4_BIT;
            GOMA_TEST_TRY(ms_image, device.CreateImage(desc));

            FramebufferDesc fb_1 = {};
            FramebufferDesc fb_2 = {};
            FramebufferDesc fb_3 = {};

            fb_1.color_attachments = {{image}, {other_image}};
            fb_2.color_attachments = {{other_image}, {image}};
            fb_3.color_attachments = {{image}, {ms_image}};

            THEN("compatibility between them can be checked correctly") {
                CHECK(IsCompatible(fb_1, fb_2) == true);
                CHECK(IsCompatible(fb_1, fb_3) == false);
            }
        }

        WHEN("a shader is created") {
            ShaderDesc shader_desc = {};
            shader_desc.name = "vtx";
            shader_desc.stage = VK_SHADER_STAGE_VERTEX_BIT;
            shader_desc.source = vtx;
            GOMA_TEST_TRY(shader, device.CreateShader(std::move(shader_desc)));

            THEN("handle and resources are set correctly") {
                REQUIRE(shader->GetHandle() != VkShaderModule{VK_NULL_HANDLE});

                auto& uv_resource = shader->GetInputs().at(2);
                CHECK(uv_resource.name == "inUVs");

                auto& uniform_buffer = shader->GetBindings().at(0);
                CHECK(uniform_buffer.name == "UBO");
            }

            AND_WHEN("a pipeline using that shader is created") {
                GOMA_TEST_TRY(pipeline, device.GetPipeline(
                                            {{shader}, FramebufferDesc{}}));

                THEN("handles are set correctly") {
                    CHECK(pipeline->GetHandle() != VkPipeline{VK_NULL_HANDLE});
                }
            }
        }

        WHEN("a buffer and a pipeline are created") {
            // Create a buffer
            BufferDesc desc = {};
            desc.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            desc.num_elements = static_cast<uint32_t>(cube_vtx_data.size());
            desc.stride = sizeof(cube_vtx_data[0]);
            desc.size = cube_vtx_data.size() * sizeof(cube_vtx_data[0]);
            desc.storage = VMA_MEMORY_USAGE_CPU_TO_GPU;

            GOMA_TEST_TRY(buffer, device.CreateBuffer(desc));

            UploadContext ctx(device);
            GOMA_TEST_TRYV(ctx.Begin());
            GOMA_TEST_TRYV(ctx.UploadBuffer(
                *buffer, {cube_vtx_data.size() * sizeof(cube_vtx_data[0]),
                          cube_vtx_data.data()}));
            ctx.End();

            GOMA_TEST_TRY(receipt, device.Submit(ctx));
            GOMA_TEST_TRYV(device.WaitOnWork(std::move(receipt)));

            // Create a pipeline
            ShaderDesc shader_desc = {};
            shader_desc.name = "vtx";
            shader_desc.stage = VK_SHADER_STAGE_VERTEX_BIT;
            shader_desc.source = vtx;

            GOMA_TEST_TRY(shader, device.CreateShader(std::move(shader_desc)));
            GOMA_TEST_TRY(pipeline,
                          device.GetPipeline({{shader}, FramebufferDesc{}}));

            THEN("a descriptor set can be bound") {
                GraphicsContext context(device);
                GOMA_TEST_TRYV(context.Begin());
                context.BindGraphicsPipeline(*pipeline);
                context.BindDescriptorSet({{0, *buffer}});
                context.End();
            }
        }

        THEN("a triangle can be rendered offscreen") {
            HelloTriangle(device, true);
        }
    }
}

SCENARIO("the rendering abstraction can render a triangle",
         "[rendering-abstraction][window][triangle]") {
    GIVEN("a valid device and platform") {
        Device device;

        std::unique_ptr<Platform> platform_ptr =
            std::make_unique<Win32Platform>();
        Platform& platform = *platform_ptr.get();

        GOMA_TEST_TRYV(platform.InitWindow(kWindowWidth, kWindowHeight));
        GOMA_TEST_TRYV(device.InitWindow(platform));

        THEN("a triangle can be rendered to the screen") {
            HelloTriangle(device);
            platform.Sleep(kTimeoutSeconds * 1000000);
        }
    }
}

void SpinningCube(Device& device, Platform& platform, bool textured = false) {
    BufferDesc desc = {};
    desc.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    desc.num_elements = static_cast<uint32_t>(cube_vtx_data.size());
    desc.stride = sizeof(cube_vtx_data[0]);
    desc.size = cube_vtx_data.size() * sizeof(cube_vtx_data[0]);
    desc.storage = VMA_MEMORY_USAGE_GPU_ONLY;
    GOMA_TEST_TRY(vtx_buf, device.CreateBuffer(desc));

    desc.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    desc.num_elements = static_cast<uint32_t>(cube_index_data.size());
    desc.stride = sizeof(cube_index_data[0]);
    desc.size = cube_index_data.size() * sizeof(cube_index_data[0]);
    desc.storage = VMA_MEMORY_USAGE_GPU_ONLY;
    GOMA_TEST_TRY(index_buf, device.CreateBuffer(desc));

    UploadContext upload_ctx(device);
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

        CAPTURE(stbi_failure_reason());
        REQUIRE(tex_data != nullptr);

        ImageDesc image_desc = ImageDesc::TextureDesc;
        image_desc.size.width = static_cast<uint32_t>(x);
        image_desc.size.height = static_cast<uint32_t>(y);
        image_desc.mip_levels = utils::ComputeMipLevels(x, y);
        GOMA_TEST_TRY(tex, device.CreateImage(image_desc));
        diffuse_tex = tex;

        GOMA_TEST_TRYV(upload_ctx.UploadImage(*tex, {tex_data}));
        upload_ctx.GenerateMipmaps(*tex);

        stbi_image_free(tex_data);  // already copied in the staging

        GOMA_TEST_TRY(s, device.CreateSampler({}));
        sampler = s;
    }

    upload_ctx.End();

    GOMA_TEST_TRY(upload_receipt, device.Submit(upload_ctx));
    GOMA_TEST_TRYV(device.WaitOnWork(std::move(upload_receipt)));

    ShaderDesc vtx_desc = {"vtx", VK_SHADER_STAGE_VERTEX_BIT, vtx};
    ShaderDesc frag_desc = {"frag", VK_SHADER_STAGE_FRAGMENT_BIT, frag};

    if (textured) {
        frag_desc.preamble = "#define HAS_DIFFUSE_TEXTURE";
    }

    GOMA_TEST_TRY(vtx, device.CreateShader(std::move(vtx_desc)));
    GOMA_TEST_TRY(frag, device.CreateShader(std::move(frag_desc)));

    auto depth_desc = ImageDesc::DepthAttachmentDesc;
    depth_desc.size = {platform.GetWidth(), platform.GetHeight(), 1};
    GOMA_TEST_TRY(depth_image, device.CreateImage(depth_desc));

    // Triple-buffered MVPs
    std::vector<Buffer*> mvp_bufs(3, nullptr);

    std::unordered_map<Image*, FramebufferDesc> fb_desc;

    GraphicsContext context(device);
    auto start_time = std::chrono::steady_clock::now();
    auto elapsed_time = std::chrono::steady_clock::now() - start_time;

    std::vector<ReceiptPtr> receipts(3);

    const auto render_frame = [&](int frame) -> result<void> {
        auto frame_index = frame % 3;

        if (receipts[frame_index]) {
            OUTCOME_TRY(device.WaitOnWork(std::move(receipts[frame_index])));
        }
        context.NextFrame();

        OUTCOME_TRY(swapchain_image, device.AcquireSwapchainImage());

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
        OUTCOME_TRY(pipeline, device.GetPipeline(std::move(pipe_desc)));

        auto eye = glm::vec3(0.0f, -5.0f, 0.0f);
        auto center = glm::vec3(0.0f);
        auto up = glm::vec3{0.0f, 0.0f, 1.0f};

        auto rot_speed = glm::vec3{glm::radians(0.05f), glm::radians(0.1f),
                                   glm::radians(0.15f)};
        auto rot = glm::quat(static_cast<float>(frame) * rot_speed);

        auto mvp = glm::perspective(glm::radians(60.0f),
                                    static_cast<float>(platform.GetWidth()) /
                                        platform.GetHeight(),
                                    0.1f, 100.0f) *
                   glm::lookAt(eye, center, up) * glm::mat4_cast(rot);

        if (!mvp_bufs[frame_index]) {
            desc.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            desc.num_elements = 3;
            desc.stride = sizeof(glm::mat4);
            desc.size = 3 * sizeof(glm::mat4);
            desc.storage = VMA_MEMORY_USAGE_CPU_TO_GPU;
            GOMA_TEST_TRY(mvp_buf, device.CreateBuffer(desc));
            mvp_bufs[frame_index] = mvp_buf;
        }

        OUTCOME_TRY(mvp_data, device.MapBuffer(*mvp_bufs[frame_index]));
        memcpy(mvp_data, &mvp, sizeof(mvp));
        device.UnmapBuffer(*mvp_bufs[frame_index]);

        OUTCOME_TRY(context.Begin());
        OUTCOME_TRY(context.BindFramebuffer(fb_res->second));

        auto ds = DescriptorSet{{
            {0, {*mvp_bufs[frame_index]}},
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

        OUTCOME_TRY(receipt, device.Submit(context));
        receipts[frame_index] = std::move(receipt);

        OUTCOME_TRY(device.Present());

        return outcome::success();
    };

    int frame = 0;
    GOMA_TEST_TRYV(platform.MainLoop([&]() -> result<bool> {
        frame++;
        elapsed_time = std::chrono::steady_clock::now() - start_time;
        if (elapsed_time > std::chrono::seconds(kTimeoutSeconds)) {
            return true;
        }

        OUTCOME_TRY(render_frame(frame));
        return false;
    }));

    char* test_name = textured ? "Spinning textured cube" : "Spinning cube";
    spdlog::info("{} - Average frame time: {} ms", test_name,
                 elapsed_time.count() / (1e6 * frame));

    for (auto& receipt : receipts) {
        if (receipt) {
            GOMA_TEST_TRYV(device.WaitOnWork(std::move(receipt)));
        }
    }
}

SCENARIO("the rendering abstraction can render a spinning cube",
         "[rendering-abstraction][window][cube][texture]") {
    GIVEN("a valid device and platform") {
        Device device;

        std::unique_ptr<Platform> platform_ptr =
            std::make_unique<Win32Platform>();
        Platform& platform = *platform_ptr.get();

        GOMA_TEST_TRYV(platform.InitWindow(kWindowWidth, kWindowHeight));
        GOMA_TEST_TRYV(device.InitWindow(platform));

        WHEN("a spinning cube is rendered to the screen") {
            SpinningCube(device, platform);

            THEN("no errors are reported") {}
        }

        WHEN("a textured spinning cube is rendered to the screen") {
            SpinningCube(device, platform, true);

            THEN("no errors are reported") {}
        }
    }
}

SCENARIO("can set up imgui", "[rendering-abstraction][gui][imgui]") {
    Device device;

    std::unique_ptr<Platform> platform_ptr = std::make_unique<Win32Platform>();
    Platform& platform = *platform_ptr.get();

    GOMA_TEST_TRYV(platform.InitWindow(kWindowWidth, kWindowHeight));
    GOMA_TEST_TRYV(device.InitWindow(platform));

    ImGuiIO& io = ImGui::GetIO();
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable
    // Keyboard Controls
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable
    // Gamepad Controls
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    io.DisplaySize =
        ImVec2{float(platform.GetWidth()), float(platform.GetHeight())};

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsClassic();

    io.Fonts->AddFontFromFileTTF(GOMA_ASSETS_DIR "fonts/Roboto-Medium.ttf",
                                 16.0f);

    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    auto desc = ImageDesc::TextureDesc;
    desc.size.width = static_cast<uint32_t>(width);
    desc.size.height = static_cast<uint32_t>(height);

    GOMA_TEST_TRY(font_tex, device.CreateImage(desc));
    GOMA_TEST_TRY(font_sampler, device.CreateSampler({}));
    io.Fonts->TexID = font_tex->GetHandle();

    UploadContext ctx(device);
    GOMA_TEST_TRYV(ctx.Begin());
    GOMA_TEST_TRYV(ctx.UploadImage(*font_tex, {pixels}));
    ctx.End();

    {
        GOMA_TEST_TRY(receipt, device.Submit(ctx));
        GOMA_TEST_TRYV(device.WaitOnWork(std::move(receipt)));
    }

    GOMA_TEST_TRY(gui_vtx, platform.ReadFileToString(GOMA_ASSETS_DIR
                                                     "shaders/imgui.vert"));
    GOMA_TEST_TRY(gui_frag, platform.ReadFileToString(GOMA_ASSETS_DIR
                                                      "shaders/imgui.frag"));

    ShaderDesc vtx_desc = {"gui_vtx", VK_SHADER_STAGE_VERTEX_BIT,
                           std::move(gui_vtx)};
    ShaderDesc frag_desc = {"gui_frag", VK_SHADER_STAGE_FRAGMENT_BIT,
                            std::move(gui_frag)};

    GOMA_TEST_TRY(vtx, device.CreateShader(std::move(vtx_desc)));
    GOMA_TEST_TRY(frag, device.CreateShader(std::move(frag_desc)));

    // ImGui passes vertex color data as RGBA8, so we override the corresponding
    // format in shader inputs
    auto inputs = vtx->GetInputs();
    auto color_input =
        std::find_if(inputs.begin(), inputs.end(),
                     [](const auto& i) { return i.name == "aColor"; });
    if (color_input != inputs.end()) {
        color_input->format = VK_FORMAT_R8G8B8A8_UNORM;
        vtx->SetInputs(inputs);
    }

    GraphicsContext context(device);
    auto start_time = std::chrono::steady_clock::now();
    auto elapsed_time = std::chrono::steady_clock::now() - start_time;

    std::vector<ReceiptPtr> receipts(3);
    std::vector<Buffer*> vtx_bufs(3, nullptr);
    std::vector<Buffer*> idx_bufs(3, nullptr);
    std::vector<Buffer*> gui_ubos(3, nullptr);

    const auto render_frame = [&](int frame) -> result<void> {
        auto frame_index = frame % 3;

        if (receipts[frame_index]) {
            OUTCOME_TRY(device.WaitOnWork(std::move(receipts[frame_index])));
        }
        context.NextFrame();

        ImGui::NewFrame();

        // Dummy window to override defaults in ShowDemoWindow()
        ImGui::SetNextWindowPos({100, 100});
        ImGui::SetNextWindowSize({300, 600});
        ImGui::Begin("Dear ImGui Demo");
        ImGui::End();

        ImGui::ShowDemoWindow();
        ImGui::Render();

        auto draw_data = ImGui::GetDrawData();

        // Create or resize the vertex/index buffers
        size_t vtx_size = draw_data->TotalVtxCount * sizeof(ImDrawVert);
        size_t idx_size = draw_data->TotalIdxCount * sizeof(ImDrawIdx);

        if (!vtx_bufs[frame_index] ||
            vtx_bufs[frame_index]->GetSize() < vtx_size) {
            if (vtx_bufs[frame_index]) {
                // TODO: device.DestroyBuffer(*vtx_bufs[frame_index]);
            }

            BufferDesc vtx_buf_desc = {};
            vtx_buf_desc.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            vtx_buf_desc.num_elements = draw_data->TotalVtxCount;
            vtx_buf_desc.stride = sizeof(ImDrawVert);
            vtx_buf_desc.size = vtx_size;
            vtx_buf_desc.storage = VMA_MEMORY_USAGE_CPU_TO_GPU;

            OUTCOME_TRY(vtx_buf, device.CreateBuffer(vtx_buf_desc));
            vtx_bufs[frame_index] = vtx_buf;
        }

        if (!idx_bufs[frame_index] ||
            idx_bufs[frame_index]->GetSize() < idx_size) {
            if (idx_bufs[frame_index]) {
                // TODO: device.DestroyBuffer(*idx_bufs[frame_index]);
            }

            BufferDesc idx_buf_desc = {};
            idx_buf_desc.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            idx_buf_desc.num_elements = draw_data->TotalIdxCount;
            idx_buf_desc.stride = sizeof(ImDrawIdx);
            idx_buf_desc.size = idx_size;
            idx_buf_desc.storage = VMA_MEMORY_USAGE_CPU_TO_GPU;

            OUTCOME_TRY(idx_buf, device.CreateBuffer(idx_buf_desc));
            idx_bufs[frame_index] = idx_buf;
        }

        // GUI uniform buffer
        struct GUIScaleTranslate {
            glm::vec2 scale;
            glm::vec2 translate;
        };

        if (!gui_ubos[frame_index]) {
            BufferDesc gui_ubo_desc = {};
            gui_ubo_desc.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            gui_ubo_desc.num_elements = 1;
            gui_ubo_desc.stride = sizeof(GUIScaleTranslate);
            gui_ubo_desc.size = gui_ubo_desc.num_elements * gui_ubo_desc.stride;
            gui_ubo_desc.storage = VMA_MEMORY_USAGE_CPU_TO_GPU;

            OUTCOME_TRY(gui_ubo, device.CreateBuffer(gui_ubo_desc));
            gui_ubos[frame_index] = gui_ubo;
        }

        // Upload vertex/index data into a single contiguous GPU buffer
        {
            OUTCOME_TRY(vtx_dst_raw, device.MapBuffer(*vtx_bufs[frame_index]));
            OUTCOME_TRY(idx_dst_raw, device.MapBuffer(*idx_bufs[frame_index]));

            auto vtx_dst = reinterpret_cast<ImDrawVert*>(vtx_dst_raw);
            auto idx_dst = reinterpret_cast<ImDrawIdx*>(idx_dst_raw);

            for (int n = 0; n < draw_data->CmdListsCount; n++) {
                const ImDrawList* cmd_list = draw_data->CmdLists[n];
                memcpy(vtx_dst, cmd_list->VtxBuffer.Data,
                       cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
                memcpy(idx_dst, cmd_list->IdxBuffer.Data,
                       cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
                vtx_dst += cmd_list->VtxBuffer.Size;
                idx_dst += cmd_list->IdxBuffer.Size;
            }

            device.UnmapBuffer(*vtx_bufs[frame_index]);
            device.UnmapBuffer(*idx_bufs[frame_index]);
        }

        OUTCOME_TRY(gui_ubo_data, device.MapBuffer(*gui_ubos[frame_index]));
        GUIScaleTranslate gui_scale_translate;

        gui_scale_translate.scale = {2.0f / draw_data->DisplaySize.x,
                                     2.0f / draw_data->DisplaySize.y};
        gui_scale_translate.translate = {
            -1.0f - draw_data->DisplayPos.x * gui_scale_translate.scale[0],
            -1.0f - draw_data->DisplayPos.y * gui_scale_translate.scale[1]};

        memcpy(gui_ubo_data, &gui_scale_translate, sizeof(gui_scale_translate));
        device.UnmapBuffer(*gui_ubos[frame_index]);

        OUTCOME_TRY(swapchain_image, device.AcquireSwapchainImage());

        FramebufferDesc fb_desc = {};
        fb_desc.color_attachments.push_back({swapchain_image});

        auto pipe_desc = PipelineDesc{{vtx, frag}, fb_desc};
        pipe_desc.color_blend = true;
        OUTCOME_TRY(pipeline, device.GetPipeline(std::move(pipe_desc)));

        OUTCOME_TRYV(context.Begin());
        OUTCOME_TRYV(context.BindFramebuffer(fb_desc));

        context.BindGraphicsPipeline(*pipeline);
        context.BindDescriptorSet({
            {0, Descriptor{*font_tex, *font_sampler}},
            {1, Descriptor{*gui_ubos[frame_index]}},
        });

        context.BindVertexBuffer(*vtx_bufs[frame_index]);
        context.BindIndexBuffer(*idx_bufs[frame_index], 0,
                                sizeof(ImDrawIdx) == 2 ? VK_INDEX_TYPE_UINT16
                                                       : VK_INDEX_TYPE_UINT32);

        // Render command lists
        // (Because we merged all buffers into a single one, we maintain our own
        // offset into them)
        int global_vtx_offset = 0;
        int global_idx_offset = 0;
        for (int n = 0; n < draw_data->CmdListsCount; n++) {
            const ImDrawList* cmd_list = draw_data->CmdLists[n];
            for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
                const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
                if (pcmd->UserCallback != NULL) {
                    // User callback, registered via ImDrawList::AddCallback()
                    // (ImDrawCallback_ResetRenderState is a special callback
                    // value used by the user to request the renderer to reset
                    // render state.)
                    if (pcmd->UserCallback == ImDrawCallback_ResetRenderState) {
                    } else {
                        pcmd->UserCallback(cmd_list, pcmd);
                    }
                } else {
                    ImVec2 clip_off = {0, 0};    // except multiview
                    ImVec2 clip_scale = {1, 1};  // except retina displays

                    // Project scissor/clipping rectangles into framebuffer
                    // space
                    ImVec4 clip_rect;
                    clip_rect.x =
                        (pcmd->ClipRect.x - clip_off.x) * clip_scale.x;
                    clip_rect.y =
                        (pcmd->ClipRect.y - clip_off.y) * clip_scale.y;
                    clip_rect.z =
                        (pcmd->ClipRect.z - clip_off.x) * clip_scale.x;
                    clip_rect.w =
                        (pcmd->ClipRect.w - clip_off.y) * clip_scale.y;

                    if (clip_rect.x < platform.GetWidth() &&
                        clip_rect.y < platform.GetHeight() &&
                        clip_rect.z >= 0.0f && clip_rect.w >= 0.0f) {
                        // Negative offsets are illegal for vkCmdSetScissor
                        if (clip_rect.x < 0.0f) clip_rect.x = 0.0f;
                        if (clip_rect.y < 0.0f) clip_rect.y = 0.0f;

                        // Apply scissor/clipping rectangle
                        VkRect2D scissor;
                        scissor.offset.x = (int32_t)(clip_rect.x);
                        scissor.offset.y = (int32_t)(clip_rect.y);
                        scissor.extent.width =
                            (uint32_t)(clip_rect.z - clip_rect.x);
                        scissor.extent.height =
                            (uint32_t)(clip_rect.w - clip_rect.y);
                        context.SetScissor(scissor);

                        context.DrawIndexed(
                            pcmd->ElemCount, 1,
                            pcmd->IdxOffset + global_idx_offset,
                            pcmd->VtxOffset + global_vtx_offset);
                    }
                }
            }
            global_idx_offset += cmd_list->IdxBuffer.Size;
            global_vtx_offset += cmd_list->VtxBuffer.Size;
        }

        OUTCOME_TRY(receipt, device.Submit(context));
        receipts[frame_index] = std::move(receipt);

        OUTCOME_TRY(device.Present());

        return outcome::success();
    };

    int frame = 0;
    GOMA_TEST_TRYV(platform.MainLoop([&]() -> result<bool> {
        frame++;
        elapsed_time = std::chrono::steady_clock::now() - start_time;
        if (elapsed_time > std::chrono::seconds(kTimeoutSeconds)) {
            return true;
        }

        OUTCOME_TRY(render_frame(frame));
        return false;
    }));

    char* test_name = "GUI test";
    spdlog::info("{} - Average frame time: {} ms", test_name,
                 elapsed_time.count() / (1e6 * frame));

    for (auto& receipt : receipts) {
        if (receipt) {
            GOMA_TEST_TRYV(device.WaitOnWork(std::move(receipt)));
        }
    }
}

}  // namespace

#include "gtest/gtest.h"

#include "scene/scene.hpp"
#include "renderer/vez/vez_backend.hpp"
#include "platform/win32_platform.hpp"

using namespace goma;

namespace {

TEST(SceneTest, CanCreateScene) {
    Scene s;
    ASSERT_EQ(s.GetRootNode(), NodeIndex(0, 1));
}

TEST(SceneTest, CanCreateNodes) {
    Scene s;

    auto node = s.CreateNode(s.GetRootNode()).value();
    ASSERT_EQ(node, NodeIndex(1, 1));
    ASSERT_EQ(s.GetParent(node).value(), s.GetRootNode());
    ASSERT_EQ(*s.GetChildren(node).value(), std::set<NodeIndex>{});

    auto child_node = s.CreateNode(node).value();
    ASSERT_EQ(child_node, NodeIndex(2, 1));
    ASSERT_EQ(s.GetParent(child_node).value(), NodeIndex(1, 1));
}

TEST(SceneTest, CanDeleteNodes) {
    Scene s;

    auto node = s.CreateNode(s.GetRootNode()).value();
    ASSERT_EQ(*s.GetChildren(s.GetRootNode()).value(),
              std::set<NodeIndex>{node});

    s.DeleteNode(node);

    ASSERT_EQ(*s.GetChildren(s.GetRootNode()).value(), std::set<NodeIndex>{});
    ASSERT_FALSE(s.GetParent(node));
}

TEST(SceneTest, CanDeleteAndRecreateNodes) {
    Scene s;

    auto node = s.CreateNode(s.GetRootNode()).value();
    auto child_node = s.CreateNode(node).value();
    s.DeleteNode(node);

    EXPECT_EQ(s.GetParent(child_node).value(), s.GetRootNode())
        << "Child node was not updated when parent node was deleted";

    auto new_node = s.CreateNode(s.GetRootNode()).value();
    ASSERT_EQ(new_node, NodeIndex(1, 2)) << "New generation not set properly";
    ASSERT_EQ(s.GetParent(new_node).value(), s.GetRootNode());
}

class VezBackendTest : public ::testing::Test {
  protected:
    VezBackend vez;

    void SetUp() override {
        auto init_context_result = vez.InitContext();
        ASSERT_TRUE(init_context_result)
            << init_context_result.error().message();
    }
};

TEST_F(VezBackendTest, RenderTriangle) {
    Win32Platform platform;
    platform.InitWindow();

    auto init_surface_result = vez.InitSurface(&platform);
    ASSERT_TRUE(init_surface_result) << init_surface_result.error().message();

    std::vector<glm::vec3> positions = {
        {0.5f, 0.0f, 0.0f}, {0.0f, -0.5f, 0.0f}, {0.0f, 0.5f, 0.0f}};

    auto create_pos_buffer_result = vez.CreateVertexBuffer(
        "triangle_pos", positions.size() * sizeof(positions[0]), true,
        positions.data());
    ASSERT_TRUE(create_pos_buffer_result)
        << create_pos_buffer_result.error().message();

    std::vector<glm::vec2> uvs = {{0.0, 0.0}, {1.0, 0.0}, {0.0, 1.0}};

    auto create_uv_buffer_result = vez.CreateVertexBuffer(
        "triangle_uvs", uvs.size() * sizeof(uvs[0]), true, uvs.data());
    ASSERT_TRUE(create_uv_buffer_result)
        << create_uv_buffer_result.error().message();

    auto vertex_input_format_result = vez.GetVertexInputFormat(
        {{{0, sizeof(glm::vec3)}, {1, sizeof(glm::vec2)}},
         {{0, 0, Format::SFloatRGB32, 0}, {1, 1, Format::SFloatRG32, 0}}});
    ASSERT_TRUE(vertex_input_format_result)
        << vertex_input_format_result.error().message();

    TextureDesc texture_desc = {512, 512};
    std::vector<std::array<uint8_t, 4>> pixels(512 * 512, {255, 0, 0, 255});
    auto create_texture_result =
        vez.CreateTexture("texture", texture_desc, pixels.data());
    ASSERT_TRUE(create_texture_result)
        << create_texture_result.error().message();

    static const char* vertex_shader_glsl = R"(
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUVs;

layout(location = 0) out vec2 outUVs;

void main() {
    gl_Position = vec4(inPosition, 1.0);
    outUVs = inUVs;
}
)";

    static const char* fragment_shader_glsl = R"(
#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform sampler2D mainTexture;

layout(location = 0) in vec2 inUVs;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture(mainTexture, inUVs);
}
)";

    auto create_pipeline_result =
        vez.GetGraphicsPipeline(vertex_shader_glsl, fragment_shader_glsl);
    ASSERT_TRUE(create_pipeline_result)
        << create_pipeline_result.error().message();

    FramebufferDesc fb_desc = {800, 600};
    auto create_fb_result = vez.CreateFramebuffer(0, "fb", fb_desc);

    vez.SetupFrames(
        1);  // TODO crashes if we don't call SetupFrames before StartFrames
    vez.StartFrame();
    vez.StartRenderPass(create_fb_result.value(), {});
    vez.BindGraphicsPipeline(create_pipeline_result.value());
    vez.BindVertexInputFormat(vertex_input_format_result.value());
    vez.BindVertexBuffers(
        {create_pos_buffer_result.value(), create_uv_buffer_result.value()});
    vez.BindTextures({create_texture_result.value()});
    vez.Draw(3);
    vez.FinishFrame();
    vez.PresentImage("color");

    system("pause");
}

}  // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    system("pause");
    return ret;
}

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

/*
TEST_F(VezBackendTest, CanInitializeContext) {}

TEST_F(VezBackendTest, CanInitializeWin32Surface) {
    Win32Platform platform;
    platform.InitWindow();

    auto init_surface_result = vez.InitSurface(&platform);
    ASSERT_TRUE(init_surface_result) << init_surface_result.error().message();
}

TEST_F(VezBackendTest, CanCreatePipeline) {
    static const char* vertex_shader_glsl = R"(
#version 450

layout(location = 0) in vec3 inPosition;

void main() {
    gl_Position = vec4(gl_InstanceIndex * 0.1 * inPosition, 1.0);
}
)";

    static const char* fragment_shader_glsl = R"(
#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(1.0, 1.0, 1.0, 1.0);
}
)";

    auto create_pipeline_result =
        vez.GetGraphicsPipeline(vertex_shader_glsl, fragment_shader_glsl);
    ASSERT_TRUE(create_pipeline_result)
        << create_pipeline_result.error().message();
}

TEST_F(VezBackendTest, CanCreateTexture) {
    TextureDesc texture_desc = {800, 600};

    auto create_texture_result = vez.CreateTexture("texture", texture_desc);
    ASSERT_TRUE(create_texture_result)
        << create_texture_result.error().message();
}

TEST_F(VezBackendTest, CanCreateFramebuffer) {
    FramebufferDesc fb_desc = {800, 600};

    auto create_fb_result = vez.CreateFramebuffer(0, "fb", fb_desc);
    ASSERT_TRUE(create_fb_result) << create_fb_result.error().message();
}
*/

TEST_F(VezBackendTest, RenderTriangle) {
    Win32Platform platform;
    platform.InitWindow();

    auto init_surface_result = vez.InitSurface(&platform);
    ASSERT_TRUE(init_surface_result) << init_surface_result.error().message();

    static const char* vertex_shader_glsl = R"(
#version 450

// layout(location = 0) in vec3 inPosition;
vec3 inPosition[3] = {{1.0, 0.0, 0.0}, {0.0, -1.0, 0.0}, {0.0, 1.0, 0.0}};

void main() {
    gl_Position = vec4(inPosition[gl_VertexIndex], 1.0);
}
)";

    static const char* fragment_shader_glsl = R"(
#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(1.0, 1.0, 1.0, 1.0);
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
    vez.Draw(3);
    vez.FinishFrame("color");

    system("Pause");
}

}  // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    system("pause");
    return ret;
}

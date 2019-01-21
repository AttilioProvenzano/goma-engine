#include "gtest/gtest.h"

#include "scene/scene.hpp"
#include "renderer/vez/vez_backend.hpp"
#include "platform/win32_platform.hpp"

using namespace goma;

namespace {

TEST(SceneTest, CanCreateScene) {
    Scene s;
    ASSERT_EQ(s.GetRootNode()->id, NodeIndex(0, 1));
    ASSERT_EQ(s.GetRootNode()->parent_id, NodeIndex(0, 0));
    ASSERT_EQ(s.GetRootNode()->transform, Transform());
}

TEST(SceneTest, CanCreateNodes) {
    Scene s;

    auto node = s.CreateNode(s.GetRootNode());
    ASSERT_EQ(node->id, NodeIndex(1, 1));
    ASSERT_EQ(node->parent_id, s.GetRootNode()->id);

    auto child_node = s.CreateNode(node);
    ASSERT_EQ(child_node->id, NodeIndex(2, 1));
    ASSERT_EQ(child_node->parent_id, NodeIndex(1, 1));
}

TEST(SceneTest, CanDeleteNodes) {
    Scene s;

    auto node = s.CreateNode(s.GetRootNode());
    ASSERT_EQ(node->id, NodeIndex(1, 1));
    ASSERT_EQ(node->parent_id, s.GetRootNode()->id);

    s.DeleteNode(node->id);
    ASSERT_EQ(node->id, NodeIndex(1, 0));
}

TEST(SceneTest, CanDeleteAndRecreateNodes) {
    Scene s;

    auto node = s.CreateNode(s.GetRootNode());
    node = s.CreateNode(node->id);
    ASSERT_EQ(node->id, NodeIndex(2, 1));
    ASSERT_EQ(node->parent_id, NodeIndex(1, 1));

    s.DeleteNode(NodeIndex(1, 1));
    EXPECT_EQ(node->parent_id, s.GetRootNode()->id)
        << "parent_id was not updated when parent node was deleted";

    node = s.CreateNode(s.GetRootNode());
    ASSERT_EQ(node->id, NodeIndex(1, 2));
    ASSERT_EQ(node->parent_id, s.GetRootNode()->id);
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

}  // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    system("pause");
    return ret;
}

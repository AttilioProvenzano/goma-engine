#include "gtest/gtest.h"

#include "engine.hpp"

#include "scene/scene.hpp"
#include "scene/attachments/texture.hpp"
#include "scene/attachments/material.hpp"
#include "scene/attachments/camera.hpp"
#include "scene/attachments/light.hpp"
#include "scene/attachments/mesh.hpp"
#include "scene/loaders/assimp_loader.hpp"

#include "renderer/vez/vez_backend.hpp"
#include "platform/win32_platform.hpp"

#include <chrono>
#include <thread>
#include <set>

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
    ASSERT_EQ(s.GetChildren(node).value(), std::set<NodeIndex>{});

    auto child_node = s.CreateNode(node).value();
    ASSERT_EQ(child_node, NodeIndex(2, 1));
    ASSERT_EQ(s.GetParent(child_node).value(), NodeIndex(1, 1));
}

TEST(SceneTest, CanDeleteNodes) {
    Scene s;

    auto node = s.CreateNode(s.GetRootNode()).value();
    ASSERT_EQ(s.GetChildren(s.GetRootNode()).value(),
              std::set<NodeIndex>{node});

    s.DeleteNode(node);

    ASSERT_EQ(s.GetChildren(s.GetRootNode()).value(), std::set<NodeIndex>{});
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

TEST(SceneTest, CanCreateAttachments) {
    Scene s;

    auto node = s.CreateNode(s.GetRootNode()).value();
    auto other_node = s.CreateNode(s.GetRootNode()).value();

    auto texture_result = s.CreateAttachment<Texture>({node}, {});
    ASSERT_TRUE(texture_result);

    auto texture = texture_result.value();
    auto attached_nodes = s.GetAttachedNodes<Texture>(texture).value();
    ASSERT_EQ(*attached_nodes, std::set<NodeIndex>({node}));

    s.Attach<Texture>(texture, other_node);
    ASSERT_EQ(*attached_nodes, std::set<NodeIndex>({node, other_node}));

    s.Detach<Texture>(texture, node);
    ASSERT_EQ(*attached_nodes, std::set<NodeIndex>({other_node}));

    s.Attach<Texture>(texture, node);
    ASSERT_EQ(*attached_nodes, std::set<NodeIndex>({node, other_node}));

    s.DetachAll<Texture>(texture);
    ASSERT_EQ(*attached_nodes, std::set<NodeIndex>());
}

TEST(SceneTest, CanCreateATexture) {
    Scene s;
    auto texture = s.CreateAttachment<Texture>(s.GetRootNode(), {});
    ASSERT_TRUE(texture);
}

TEST(AssimpLoaderTest, CanLoadAModel) {
    AssimpLoader loader;
    auto result =
        loader.ReadSceneFromFile("../../../assets/models/Duck/glTF/Duck.gltf");
    ASSERT_TRUE(result) << result.error().message();

    // Extract the unique_ptr from the result wrapper
    auto scene = std::move(result.value());

    EXPECT_EQ(scene->GetAttachmentCount<Texture>(), 1);
    EXPECT_EQ(scene->GetAttachmentCount<Material>(), 1);
    EXPECT_EQ(scene->GetAttachmentCount<Camera>(), 1);
    EXPECT_EQ(scene->GetAttachmentCount<Light>(), 0);
    EXPECT_EQ(scene->GetAttachmentCount<Mesh>(), 1);

    auto attached_nodes = scene->GetAttachedNodes<Mesh>({0}).value();
    EXPECT_EQ(*attached_nodes, std::set<NodeIndex>({1}));

    auto children = scene->GetChildren(scene->GetRootNode());
    ASSERT_TRUE(children);
    ASSERT_EQ(children.value().size(), 2);
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

TEST_F(VezBackendTest, RenderQuad) {
    Win32Platform platform;
    platform.InitWindow();

    auto init_surface_result = vez.InitSurface(&platform);
    ASSERT_TRUE(init_surface_result) << init_surface_result.error().message();

    std::vector<glm::vec3> positions = {{-0.5f, -0.5f, 0.0f},
                                        {0.5f, -0.5f, 0.0f},
                                        {-0.5f, 0.5f, 0.0f},
                                        {0.5f, 0.5f, 0.0f}};

    auto create_pos_buffer_result = vez.CreateVertexBuffer(
        {0}, "triangle_pos", positions.size() * sizeof(positions[0]), true,
        positions.data());
    ASSERT_TRUE(create_pos_buffer_result)
        << create_pos_buffer_result.error().message();

    std::vector<glm::vec2> uvs = {
        {0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f}};

    auto create_uv_buffer_result = vez.CreateVertexBuffer(
        {0}, "triangle_uvs", uvs.size() * sizeof(uvs[0]), true, uvs.data());
    ASSERT_TRUE(create_uv_buffer_result)
        << create_uv_buffer_result.error().message();

    std::vector<uint32_t> indices = {0, 1, 3, 0, 3, 2};

    auto create_index_buffer_result = vez.CreateIndexBuffer(
        {0}, "triangle_indices", indices.size() * sizeof(indices[0]), true,
        indices.data());
    ASSERT_TRUE(create_index_buffer_result)
        << create_index_buffer_result.error().message();

    auto vertex_input_format_result = vez.GetVertexInputFormat(
        {{{0, sizeof(glm::vec3)}, {1, sizeof(glm::vec2)}},
         {{0, 0, Format::SFloatRGB32, 0}, {1, 1, Format::SFloatRG32, 0}}});
    ASSERT_TRUE(vertex_input_format_result)
        << vertex_input_format_result.error().message();

    TextureDesc texture_desc = {512, 512};
    std::vector<std::array<uint8_t, 4>> pixels(512 * 512, {0, 0, 0, 255});
    for (uint32_t y = 0; y < 512; y++) {
        for (uint32_t x = 0; x < 512; x++) {
            pixels[512 * y + x][0] = x / 2;
            pixels[512 * y + x][1] = y / 2;
        }
    }

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
        vez.GetGraphicsPipeline({vertex_shader_glsl}, {fragment_shader_glsl});
    ASSERT_TRUE(create_pipeline_result)
        << create_pipeline_result.error().message();

    vez.RenderFrame(
        {[&](RenderPassDesc rp, FramebufferDesc fb, FrameIndex frame_id) {
            vez.BindGraphicsPipeline(*create_pipeline_result.value());
            vez.BindVertexInputFormat(*vertex_input_format_result.value());
            vez.BindVertexBuffers({*create_pos_buffer_result.value(),
                                   *create_uv_buffer_result.value()});
            vez.BindIndexBuffer(*create_index_buffer_result.value());
            vez.BindTextures({*create_texture_result.value()});
            vez.DrawIndexed(static_cast<uint32_t>(indices.size()));
            return outcome::success();
        }},
        "color");

    std::this_thread::sleep_for(std::chrono::seconds(1));
}

TEST_F(VezBackendTest, RenderModel) {
    Win32Platform platform;
    platform.InitWindow();

    auto init_surface_result = vez.InitSurface(&platform);
    ASSERT_TRUE(init_surface_result) << init_surface_result.error().message();

    AssimpLoader loader;
    auto result =
        loader.ReadSceneFromFile("../../../assets/models/Duck/glTF/Duck.gltf");
    ASSERT_TRUE(result) << result.error().message();

    // Extract the unique_ptr from the result wrapper
    auto scene = std::move(result.value());

    auto mesh = scene->GetAttachment<Mesh>({0}).value();
    auto create_pos_buffer_result = vez.CreateVertexBuffer(
        {0}, "triangle_pos", mesh->vertices.size() * sizeof(mesh->vertices[0]),
        true, mesh->vertices.data());
    ASSERT_TRUE(create_pos_buffer_result)
        << create_pos_buffer_result.error().message();

    auto create_uv_buffer_result = vez.CreateVertexBuffer(
        {0}, "triangle_uvs",
        mesh->uv_sets[0].size() * sizeof(mesh->uv_sets[0][0]), true,
        mesh->uv_sets[0].data());
    ASSERT_TRUE(create_uv_buffer_result)
        << create_uv_buffer_result.error().message();

    auto create_index_buffer_result =
        vez.CreateIndexBuffer({0}, "triangle_indices",
                              mesh->indices.size() * sizeof(mesh->indices[0]),
                              true, mesh->indices.data());
    ASSERT_TRUE(create_index_buffer_result)
        << create_index_buffer_result.error().message();

    auto vertex_input_format_result = vez.GetVertexInputFormat(
        {{{0, sizeof(glm::vec3)}, {1, sizeof(glm::vec2)}},
         {{0, 0, Format::SFloatRGB32, 0}, {1, 1, Format::SFloatRG32, 0}}});
    ASSERT_TRUE(vertex_input_format_result)
        << vertex_input_format_result.error().message();

    /*
TextureDesc texture_desc = {512, 512};
std::vector<std::array<uint8_t, 4>> pixels(512 * 512, {0, 0, 0, 255});
for (uint32_t y = 0; y < 512; y++) {
    for (uint32_t x = 0; x < 512; x++) {
        pixels[512 * y + x][0] = x / 2;
        pixels[512 * y + x][1] = y / 2;
    }
}
    */

    auto tex = scene->GetAttachment<Texture>({0}).value();
    TextureDesc texture_desc = {tex->width, tex->height};

    auto create_texture_result =
        vez.CreateTexture("texture", texture_desc, tex->data.data());
    ASSERT_TRUE(create_texture_result)
        << create_texture_result.error().message();

    static const char* vertex_shader_glsl = R"(
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUVs;

layout(location = 0) out vec2 outUVs;

layout(set = 0, binding = 1) uniform UBO {
	mat4 mvp;
} ubo;

void main() {
    gl_Position = ubo.mvp * vec4(inPosition, 1.0);
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
    outColor = texture(mainTexture, vec2(inUVs.x, -inUVs.y));
}
)";

    auto create_pipeline_result =
        vez.GetGraphicsPipeline({vertex_shader_glsl}, {fragment_shader_glsl});
    ASSERT_TRUE(create_pipeline_result)
        << create_pipeline_result.error().message();

    auto nodes = scene->GetAttachedNodes<Mesh>({0}).value();
    auto node = *nodes->begin();

    glm::mat4 model = glm::mat4(1.0f);

    auto transform = scene->GetTransform(node).value();
    model = glm::scale(model, transform.scale);
    model = glm::mat4_cast(transform.rotation) * model;
    model = glm::translate(model, transform.position);
    scene->SetTransform(node, transform);

    NodeIndex parent_node;
    while (node != scene->GetRootNode()) {
        parent_node = scene->GetParent(node).value();

        auto transform = scene->GetTransform(parent_node).value();
        model = glm::scale(model, transform.scale);
        model = glm::mat4_cast(transform.rotation) * model;
        model = glm::translate(model, transform.position);
        scene->SetTransform(parent_node, transform);

        node = parent_node;
    }

    // TODO check .end() - 1
    mesh->bounding_box = {
        {std::min_element(
             mesh->vertices.begin(), mesh->vertices.end(),
             [](const auto& v1, const auto& v2) { return (v1.x < v2.x); })
             ->x,
         std::min_element(
             mesh->vertices.begin(), mesh->vertices.end(),
             [](const auto& v1, const auto& v2) { return (v1.y < v2.y); })
             ->y,
         std::min_element(
             mesh->vertices.begin(), mesh->vertices.end(),
             [](const auto& v1, const auto& v2) { return (v1.z < v2.z); })
             ->z},
        {
            std::max_element(
                mesh->vertices.begin(), mesh->vertices.end(),
                [](const auto& v1, const auto& v2) { return (v1.x < v2.x); })
                ->x,
            std::max_element(
                mesh->vertices.begin(), mesh->vertices.end(),
                [](const auto& v1, const auto& v2) { return (v1.y < v2.y); })
                ->y,
            std::max_element(
                mesh->vertices.begin(), mesh->vertices.end(),
                [](const auto& v1, const auto& v2) { return (v1.z < v2.z); })
                ->z,
        }};

    Box transformed_bbox = {model * glm::vec4(mesh->bounding_box.min, 1.0f),
                            model * glm::vec4(mesh->bounding_box.max, 1.0f)};

    auto camera = scene->GetAttachment<Camera>({0}).value();

    // Let's draw a cone with the fov angle from the camera to the bounding box
    auto fovy = camera->h_fov * 600 /
                800;  // TODO implement the full formula in the renderer
    // TODO add absolute values (transformed might have min/max swapped)
    float dist = (transformed_bbox.max.x - transformed_bbox.min.x) /
                 (2 * tan(glm::radians(camera->h_fov)));
    dist = std::max(dist, (transformed_bbox.max.y - transformed_bbox.min.y) /
                              (2 * tan(glm::radians(fovy))));
    dist += camera->near_plane;
    dist += transformed_bbox.max.z;
    dist *= 3;

    glm::vec3 center = {(transformed_bbox.max.x + transformed_bbox.min.x) / 2,
                        (transformed_bbox.max.y + transformed_bbox.min.y) / 2,
                        (transformed_bbox.max.z + transformed_bbox.min.z) / 2};

    glm::mat4 view =
        glm::lookAt(center - glm::vec3(0, 0, dist), center, camera->up);
    glm::mat4 proj = glm::perspective(glm::radians(fovy), camera->aspect_ratio,
                                      camera->near_plane, camera->far_plane);
    proj[1][1] *= -1;

    uint64_t unif_offset = 256;
    auto create_unif_buffer_result =
        vez.CreateBuffer({}, 3 * unif_offset, VEZ_MEMORY_CPU_TO_GPU,
                         VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    ASSERT_TRUE(create_unif_buffer_result)
        << create_unif_buffer_result.error().message();

    auto mvp_buffer = create_unif_buffer_result.value();

    for (size_t i = 0; i < 100; i++) {
        vez.RenderFrame(
            {[&](RenderPassDesc rp, FramebufferDesc fb, FrameIndex frame_id) {
                vez.BindGraphicsPipeline(*create_pipeline_result.value());

                model = glm::rotate(model, glm::radians(2.0f), camera->up);
                glm::mat4 mvp = proj * view * model;
                vez.UpdateBuffer(*mvp_buffer, frame_id * unif_offset,
                                 sizeof(mvp), &mvp);

                vez.BindUniformBuffer(*mvp_buffer, frame_id * unif_offset,
                                      sizeof(mvp), 1, 0);
                vez.BindVertexInputFormat(*vertex_input_format_result.value());
                vez.BindVertexBuffers({*create_pos_buffer_result.value(),
                                       *create_uv_buffer_result.value()});
                vez.BindIndexBuffer(*create_index_buffer_result.value());
                vez.BindTextures({*create_texture_result.value()});

                VezDepthStencilState ds_state = {};
                ds_state.depthTestEnable = VK_TRUE;
                ds_state.depthCompareOp = VK_COMPARE_OP_LESS;
                ds_state.depthWriteEnable = VK_TRUE;
                vezCmdSetDepthStencilState(&ds_state);

                VezRasterizationState raster_state = {};
                raster_state.cullMode = VK_CULL_MODE_BACK_BIT;
                raster_state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
                vezCmdSetRasterizationState(&raster_state);

                vez.DrawIndexed(static_cast<uint32_t>(mesh->indices.size()));
                return outcome::success();
            }},
            "color");
    }
}

TEST(RendererTest, RenderDuck) {
    Engine e;
    e.LoadScene("../../../assets/models/Duck/glTF/Duck.gltf");
    auto res = e.renderer()->Render();

    ASSERT_TRUE(res) << res.error().message();

    std::this_thread::sleep_for(std::chrono::seconds(1));
}

TEST(RendererTest, RenderLantern) {
    Engine e;
    e.LoadScene("../../../assets/models/Lantern/glTF/Lantern.gltf");

    for (size_t i = 0; i < 300; i++) {
        auto res = e.renderer()->Render();
        ASSERT_TRUE(res) << res.error().message();
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
}

TEST(RendererTest, RenderSponza) {
    Engine e;
    e.LoadScene("../../../assets/models/Sponza/glTF/Sponza.gltf");

    for (size_t i = 0; i < 300; i++) {
        auto res = e.renderer()->Render();
        ASSERT_TRUE(res) << res.error().message();
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
}

TEST(EngineTest, RenderLantern) {
    Engine e;
    e.LoadScene("../../../assets/models/Lantern/glTF/Lantern.gltf");

    auto res = e.MainLoop([&]() {
        // Stop after 300 frames
        return e.frame_count() > 300;
    });
    ASSERT_TRUE(res) << res.error().message();

    std::this_thread::sleep_for(std::chrono::seconds(1));
}

}  // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}

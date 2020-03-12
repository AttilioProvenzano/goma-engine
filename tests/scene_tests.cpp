#include "goma_tests.hpp"

#include "scene/scene.hpp"
#include "scene/attachments/texture.hpp"
#include "scene/attachments/material.hpp"
#include "scene/attachments/camera.hpp"
#include "scene/attachments/light.hpp"
#include "scene/attachments/mesh.hpp"
#include "scene/utils.hpp"

#include "scene/loaders/assimp_loader.hpp"

using namespace goma;

namespace {

SCENARIO("nodes can be created and destroyed safely", "[scene][node]") {
    GIVEN("an empty scene") {
        Scene s;

        auto root_node = s.GetRootNode();
        REQUIRE(root_node == NodeIndex{0, 1});
        REQUIRE(s.GetChildren(root_node).value() == std::set<NodeIndex>{});

        WHEN("a node is added") {
            GOMA_TEST_TRY(node, s.CreateNode(root_node));

            THEN("it has index (1, 1), root as parent and no children") {
                CHECK(node == NodeIndex{1, 1});
                CHECK(s.GetParent(node).value() == root_node);
                CHECK(s.GetChildren(node).value() == std::set<NodeIndex>{});
            }

            AND_WHEN("that node is deleted") {
                s.DeleteNode(node);

                THEN(
                    "the root has no children and the node's parent is "
                    "invalid") {
                    CHECK(s.GetChildren(root_node).value() ==
                          std::set<NodeIndex>{});
                    CHECK(s.GetParent(node).has_error());
                }

                AND_WHEN("a new node is created in its place") {
                    GOMA_TEST_TRY(new_node, s.CreateNode(root_node));

                    THEN("the new node takes its index, with generation 2") {
                        CHECK(new_node == NodeIndex(1, 2));
                        CHECK(s.GetParent(new_node).value() == root_node);
                    }
                }
            }

            AND_WHEN("a child node is added to it") {
                GOMA_TEST_TRY(child_node, s.CreateNode(node));

                THEN(
                    "it has index (2, 1) and parent/child relationships are "
                    "set correctly") {
                    CHECK(child_node == NodeIndex{2, 1});
                    CHECK(s.GetParent(child_node).value() == NodeIndex{1, 1});
                    CHECK(s.GetChildren(node).value() ==
                          std::set<NodeIndex>{child_node});
                }

                AND_WHEN("the original node is deleted") {
                    s.DeleteNode(node);

                    THEN("the root node becomes the child node's parent") {
                        CHECK(s.GetParent(child_node).value() == root_node);
                    }
                }
            }
        }
    }
}

SCENARIO("attachments are handled correctly") {
    GIVEN("a scene with two nodes") {
        Scene s;

        auto root_node = s.GetRootNode();
        GOMA_TEST_TRY(node, s.CreateNode(s.GetRootNode()));
        GOMA_TEST_TRY(other_node, s.CreateNode(s.GetRootNode()));

        REQUIRE(s.GetChildren(root_node).value() ==
                std::set<NodeIndex>{node, other_node});

        WHEN("a texture is created") {
            GOMA_TEST_TRY(texture, s.CreateAttachment<Texture>({node}, {}));
            auto& attached_nodes =
                s.GetAttachedNodes<Texture>(texture).value().get();

            THEN("the set of attached nodes is correctly populated") {
                CHECK(attached_nodes == std::set<NodeIndex>({node}));
            }

            AND_WHEN("the texture is attached to a second node") {
                REQUIRE(s.Attach<Texture>(texture, other_node));

                THEN("the set of attached nodes has both nodes") {
                    CHECK(attached_nodes ==
                          std::set<NodeIndex>({node, other_node}));
                }

                AND_WHEN("it is detached from the first node") {
                    REQUIRE(s.Detach<Texture>(texture, node));

                    THEN("the set of attached nodes has only the second one") {
                        REQUIRE(attached_nodes ==
                                std::set<NodeIndex>({other_node}));
                    }
                }

                AND_WHEN("all nodes are detached") {
                    REQUIRE(s.DetachAll<Texture>(texture));

                    THEN("the set of attached nodes is empty") {
                        REQUIRE(attached_nodes == std::set<NodeIndex>({}));
                    }
                }
            }
        }
    }
}

SCENARIO("a model can be loaded via Assimp", "[scene][assimp]") {
    GIVEN("an Assimp loader") {
        AssimpLoader loader;

        WHEN("a scene is loaded") {
            GOMA_TEST_TRY(
                scene, loader.ReadSceneFromFile(GOMA_ASSETS_DIR
                                                "models/Duck/glTF/Duck.gltf"));

            THEN("node hierarchy and attachment counts are correct") {
                CHECK(scene->GetAttachmentCount<Texture>() == 1);
                CHECK(scene->GetAttachmentCount<Material>() == 1);
                CHECK(scene->GetAttachmentCount<Camera>() == 1);
                CHECK(scene->GetAttachmentCount<Light>() == 0);
                CHECK(scene->GetAttachmentCount<Mesh>() == 1);

                GOMA_TEST_TRY(attached_nodes,
                              scene->GetAttachedNodes<Mesh>({0}));
                CHECK(attached_nodes.get() == std::set<NodeIndex>({2}));

                GOMA_TEST_TRY(children,
                              scene->GetChildren(scene->GetRootNode()));
                CHECK(children.size() == 1);

                auto assimp_root_node = children.begin();
                GOMA_TEST_TRY(assimp_children,
                              scene->GetChildren(*assimp_root_node));
                CHECK(assimp_children.size() == 2);

                GOMA_TEST_TRY(mesh_ref, scene->GetAttachment<Mesh>(0));
                auto& mesh = mesh_ref.get();

                auto& layout = mesh.vertices.layout;
                auto stride = utils::GetStride(layout);

                // Check that the vertex buffer has the expected size
                CHECK(mesh.vertices.size == 2399);
                CHECK(mesh.vertices.layout.size() == 5);
                CHECK(mesh.vertices.data.size() == mesh.vertices.size * stride);

                // Check that sample vertex data match expected values
                auto sample_pos = reinterpret_cast<glm::vec3*>(
                    mesh.vertices.data.data() + 300 * stride +
                    utils::GetOffset(layout, VertexAttribute::Position));

                CHECK(sample_pos->x == Approx(45.6635971f));
                CHECK(sample_pos->y == Approx(56.4673004f));
                CHECK(sample_pos->z == Approx(46.2574005f));

                auto sample_uvs = reinterpret_cast<glm::vec2*>(
                    mesh.vertices.data.data() + 400 * stride +
                    utils::GetOffset(layout, VertexAttribute::UV0));

                CHECK(sample_uvs->x == Approx(0.929875970f));
                CHECK(sample_uvs->y == Approx(0.470180988f));
            }
        }
    }
}

}  // namespace

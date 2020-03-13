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

        auto& root_node = s.root_node();
        REQUIRE(root_node.children().empty());

        WHEN("a node is added") {
            auto& node = root_node.add_child({"Node"});

            THEN("parent/child relationships are set correctly") {
                CHECK(root_node.children().size() == 1);
                CHECK(*root_node.children()[0] == node);

                CHECK(node.parent() == &root_node);
                CHECK(node.children().empty());
                CHECK(node.name() == "Node");
            }

            AND_WHEN("a child node is added to it") {
                auto& child = node.add_child({"Child"});

                THEN("parent/child relationships are set correctly") {
                    CHECK(child.parent() == &node);
                    CHECK(*node.children()[0] == child);
                }

                AND_WHEN("the original node is deleted") {
                    root_node.remove_child(node);

                    THEN("the root has no children anymore") {
                        CHECK(root_node.children().empty());
                    }
                }
            }
        }
    }
}

SCENARIO("attachments are handled correctly") {
    GIVEN("a scene with two nodes") {
        Scene s;

        auto& root_node = s.root_node();
        auto& node = root_node.add_child();
        auto& other_node = root_node.add_child();

        REQUIRE(root_node.children().size() == 2);

        WHEN("a texture is created") {
            auto tex = Texture{};
            tex.attach_to(node);
            s.textures().push_back(std::move(tex));

            THEN("the set of attached nodes is correctly populated") {
                CHECK(tex.attached_nodes() == std::vector<Node*>{&node});
            }

            AND_WHEN("the texture is attached to a second node") {
                tex.attach_to(other_node);

                THEN("the set of attached nodes has both nodes") {
                    CHECK(tex.attached_nodes() ==
                          std::vector<Node*>{&node, &other_node});
                }

                AND_WHEN("it is detached from the first node") {
                    tex.detach_from(node);

                    THEN("the set of attached nodes has only the second one") {
                        CHECK(tex.attached_nodes() ==
                              std::vector<Node*>{&other_node});
                    }
                }

                AND_WHEN("all nodes are detached") {
                    tex.detach_all();

                    THEN("the set of attached nodes is empty") {
                        CHECK(tex.attached_nodes().empty());
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
                CHECK(scene->textures().size() == 2);
                CHECK(scene->materials().size() == 1);
                CHECK(scene->cameras().size() == 1);
                CHECK(scene->lights().size() == 0);
                CHECK(scene->meshes().size() == 1);

                CHECK(scene->root_node().children().size() == 2);

                REQUIRE(scene->meshes().is_valid({0, 0}));
                auto& mesh = scene->meshes().at({0, 0});

                REQUIRE(mesh.attached_nodes().size() == 1);
                CHECK(mesh.attached_nodes()[0]->name() == "nodes_2");

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

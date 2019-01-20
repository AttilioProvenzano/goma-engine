#include "gtest/gtest.h"

#include "scene/scene.hpp"

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

}  // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    system("pause");
    return ret;
}

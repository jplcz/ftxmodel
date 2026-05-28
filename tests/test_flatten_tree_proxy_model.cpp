#include <gtest/gtest.h>
#include <ftxmodel/flatten_tree_proxy_model.hpp>

using namespace ftxmodel;

class TreeFlattenMockModel : public AbstractItemModel {
 public:
  struct Node {
    std::string name;
    std::vector<std::shared_ptr<Node>> children;
  };

 private:
  std::shared_ptr<Node> m_root;

 public:
  TreeFlattenMockModel() {
    // Level 0 (Root context) -> Level 1 (Folders) -> Level 2 (Files)
    m_root = std::make_shared<Node>(
        "Root",
        std::vector<std::shared_ptr<Node>>{
            std::make_shared<Node>(
                "Folder_A",
                std::vector<std::shared_ptr<Node>>{
                    std::make_shared<Node>(
                        "File_A_1", std::vector<std::shared_ptr<Node>>{}),
                    std::make_shared<Node>(
                        "File_A_2", std::vector<std::shared_ptr<Node>>{})}),
            std::make_shared<Node>("Folder_B",
                                   std::vector<std::shared_ptr<Node>>{})});
  }

  ModelIndex index(int row,
                   int column,
                   const ModelIndex& parent = ModelIndex()) const override {
    if (row < 0 || column < 0) {
      return {};
    }
    const Node* parentNode =
        parent.isValid() ? static_cast<const Node*>(parent.internalPointer())
                         : m_root.get();
    if (row >= static_cast<int>(parentNode->children.size())) {
      return {};
    }
    return createIndex(row, column,
                       parentNode->children[static_cast<size_t>(row)].get());
  }

  ModelIndex parent(const ModelIndex& child) const override {
    if (!child.isValid()) {
      return {};
    }
    const Node* childNode = static_cast<const Node*>(child.internalPointer());

    // Scan Level 1 direct root children
    for (size_t i = 0; i < m_root->children.size(); ++i) {
      if (m_root->children[i].get() == childNode) {
        return {};
      }
      // Scan Level 2 grandchild files
      for (size_t j = 0; j < m_root->children[i]->children.size(); ++j) {
        if (m_root->children[i]->children[j].get() == childNode) {
          return createIndex(static_cast<int>(i), 0, m_root->children[i].get());
        }
      }
    }
    return {};
  }

  int rowCount(const ModelIndex& parent = ModelIndex()) const override {
    const Node* parentNode =
        parent.isValid() ? static_cast<const Node*>(parent.internalPointer())
                         : m_root.get();
    return static_cast<int>(parentNode->children.size());
  }

  int columnCount(const ModelIndex& parent = ModelIndex()) const override {
    return 1;
  }

  std::any data(const ModelIndex& index,
                ItemRole role = ItemRole::DisplayRole) const override {
    if (!index.isValid()) {
      return {};
    }
    const Node* node = static_cast<const Node*>(index.internalPointer());
    if (role == ItemRole::DisplayRole) {
      return node->name;
    }
    return {};
  }
};

class FlattenTreeProxyModelTest : public ::testing::Test {
 protected:
  std::shared_ptr<TreeFlattenMockModel> source_model;
  std::unique_ptr<FlattenTreeProxyModel> proxy_model;

  void SetUp() override {
    source_model = std::make_shared<TreeFlattenMockModel>();
    proxy_model = std::make_unique<FlattenTreeProxyModel>();
    proxy_model->setSourceModel(source_model);
  }
};

TEST_F(FlattenTreeProxyModelTest, DefaultStateShowsOnlyTopLevelRootNodes) {
  // Initially all branches are collapsed, so rowCount should only return Level
  // 1 elements
  EXPECT_EQ(proxy_model->rowCount(ModelIndex()), 2);  // Folder_A, Folder_B
  EXPECT_EQ(proxy_model->columnCount(ModelIndex()), 1);

  ModelIndex row0 = proxy_model->index(0, 0);
  ModelIndex row1 = proxy_model->index(1, 0);

  EXPECT_EQ(proxy_model->textData(row0), "Folder_A");
  EXPECT_EQ(proxy_model->textData(row1), "Folder_B");
}

TEST_F(FlattenTreeProxyModelTest,
       EnforcesLinearListContractByDenyingNestedQueries) {
  ModelIndex proxy_root_item = proxy_model->index(0, 0);

  // The proxy architecture contract demands that querying rowCount with a valid
  // parent index MUST return 0 because it tricks the view into seeing a flat 1D
  // list.
  EXPECT_EQ(proxy_model->rowCount(proxy_root_item), 0);

  // Similarly, parent() calls on any visual item must return an invalid empty
  // handle.
  EXPECT_FALSE(proxy_model->parent(proxy_root_item).isValid());
}

TEST_F(FlattenTreeProxyModelTest,
       DynamicExpansionBalloonsRowCountAndInjectsChildren) {
  // Verify initial closed states
  EXPECT_FALSE(proxy_model->isExpanded(0));

  // Expand Row 0 ("Folder_A"), which possesses 2 children ("File_A_1",
  // "File_A_2")
  proxy_model->expand(0);
  EXPECT_TRUE(proxy_model->isExpanded(0));

  // Visual layout size must increase: 2 initial roots + 2 nested files = 4 flat
  // list rows
  ASSERT_EQ(proxy_model->rowCount(ModelIndex()), 4);

  // Check the sequential flattened mapping pipeline:
  EXPECT_EQ(proxy_model->textData(proxy_model->index(0, 0)), "Folder_A");
  EXPECT_EQ(proxy_model->textData(proxy_model->index(1, 0)),
            "File_A_1");  // Squashed down!
  EXPECT_EQ(proxy_model->textData(proxy_model->index(2, 0)),
            "File_A_2");  // Squashed down!
  EXPECT_EQ(proxy_model->textData(proxy_model->index(3, 0)),
            "Folder_B");  // Pushed down to Row 3!
}

TEST_F(FlattenTreeProxyModelTest,
       DynamicCollapseCompressesListAndHidesChildren) {
  proxy_model->expand(0);
  ASSERT_EQ(proxy_model->rowCount(ModelIndex()), 4);

  // Re-collapse Row 0 to hide nested elements
  proxy_model->collapse(0);
  EXPECT_FALSE(proxy_model->isExpanded(0));

  // Grid row height metrics must cleanly shrink back to baseline
  EXPECT_EQ(proxy_model->rowCount(ModelIndex()), 2);
  EXPECT_EQ(proxy_model->textData(proxy_model->index(1, 0)),
            "Folder_B");  // Moves back to Row 1
}

TEST_F(FlattenTreeProxyModelTest,
       MapToSourceCorrectlyTracksHierarchicalPlacements) {
  proxy_model->expand(0);  // Flatten out branch

  // Target the visually flattened child sitting at flat linear List Row 1
  ModelIndex proxy_child_idx = proxy_model->index(1, 0);
  ASSERT_EQ(proxy_model->textData(proxy_child_idx), "File_A_1");

  // Run conversion to find true placement in original nested tree topology
  ModelIndex source_child_idx = proxy_model->mapToSource(proxy_child_idx);
  ASSERT_TRUE(source_child_idx.isValid());

  // Verify that its position inside the source model indicates it is at row 0
  // under its parent
  EXPECT_EQ(source_child_idx.row(), 0);

  // Read source parent link to verify depth structure
  ModelIndex source_parent_idx = source_model->parent(source_child_idx);
  ASSERT_TRUE(source_parent_idx.isValid());
  EXPECT_EQ(source_model->textData(source_parent_idx), "Folder_A");
}

TEST_F(FlattenTreeProxyModelTest,
       MapFromSourceReturnsInvalidTokenIfParentIsCollapsed) {
  // Locate a deep nested file directly inside the backend source dataset
  ModelIndex source_parent = source_model->index(0, 0);  // Folder_A
  ModelIndex source_child =
      source_model->index(0, 0, source_parent);  // File_A_1
  ASSERT_TRUE(source_child.isValid());

  // Because proxy_model remains collapsed at initialization, File_A_1 is
  // invisible to views
  ModelIndex proxy_invisible_idx = proxy_model->mapFromSource(source_child);

  // The contract demands that an un-rendered hidden node maps to an invalid
  // index token
  EXPECT_FALSE(proxy_invisible_idx.isValid());

  // Expand the branch, running the conversion pass again
  proxy_model->expand(0);
  ModelIndex proxy_visible_idx = proxy_model->mapFromSource(source_child);

  ASSERT_TRUE(proxy_visible_idx.isValid());
  EXPECT_EQ(proxy_visible_idx.row(),
            1);  // Discovered sitting on linear row index 1
}

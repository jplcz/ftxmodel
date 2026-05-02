#include <gtest/gtest.h>
#include <ftxmodel/sort_filter_proxy_model.hpp>
#include <memory>
#include "tree_mock_model.hpp"

using namespace ftxmodel;

class SortFilterProxyModelTest : public ::testing::Test {
 protected:
  std::shared_ptr<TreeMockModel> source_model;
  std::unique_ptr<SortFilterProxyModel> proxy_model;

  void SetUp() override {
    source_model = std::make_shared<TreeMockModel>();
    proxy_model = std::make_unique<SortFilterProxyModel>();
    proxy_model->setSourceModel(source_model);
  }
};

TEST_F(SortFilterProxyModelTest, DefaultStateIsPerfectIdentityPassthrough) {
  EXPECT_EQ(proxy_model->rowCount(ModelIndex()), 2);  // Fruit and Animal

  ModelIndex proxy_idx = proxy_model->index(0, 0, ModelIndex());
  EXPECT_EQ(proxy_model->textData(proxy_idx), "Fruit");

  // Matrix structural depths should align identically before filters hit
  ModelIndex proxy_fruit = proxy_model->index(0, 0, ModelIndex());
  EXPECT_EQ(proxy_model->rowCount(proxy_fruit), 2);  // Apple and Banana
}

TEST_F(SortFilterProxyModelTest, FilterStripsNonMatchingLeafNodes) {
  // Setup callback to only accept items containing the letter 'A' or 'a'
  proxy_model->setFilterCallback([&](const ModelIndex& index) -> bool {
    if (index.column() != 0) {
      return true;
    }
    std::string name = source_model->textData(index);
    return name.find('A') != std::string::npos ||
           name.find('a') != std::string::npos;
  });

  // Top level matches: "Fruit" (contains nothing, but has children matching
  // Apple/Banana) "Animal" (contains 'A') -> total = 2 rows
  EXPECT_EQ(proxy_model->rowCount(ModelIndex()), 2);

  // Check Fruit branch content adjustments
  ModelIndex proxy_fruit = proxy_model->index(0, 0, ModelIndex());
  ASSERT_EQ(proxy_model->textData(proxy_fruit), "Fruit");

  // Apple (accepted), Banana (accepted). Total = 2 rows.
  EXPECT_EQ(proxy_model->rowCount(proxy_fruit), 2);

  // Check Animal branch content adjustments
  ModelIndex proxy_animal = proxy_model->index(1, 0, ModelIndex());
  ASSERT_EQ(proxy_model->textData(proxy_animal), "Animal");

  // Zebra (contains 'a') -> total = 1 row
  EXPECT_EQ(proxy_model->rowCount(proxy_animal), 1);
}

TEST_F(SortFilterProxyModelTest, HierarchyPruningProtectionKeepsParentsAlive) {
  // Severe target condition: Filter strictly for "Banana"
  proxy_model->setFilterCallback([&](const ModelIndex& index) -> bool {
    if (index.column() != 0) {
      return true;
    }
    return source_model->textData(index) == "Banana";
  });

  // "Fruit" does NOT match "Banana", but because it contains "Banana" deeply
  // nested, the hasMatchingDescendants pipeline must keep it structurally
  // alive! "Animal" branch contains zero matches, so it must be completely
  // hidden.
  ASSERT_EQ(proxy_model->rowCount(ModelIndex()), 1);

  ModelIndex proxy_fruit = proxy_model->index(0, 0, ModelIndex());
  EXPECT_EQ(proxy_model->textData(proxy_fruit), "Fruit");

  // Under Fruit, Apple is dropped, Banana is visible -> count = 1
  ASSERT_EQ(proxy_model->rowCount(proxy_fruit), 1);
  EXPECT_EQ(proxy_model->textData(proxy_model->index(0, 0, proxy_fruit)),
            "Banana");
}

TEST_F(SortFilterProxyModelTest,
       SortsColumnZeroAlphabeticallyAscendingAndDescending) {
  // Default raw top row: Row 0 is "Fruit", Row 1 is "Animal"
  // Ascending Alphabetical sort on Column 0: "Animal" should switch to Row 0
  proxy_model->sort(0, true);

  ModelIndex first_row = proxy_model->index(0, 0, ModelIndex());
  EXPECT_EQ(proxy_model->textData(first_row), "Animal");

  // Descending Alphabetical sort on Column 0: "Fruit" shifts back to top row
  proxy_model->sort(0, false);
  first_row = proxy_model->index(0, 0, ModelIndex());
  EXPECT_EQ(proxy_model->textData(first_row), "Fruit");
}

TEST_F(SortFilterProxyModelTest, SortsColumnOneNumerically) {
  // 1. Sort by Column 1 Ascending (lowest weight first)
  proxy_model->sort(1, true);

  // Re-locate where "Fruit" is now sitting at the top level
  ModelIndex proxy_fruit_asc = proxy_model->findIndexById(
      UniqueNodeId{source_model->index(0, 0, ModelIndex()).internalPointer()});

  ModelIndex child_0 = proxy_model->index(0, 0, proxy_fruit_asc);
  ModelIndex child_1 = proxy_model->index(1, 0, proxy_fruit_asc);

  // Weights: Banana (2) < Apple (5)
  EXPECT_EQ(proxy_model->textData(child_0), "Banana");
  EXPECT_EQ(proxy_model->textData(child_1), "Apple");

  // 2. Sort by Column 1 Descending (highest weight first)
  proxy_model->sort(1, false);

  // Re-locate "Fruit" again, as the top-level order has flipped!
  ModelIndex proxy_fruit_desc = proxy_model->findIndexById(
      UniqueNodeId{source_model->index(0, 0, ModelIndex()).internalPointer()});

  child_0 = proxy_model->index(0, 0, proxy_fruit_desc);
  child_1 = proxy_model->index(1, 0, proxy_fruit_desc);

  // Weights: Apple (5) > Banana (2)
  EXPECT_EQ(proxy_model->textData(child_0), "Apple");
  EXPECT_EQ(proxy_model->textData(child_1), "Banana");
}

TEST_F(SortFilterProxyModelTest, BidirectionalCoordinateMapTranslation) {
  // Apply a sort that reverses raw rows
  proxy_model->sort(0, true);  // Row 0 is now Animal (Source Row 1)

  ModelIndex proxy_idx = proxy_model->index(0, 0, ModelIndex());
  ASSERT_EQ(proxy_model->textData(proxy_idx), "Animal");

  // Map to source
  ModelIndex source_idx = proxy_model->mapToSource(proxy_idx);
  ASSERT_TRUE(source_idx.isValid());
  EXPECT_EQ(source_idx.row(), 1);  // Confirms index mapping inversion

  // Map back from source
  ModelIndex verification_proxy_idx = proxy_model->mapFromSource(source_idx);
  EXPECT_EQ(proxy_idx, verification_proxy_idx);
}

TEST_F(SortFilterProxyModelTest,
       ResolvesParentChainCoordinatesAccuratelyAcrossSorts) {
  proxy_model->sort(0, true);  // Shuffle items layouts

  // Locate Zebra deep inside the proxy
  ModelIndex proxy_animal = proxy_model->index(0, 0, ModelIndex());
  ModelIndex proxy_zebra = proxy_model->index(0, 0, proxy_animal);
  ASSERT_TRUE(proxy_zebra.isValid());
  EXPECT_EQ(proxy_model->textData(proxy_zebra), "Zebra");

  // Call parent traversal override hook
  ModelIndex proxy_parent =
      proxy_zebra.parent();  // Invokes our new parent() function

  ASSERT_TRUE(proxy_parent.isValid());
  EXPECT_EQ(proxy_model->textData(proxy_parent), "Animal");
  EXPECT_EQ(proxy_parent, proxy_animal);
}

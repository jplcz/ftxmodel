#include <gtest/gtest.h>
#include <ftxmodel/identity_proxy_model.hpp>
#include <memory>
#include "mock_item_model.hpp"

using namespace ftxmodel;

class IdentityProxyModelTest : public ::testing::Test {
 protected:
  std::shared_ptr<MockItemModel> source_model;
  std::unique_ptr<IdentityProxyModel> proxy_model;

  void SetUp() override {
    source_model = std::make_shared<MockItemModel>();
    proxy_model = std::make_unique<IdentityProxyModel>();
    proxy_model->setSourceModel(source_model);
  }
};

// ========================================================================
// 1. BOUNDARY VALIDATION TESTS
// ========================================================================

TEST_F(IdentityProxyModelTest, ReturnsNullptrAndZeroWhenNoSourceModelBound) {
  IdentityProxyModel orphaned_proxy;

  EXPECT_EQ(orphaned_proxy.sourceModel(), nullptr);
  EXPECT_EQ(orphaned_proxy.rowCount(ModelIndex()), 0);
  EXPECT_EQ(orphaned_proxy.columnCount(ModelIndex()), 0);
  EXPECT_FALSE(orphaned_proxy.index(0, 0, ModelIndex()).isValid());
}

TEST_F(IdentityProxyModelTest, StructuralDimensionsMatchSourceModelPrecisely) {
  // Check Root count mapping: 2 items at top layer, columns map 1:1
  EXPECT_EQ(proxy_model->rowCount(ModelIndex()),
            source_model->rowCount(ModelIndex()));
  EXPECT_EQ(proxy_model->columnCount(ModelIndex()),
            source_model->columnCount(ModelIndex()));

  // Check Nested Branch mapping count
  ModelIndex proxy_parent = proxy_model->index(0, 0, ModelIndex());
  ModelIndex source_parent = source_model->index(0, 0, ModelIndex());

  EXPECT_EQ(proxy_model->rowCount(proxy_parent),
            source_model->rowCount(source_parent));
}

// ========================================================================
// 2. COORDINATE TRANSFORM SYMMETRY TESTS
// ========================================================================

TEST_F(IdentityProxyModelTest,
       TranslationMismatchesResolveInvalidIndicesGracefully) {
  ModelIndex empty_index;
  EXPECT_FALSE(proxy_model->mapToSource(empty_index).isValid());
  EXPECT_FALSE(proxy_model->mapFromSource(empty_index).isValid());
}

TEST_F(IdentityProxyModelTest, 1to1BidirectionalIndexCoordinateMapping) {
  ModelIndex proxy_idx = proxy_model->index(1, 0, ModelIndex());
  ASSERT_TRUE(proxy_idx.isValid());

  // Translate Proxy -> Source
  ModelIndex source_idx = proxy_model->mapToSource(proxy_idx);
  ASSERT_TRUE(source_idx.isValid());
  EXPECT_EQ(source_idx.row(), 1);
  EXPECT_EQ(source_idx.column(), 0);

  // Verify that the underlying index owner pointer points to the source model
  EXPECT_EQ(proxy_model->data(proxy_idx).type(), typeid(std::string));
  EXPECT_EQ(std::any_cast<std::string>(proxy_model->data(proxy_idx)), "Node_1");

  // Translate Source -> Proxy
  ModelIndex inverted_proxy_idx = proxy_model->mapFromSource(source_idx);
  EXPECT_EQ(proxy_idx, inverted_proxy_idx);
}

// ========================================================================
// 3. TREE TOPOLOGY TRAVERSAL TESTS
// ========================================================================

TEST_F(IdentityProxyModelTest, DeepHierarchicalTreeParentUpwardResolutions) {
  // Traverse deep down into child layer on proxy space: Node_0 -> Child_0_0
  ModelIndex proxy_parent = proxy_model->index(0, 0, ModelIndex());
  ModelIndex proxy_child = proxy_model->index(0, 0, proxy_parent);

  ASSERT_TRUE(proxy_child.isValid());
  EXPECT_EQ(proxy_model->textData(proxy_child), "Child_0_0");

  // Execute upward structural layout lookup
  ModelIndex discovered_proxy_parent =
      proxy_child.parent();  // Triggers inline index helper

  ASSERT_TRUE(discovered_proxy_parent.isValid());
  EXPECT_EQ(proxy_model->textData(discovered_proxy_parent), "Node_0");
  EXPECT_EQ(discovered_proxy_parent, proxy_parent);

  // Top layer parents must return invalid root-level tokens
  EXPECT_FALSE(discovered_proxy_parent.parent().isValid());
}

TEST_F(IdentityProxyModelTest,
       ReverseLookupIdentityQueriesRouteBypassingProxyLayers) {
  UniqueNodeId search_target = std::string("id_Child_0_0");

  // Ask the proxy to look up where this target data key is located visually
  ModelIndex found_proxy_index = proxy_model->findIndexById(search_target);

  ASSERT_TRUE(found_proxy_index.isValid());
  EXPECT_EQ(proxy_model->textData(found_proxy_index), "Child_0_0");

  // Verify that row locations align perfectly to proxy layout structures
  EXPECT_EQ(found_proxy_index.row(), 0);
}

// ========================================================================
// 4. SIGNAL PROPAGATION & RELAY TESTS
// ========================================================================

TEST_F(IdentityProxyModelTest,
       RelaysDataChangedSignalsSeamlesslyWithProxyIndices) {
  bool signal_intercepted = false;
  ModelIndex expected_proxy_target = proxy_model->index(1, 0, ModelIndex());

  // Connect slot observer directly onto proxy signal tracking channel
  auto connection = proxy_model->dataChanged.connect(
      [&](const ModelIndex& topLeft, const ModelIndex& bottomRight) {
        signal_intercepted = true;
        // Verify coordinates reaching the view layer have been accurately
        // re-mapped to proxy ownership
        EXPECT_EQ(topLeft, expected_proxy_target);
        EXPECT_EQ(bottomRight, expected_proxy_target);
      });

  // Simulate concrete model database or background alteration updates
  ModelIndex source_target = source_model->index(1, 0, ModelIndex());
  source_model->triggerItemChanged(source_target, source_target);

  EXPECT_TRUE(signal_intercepted);
}

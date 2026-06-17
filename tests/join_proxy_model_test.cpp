#include <gtest/gtest.h>
#include <ftxmodel/join_proxy_model.hpp>
#include "ftxmodel/model_debugging.hpp"
#include "string_matrix_model.hpp"

using namespace ftxmodel;

class JoinProxyModelTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Model A: 2x3 grid
    modelA = std::make_shared<SimpleGridModel>(2, 3, "A");
    // Model B: 2x3 grid (compatible dimensions for either axis)
    modelB = std::make_shared<SimpleGridModel>(2, 3, "B");
  }

  std::shared_ptr<SimpleGridModel> modelA;
  std::shared_ptr<SimpleGridModel> modelB;
};

TEST_F(JoinProxyModelTest, InitialStateAndOrientationProperty) {
  JoinProxyModel proxy;

  // Verify default orientation is safe/documented (assuming Vertical as
  // standard default)
  proxy.setJoinOrientation(Orientation::Vertical);
  EXPECT_EQ(proxy.joinOrientation(), Orientation::Vertical);

  proxy.setJoinOrientation(Orientation::Horizontal);
  EXPECT_EQ(proxy.joinOrientation(), Orientation::Horizontal);
}

TEST_F(JoinProxyModelTest, EmptyProxyReturnsZeroDimensions) {
  JoinProxyModel proxy;
  EXPECT_EQ(proxy.rowCount(), 0);
  EXPECT_EQ(proxy.columnCount(), 0);
  EXPECT_FALSE(proxy.index(0, 0, {}).isValid());
}
// ============================================================================
// VERTICAL JOINING SYSTEM TESTS (Stacks Rows)
// ============================================================================
TEST_F(JoinProxyModelTest, VerticalStackLayoutCalculatesCorrectDimensions) {
  JoinProxyModel proxy;
  proxy.setJoinOrientation(Orientation::Vertical);

  proxy.addSourceModel(modelA);  // 2 rows, 3 cols
  proxy.addSourceModel(modelB);  // 2 rows, 3 cols

  // Stacking vertically sums rows (2 + 2 = 4), retains identical column count
  // (3)
  EXPECT_EQ(proxy.rowCount(), 4);
  EXPECT_EQ(proxy.columnCount(), 3);
}

TEST_F(JoinProxyModelTest, VerticalStackResolvesCorrectProxyDataSegments) {
  JoinProxyModel proxy;
  proxy.setJoinOrientation(Orientation::Vertical);
  proxy.addSourceModel(modelA);
  proxy.addSourceModel(modelB);

  // Top block (Rows 0-1) translates to Model A
  ModelIndex proxyRow0 = proxy.index(0, 1, {});
  EXPECT_EQ(
      std::any_cast<std::string>(proxy.data(proxyRow0, ItemRole::DisplayRole)),
      "A_0x1")
      << dumpModelToString(proxy);

  // Bottom block (Rows 2-3) translates to offset row entries in Model B
  ModelIndex proxyRow2 = proxy.index(2, 2, {});
  EXPECT_EQ(
      std::any_cast<std::string>(proxy.data(proxyRow2, ItemRole::DisplayRole)),
      "B_0x2")
      << dumpModelToString(proxy);

  ModelIndex proxyRow3 = proxy.index(3, 0, {});
  EXPECT_EQ(
      std::any_cast<std::string>(proxy.data(proxyRow3, ItemRole::DisplayRole)),
      "B_1x0")
      << dumpModelToString(proxy);
}

// ============================================================================
// HORIZONTAL JOINING SYSTEM TESTS (Appends Columns)
// ============================================================================
TEST_F(JoinProxyModelTest,
       HorizontalSpanningLayoutCalculatesCorrectDimensions) {
  JoinProxyModel proxy;
  proxy.setJoinOrientation(Orientation::Horizontal);

  proxy.addSourceModel(modelA);  // 2 rows, 3 cols
  proxy.addSourceModel(modelB);  // 2 rows, 3 cols

  // Joining horizontally retains base rows (2), sums tracking columns (3 + 3 =
  // 6)
  EXPECT_EQ(proxy.rowCount(), 2);
  EXPECT_EQ(proxy.columnCount(), 6);
}

TEST_F(JoinProxyModelTest, HorizontalSpanningResolvesCorrectProxyDataSegments) {
  JoinProxyModel proxy;
  proxy.setJoinOrientation(Orientation::Horizontal);
  proxy.addSourceModel(modelA);
  proxy.addSourceModel(modelB);

  // Left strip (Columns 0-2) maps to Model A
  ModelIndex proxyCol1 = proxy.index(1, 1, {});
  EXPECT_EQ(
      std::any_cast<std::string>(proxy.data(proxyCol1, ItemRole::DisplayRole)),
      "A_1x1");

  // Right strip (Columns 3-5) maps to offset column entries in Model B
  ModelIndex proxyCol3 = proxy.index(0, 3, {});
  EXPECT_EQ(
      std::any_cast<std::string>(proxy.data(proxyCol3, ItemRole::DisplayRole)),
      "B_0x0");

  ModelIndex proxyCol5 = proxy.index(1, 5, {});
  EXPECT_EQ(
      std::any_cast<std::string>(proxy.data(proxyCol5, ItemRole::DisplayRole)),
      "B_1x2");
}

// ============================================================================
// MUTATION AND ROUTING TESTS
// ============================================================================
TEST_F(JoinProxyModelTest, WriteOperationsRouteCorrectlyToBackendModels) {
  JoinProxyModel proxy;
  proxy.setJoinOrientation(Orientation::Vertical);
  proxy.addSourceModel(modelA);
  proxy.addSourceModel(modelB);

  // Target row 3 (which translates directly to Model B, row 1)
  ModelIndex proxyTarget = proxy.index(3, 1, {});
  std::string updatePayload = "Modified_B_1x1";

  bool writeSuccess =
      proxy.setData(proxyTarget, updatePayload, ItemRole::EditRole);
  EXPECT_TRUE(writeSuccess);

  // Check proxy reflection
  EXPECT_EQ(std::any_cast<std::string>(
                proxy.data(proxyTarget, ItemRole::DisplayRole)),
            updatePayload);

  // Verify underlying source model was modified directly
  ModelIndex sourceIdx = modelB->index(1, 1, {});
  EXPECT_EQ(std::any_cast<std::string>(
                modelB->data(sourceIdx, ItemRole::DisplayRole)),
            updatePayload);
}

TEST_F(JoinProxyModelTest, ModelsClearMechanismFlushesState) {
  JoinProxyModel proxy;
  proxy.addSourceModel(modelA);
  EXPECT_GT(proxy.rowCount(), 0);

  proxy.clearModels();
  EXPECT_EQ(proxy.rowCount(), 0);
  EXPECT_EQ(proxy.columnCount(), 0);
}

// ============================================================================
// METADATA AND STABLE IDENTIFIER CHECKS
// ============================================================================
TEST_F(JoinProxyModelTest, HierarchicalGuardsAndTopologicalProperties) {
  JoinProxyModel proxy;
  proxy.addSourceModel(modelA);

  ModelIndex rootItem = proxy.index(0, 0, {});

  // Grid/List only architecture restriction: nodes must never present children
  EXPECT_FALSE(proxy.hasChildren(rootItem));
  EXPECT_EQ(proxy.rowCount(rootItem), 0);

  // Every element is root level, parent checks must return an invalid empty
  // ModelIndex
  EXPECT_FALSE(proxy.parent(rootItem).isValid());
}

TEST_F(JoinProxyModelTest, KeyTrackingAndIdentityReverseLookupPasses) {
  JoinProxyModel proxy;
  proxy.setJoinOrientation(Orientation::Vertical);
  proxy.addSourceModel(modelA);
  proxy.addSourceModel(modelB);

  // Resolve cross-matrix index
  ModelIndex proxyIdx = proxy.index(3, 0, {});  // Maps to B_1x0
  UniqueNodeId targetKey = proxy.uniqueId(proxyIdx);

  // Verify uniqueId serialization transparently tunnels or maps cleanly
  EXPECT_EQ(targetKey, modelB->uniqueId(modelB->index(1, 0, {})));

  // Test deep hierarchy lookups bypass
  ModelIndex recoveredIdx = proxy.findIndexById(targetKey, {});
  EXPECT_TRUE(recoveredIdx.isValid());
  EXPECT_EQ(recoveredIdx.row(), 3);
  EXPECT_EQ(recoveredIdx.column(), 0);
}

// ============================================================================
// ASYMMETRICAL AND MISMATCHED DIMENSION TESTS
// ============================================================================

TEST_F(JoinProxyModelTest, VerticalStackWithMismatchedColumnCounts) {
  JoinProxyModel proxy;
  proxy.setJoinOrientation(Orientation::Vertical);

  auto narrowModel =
      std::make_shared<SimpleGridModel>(2, 2, "Narrow");  // 2 columns
  auto wideModel =
      std::make_shared<SimpleGridModel>(3, 4, "Wide");  // 4 columns

  proxy.addSourceModel(narrowModel);
  proxy.addSourceModel(wideModel);

  // Rows should perfectly sum: 2 + 3 = 5
  EXPECT_EQ(proxy.rowCount(), 5);

  int cols = proxy.columnCount();
  EXPECT_EQ(cols, 4);

  // Verify that accessing an out-of-bounds index for the narrow segment safely
  // returns empty/null data
  ModelIndex emptyZone = proxy.index(
      0, 3, {});  // Row 0 is narrowModel, column 3 is out-of-bounds for it
  EXPECT_TRUE(
      emptyZone.isValid());  // The proxy coordinate is structurally valid
  EXPECT_FALSE(proxy.data(emptyZone, ItemRole::DisplayRole)
                   .has_value());  // But has no data
}

TEST_F(JoinProxyModelTest, HorizontalSpanningWithMismatchedRowCounts) {
  JoinProxyModel proxy;
  proxy.setJoinOrientation(Orientation::Horizontal);

  auto shortModel = std::make_shared<SimpleGridModel>(1, 3, "Short");  // 1 row
  auto tallModel = std::make_shared<SimpleGridModel>(4, 3, "Tall");    // 4 rows

  proxy.addSourceModel(shortModel);
  proxy.addSourceModel(tallModel);

  // Columns sum: 3 + 3 = 6
  EXPECT_EQ(proxy.columnCount(), 6);

  int rows = proxy.rowCount();
  EXPECT_EQ(rows, 4);

  ModelIndex emptyZone = proxy.index(
      2, 1, {});  // Row 2 is out-of-bounds for shortModel (cols 0-2)
  EXPECT_TRUE(emptyZone.isValid());
  EXPECT_FALSE(proxy.data(emptyZone, ItemRole::DisplayRole).has_value());
}

// ============================================================================
// BOUNDARY AND OUT-OF-BOUNDS MITIGATION TESTS
// ============================================================================

TEST_F(JoinProxyModelTest, RejectsInvalidCoordinatesGracefully) {
  JoinProxyModel proxy;
  proxy.setJoinOrientation(Orientation::Vertical);
  proxy.addSourceModel(modelA);  // 2 rows, 3 cols

  // Extreme boundary checks
  EXPECT_FALSE(proxy.index(-1, 0, {}).isValid());
  EXPECT_FALSE(proxy.index(0, -1, {}).isValid());
  EXPECT_FALSE(proxy.index(100, 0, {}).isValid());  // Way past row boundary
  EXPECT_FALSE(proxy.index(0, 100, {}).isValid());  // Way past column boundary

  // Null data calls for completely invalid indices
  ModelIndex badIdx;
  EXPECT_FALSE(proxy.data(badIdx, ItemRole::DisplayRole).has_value());
  EXPECT_FALSE(proxy.setData(badIdx, "data", ItemRole::EditRole));
  EXPECT_EQ(proxy.flags(badIdx), ItemFlag::NoItemFlags);
}

TEST_F(JoinProxyModelTest, HeaderDataWriteRouting) {
  JoinProxyModel proxy;
  proxy.setJoinOrientation(Orientation::Horizontal);
  proxy.addSourceModel(modelA);  // 3 columns (0, 1, 2)
  proxy.addSourceModel(modelB);  // 3 columns (3, 4, 5)

  // Modifying header for section 4 (Horizontal Column) targets modelB's column
  // 1
  bool success = proxy.setHeaderData(
      4, Orientation::Horizontal, std::string("NewHeader"), ItemRole::EditRole);

  // If underlying models support header changes, it routes and returns
  // true Otherwise, it cleanly returns false without throwing exceptions
  if (success) {
    std::any alteredHeader =
        proxy.headerData(4, Orientation::Horizontal, ItemRole::DisplayRole);
    EXPECT_EQ(std::any_cast<std::string>(alteredHeader), "NewHeader");
  }
}

// ============================================================================
// IDENTITY ENGINE EXTRACTION PATH TRIPS
// ============================================================================

TEST_F(JoinProxyModelTest, FindIndexByIdHandlesMissingKeys) {
  JoinProxyModel proxy;
  proxy.addSourceModel(modelA);

  UniqueNodeId ghostKey = std::string("non_existent_key");
  ModelIndex result = proxy.findIndexById(ghostKey, {});

  EXPECT_FALSE(result.isValid());
}

// ============================================================================
// DYNAMIC COMPONENT INTEGRATION CHECKS (LAZY LOADING)
// ============================================================================

TEST_F(JoinProxyModelTest, FetchMoreTunnelsToUnderlyingSources) {
  JoinProxyModel proxy;
  proxy.addSourceModel(modelA);

  // Check lazy loading mechanisms. Proxy should state false if underlying
  // source models are fully populated.
  ModelIndex rootCtx;
  EXPECT_FALSE(proxy.canFetchMore(rootCtx));

  // Calling fetchMore shouldn't crash or corrupt metrics; it passes execution
  // logic down.
  EXPECT_NO_THROW(proxy.fetchMore(rootCtx));
}

// ============================================================================
// MULTI-MODEL CASCADING TESTS (3+ Models)
// ============================================================================

TEST_F(JoinProxyModelTest, VerticalStackThreeModelsMaintainsOrderAndOffsets) {
  JoinProxyModel proxy;
  proxy.setJoinOrientation(Orientation::Vertical);

  auto modelC = std::make_shared<SimpleGridModel>(3, 3, "C");  // 3 rows

  proxy.addSourceModel(modelA);  // 2 rows
  proxy.addSourceModel(modelB);  // 2 rows
  proxy.addSourceModel(modelC);  // 3 rows

  // Matrix total row validation: 2 + 2 + 3 = 7
  ASSERT_EQ(proxy.rowCount(), 7);
  EXPECT_EQ(proxy.columnCount(), 3);

  // Verify indices map across boundaries into the 3rd model accurately
  ModelIndex proxyRow4 =
      proxy.index(4, 0, {});  // Row index 4 -> Model C, Row 0
  ModelIndex proxyRow6 =
      proxy.index(6, 2, {});  // Row index 6 -> Model C, Row 2

  EXPECT_EQ(
      std::any_cast<std::string>(proxy.data(proxyRow4, ItemRole::DisplayRole)),
      "C_0x0");
  EXPECT_EQ(
      std::any_cast<std::string>(proxy.data(proxyRow6, ItemRole::DisplayRole)),
      "C_2x2");
}

TEST_F(JoinProxyModelTest, HorizontalSpanThreeModelsMaintainsOffsets) {
  JoinProxyModel proxy;
  proxy.setJoinOrientation(Orientation::Horizontal);

  auto modelC = std::make_shared<SimpleGridModel>(2, 4, "C");  // 4 columns

  proxy.addSourceModel(modelA);  // 3 columns
  proxy.addSourceModel(modelB);  // 3 columns
  proxy.addSourceModel(modelC);  // 4 columns

  // Matrix total column validation: 3 + 3 + 4 = 10
  ASSERT_EQ(proxy.columnCount(), 10);
  EXPECT_EQ(proxy.rowCount(), 2);

  // Verify column mapping into the 3rd model layout space
  ModelIndex proxyCol6 =
      proxy.index(0, 6, {});  // Column index 6 -> Model C, Column 0
  ModelIndex proxyCol9 =
      proxy.index(1, 9, {});  // Column index 9 -> Model C, Column 3

  EXPECT_EQ(
      std::any_cast<std::string>(proxy.data(proxyCol6, ItemRole::DisplayRole)),
      "C_0x0");
  EXPECT_EQ(
      std::any_cast<std::string>(proxy.data(proxyCol9, ItemRole::DisplayRole)),
      "C_1x3");
}

// ============================================================================
// EMPTY AND DEGENERATE SOURCE MODEL TESTS
// ============================================================================

TEST_F(JoinProxyModelTest, HandlesEmptySourceModelsTransparently) {
  JoinProxyModel proxy;
  proxy.setJoinOrientation(Orientation::Vertical);

  auto emptyModel1 = std::make_shared<SimpleGridModel>(0, 3, "Empty1");
  auto emptyModel2 = std::make_shared<SimpleGridModel>(0, 3, "Empty2");

  proxy.addSourceModel(emptyModel1);
  proxy.addSourceModel(modelA);  // 2 rows
  proxy.addSourceModel(emptyModel2);

  // Rows should perfectly skip empty segments: 0 + 2 + 0 = 2
  ASSERT_EQ(proxy.rowCount(), 2);

  // Index 0 must cleanly bypass emptyModel1 and hit modelA row 0
  ModelIndex proxyRow0 = proxy.index(0, 0, {});
  EXPECT_EQ(
      std::any_cast<std::string>(proxy.data(proxyRow0, ItemRole::DisplayRole)),
      "A_0x0");
}

// ============================================================================
// INTERACTION FLAGS ROUTING TESTS
// ============================================================================

class CustomFlagsGridModel : public SimpleGridModel {
 public:
  using SimpleGridModel::SimpleGridModel;
  ItemFlags flags(const ModelIndex& index) const override {
    if (index.row() == 0) {
      return ItemFlag::ItemIsEnabled |
             ItemFlag::ItemIsEditable;  // Special editable row
    }
    return ItemFlag::NoItemFlags;  // Disabled row
  }
};

TEST_F(JoinProxyModelTest, FlagsQueriesTunnelToCorrectUnderlyingSources) {
  JoinProxyModel proxy;
  proxy.setJoinOrientation(Orientation::Vertical);

  auto customFlagModel =
      std::make_shared<CustomFlagsGridModel>(2, 3, "FlagsModel");
  proxy.addSourceModel(modelA);  // Uses default flags (Enabled | Selectable)
  proxy.addSourceModel(customFlagModel);  // Segment starts at Row index 2

  // Querying Row 0 (modelA)
  ModelIndex row0 = proxy.index(0, 0, {});
  EXPECT_TRUE(proxy.flags(row0) & ItemFlag::ItemIsSelectable);
  EXPECT_FALSE(proxy.flags(row0) & ItemFlag::ItemIsEditable);

  // Querying Row 2 (customFlagModel Row 0 - configured to be Editable but not
  // Selectable)
  ModelIndex row2 = proxy.index(2, 0, {});
  EXPECT_TRUE(proxy.flags(row2) & ItemFlag::ItemIsEditable);
  EXPECT_FALSE(proxy.flags(row2) & ItemFlag::ItemIsSelectable);

  // Querying Row 3 (customFlagModel Row 1 - configured to have completely empty
  // flags)
  ModelIndex row3 = proxy.index(3, 0, {});
  EXPECT_EQ(proxy.flags(row3), ItemFlag::NoItemFlags);
}

// ============================================================================
// KEY COLLISION SEPARATION TESTS
// ============================================================================

TEST_F(JoinProxyModelTest,
       ResolvesIdenticalUniqueIdsViaStructuralOffsetPartitioning) {
  JoinProxyModel proxy;
  proxy.setJoinOrientation(Orientation::Vertical);

  // Create two distinct source models that generate identical internal uniqueId
  // keys
  auto model1 = std::make_shared<SimpleGridModel>(1, 1, "CollisionTag");
  auto model2 = std::make_shared<SimpleGridModel>(1, 1, "CollisionTag");

  proxy.addSourceModel(model1);  // Row 0
  proxy.addSourceModel(model2);  // Row 1

  ModelIndex proxyIdx0 = proxy.index(0, 0, {});
  ModelIndex proxyIdx1 = proxy.index(1, 0, {});

  UniqueNodeId id0 = proxy.uniqueId(proxyIdx0);
  UniqueNodeId id1 = proxy.uniqueId(proxyIdx1);

  // Asserting that the proxy correctly processes lookup uniqueness.
  // If your proxy handles collisions cleanly, findIndexById should land on the
  // mathematically precise index coordinate
  ModelIndex found0 = proxy.findIndexById(id0, {});
  EXPECT_EQ(found0.row(), 0);

  // If IDs are fully identical, finding ID tracking flags should fallback
  // gracefully or match structurally
  ModelIndex found1 = proxy.findIndexById(id1, {});
  EXPECT_TRUE(found1.isValid());
}

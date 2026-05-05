#include <gtest/gtest.h>
#include <ftxmodel/concat_proxy_model.hpp>

using namespace ftxmodel;

class MockItemModel : public AbstractItemModel {
 public:
  int rows = 0;
  int cols = 0;
  std::string name_prefix;

  MockItemModel(const int r, const int c, std::string prefix)
      : rows(r), cols(c), name_prefix(std::move(prefix)) {}

  int rowCount(const ModelIndex& parent = ModelIndex()) const override {
    return parent.isValid() ? 0 : rows;
  }

  int columnCount(const ModelIndex& parent = ModelIndex()) const override {
    return parent.isValid() ? 0 : cols;
  }

  ModelIndex index(const int row,
                   const int column,
                   const ModelIndex& parent = ModelIndex()) const override {
    if (parent.isValid() || row < 0 || row >= rows || column < 0 ||
        column >= cols) {
      return {};
    }
    // Emulate unique internal pointers using dummy staggered pointers
    return createIndex(row, column,
                       reinterpret_cast<void*>(
                           static_cast<uintptr_t>(row * 100 + column + 1)));
  }

  ModelIndex parent(const ModelIndex&) const override { return {}; }

  std::any data(const ModelIndex& index, const ItemRole role) const override {
    if (!index.isValid() || role != ItemRole::DisplayRole) {
      return {};
    }
    return name_prefix + std::format("_{}_{}", index.row(), index.column());
  }

  UniqueNodeId uniqueId(const ModelIndex& index) const override {
    if (!index.isValid()) {
      return {nullptr};
    }
    std::string result = std::format("{}_ID_{}", name_prefix, index.row());
    return {result};
  }

  // Helper triggers to simulate mutations manually
  void triggerInsertRows(int start, int end) {
    beginInsertRows(ModelIndex(), start, end);
    rows += (end - start + 1);
    endInsertRows();
  }

  void triggerRemoveRows(int start, int end) {
    beginRemoveRows(ModelIndex(), start, end);
    rows -= (end - start + 1);
    endRemoveRows();
  }

  void triggerReset() {
    beginResetModel();
    endResetModel();
  }
};

class ConcatProxyModelTest : public ::testing::Test {
 protected:
  std::shared_ptr<MockItemModel> modelA;
  std::shared_ptr<MockItemModel> modelB;
  std::shared_ptr<MockItemModel> modelC;
  ConcatProxyModel proxy;

  void SetUp() override {
    modelA = std::make_shared<MockItemModel>(3, 2, "A");  // 3x2 matrix
    modelB = std::make_shared<MockItemModel>(2, 2, "B");  // 2x2 matrix
    modelC = std::make_shared<MockItemModel>(4, 2, "C");  // 4x2 matrix
  }

  void TearDown() override {}
};

// --- Test Group A: Geometry & Basic Dimensions ---

TEST_F(ConcatProxyModelTest, VerticalOrientationCounts) {
  proxy.setSourceModels({modelA, modelB, modelC}, Orientation::Vertical);

  // Rows should aggregate (3 + 2 + 4 = 9)
  EXPECT_EQ(proxy.rowCount(), 9);
  // Columns should map to maximum footprint available (all are 2)
  EXPECT_EQ(proxy.columnCount(), 2);
}

TEST_F(ConcatProxyModelTest, HorizontalOrientationCounts) {
  proxy.setSourceModels({modelA, modelB, modelC}, Orientation::Horizontal);

  // Rows should scale to max footprint available (max of 3, 2, 4 = 4)
  EXPECT_EQ(proxy.rowCount(), 4);
  // Columns should aggregate (2 + 2 + 2 = 6)
  EXPECT_EQ(proxy.columnCount(), 6);
}

TEST_F(ConcatProxyModelTest, HandlesEmptyModelVectors_Safely_V) {
  proxy.setSourceModels({}, Orientation::Vertical);
  EXPECT_EQ(proxy.rowCount(), 0);
  EXPECT_EQ(proxy.columnCount(), 0);
  EXPECT_FALSE(proxy.index(0, 0).isValid());
}

TEST_F(ConcatProxyModelTest, HandlesEmptyModelVectors_Safely_H) {
  proxy.setSourceModels({}, Orientation::Horizontal);
  EXPECT_EQ(proxy.rowCount(), 0);
  EXPECT_EQ(proxy.columnCount(), 0);
  EXPECT_FALSE(proxy.index(0, 0).isValid());
}

TEST_F(ConcatProxyModelTest, VerticalIndexMappingPasses) {
  proxy.setSourceModels({modelA, modelB, modelC}, Orientation::Vertical);

  // Visual Row 1 -> Maps to Model A, Row 1
  ModelIndex idxA = proxy.index(1, 0);
  EXPECT_TRUE(idxA.isValid());
  EXPECT_EQ(std::any_cast<std::string>(proxy.data(idxA)), "A_1_0");

  // Visual Row 4 -> Maps to Model B, Row 1 (4 - 3 rows of Model A = 1)
  ModelIndex idxB = proxy.index(4, 1);
  EXPECT_TRUE(idxB.isValid());
  EXPECT_EQ(std::any_cast<std::string>(proxy.data(idxB)), "B_1_1");

  // Visual Row 7 -> Maps to Model C, Row 2 (7 - 3 - 2 = 2)
  ModelIndex idxC = proxy.index(7, 0);
  EXPECT_TRUE(idxC.isValid());
  EXPECT_EQ(std::any_cast<std::string>(proxy.data(idxC)), "C_2_0");

  // Boundary overflow check
  EXPECT_FALSE(proxy.index(9, 0).isValid());
}

TEST_F(ConcatProxyModelTest, HorizontalIndexMappingPasses) {
  proxy.setSourceModels({modelA, modelB, modelC}, Orientation::Horizontal);

  // Visual Col 1 -> Model A, Col 1
  ModelIndex idxA = proxy.index(0, 1);
  EXPECT_EQ(std::any_cast<std::string>(proxy.data(idxA)), "A_0_1");

  // Visual Col 3 -> Model B, Col 1 (3 - 2 cols of Model A = 1)
  ModelIndex idxB = proxy.index(1, 3);
  EXPECT_EQ(std::any_cast<std::string>(proxy.data(idxB)), "B_1_1");

  // Boundary check: Model B only has 2 rows. Querying proxy row 3 on Model B's
  // columns should return null/empty
  ModelIndex emptyRowB = proxy.index(3, 3);
  EXPECT_EQ(proxy.data(emptyRowB).type(), typeid(void));  // empty std::any
}

TEST_F(ConcatProxyModelTest, UniqueIdResolutionWorks) {
  proxy.setSourceModels({modelA, modelB}, Orientation::Vertical);

  UniqueNodeId idA = proxy.uniqueId(proxy.index(1, 0));
  UniqueNodeId idB = proxy.uniqueId(proxy.index(4, 0));  // Model B Row 1

  EXPECT_EQ(std::get<std::string>(idA), "A_ID_1");
  EXPECT_EQ(std::get<std::string>(idB), "B_ID_1");
}

TEST_F(ConcatProxyModelTest, FindIndexByIdTraversesSequentially) {
  proxy.setSourceModels({modelA, modelB, modelC}, Orientation::Vertical);

  const UniqueNodeId target(
      std::string("C_ID_2"));  // Model C, Row 2 -> Globally row 7 (3 + 2 + 2)
  ModelIndex found = proxy.findIndexById(target);

  EXPECT_TRUE(found.isValid());
  EXPECT_EQ(found.row(), 7);
  EXPECT_EQ(found.column(), 0);
}

TEST_F(ConcatProxyModelTest, RowInsertionSignalsOffsetCorrectly) {
  proxy.setSourceModels({modelA, modelB, modelC}, Orientation::Vertical);

  int intercepted_start = -1;
  int intercepted_end = -1;
  bool signal_fired = false;

  proxy.beginInsertRows.connect([&](const ModelIndex&, int start, int end) {
    intercepted_start = start;
    intercepted_end = end;
    signal_fired = true;
  });

  // Trigger an insert of 2 rows inside Model C (at local row index 1)
  // Upstream offsets: Model A (3 rows) + Model B (2 rows) = 5 rows offset
  // Expected global proxy rows: 1 + 5 = 6, ending at 2 + 5 = 7
  modelC->triggerInsertRows(1, 2);

  EXPECT_TRUE(signal_fired);
  EXPECT_EQ(intercepted_start, 6);
  EXPECT_EQ(intercepted_end, 7);
  EXPECT_EQ(proxy.rowCount(), 11);  // Total rows scale automatically (9 + 2)
}

TEST_F(ConcatProxyModelTest, RowRemovalSignalsOffsetCorrectly) {
  proxy.setSourceModels({modelA, modelB, modelC}, Orientation::Vertical);

  int intercepted_start = -1;
  int intercepted_end = -1;
  bool signal_fired = false;

  proxy.beginRemoveRows.connect([&](const ModelIndex&, int start, int end) {
    intercepted_start = start;
    intercepted_end = end;
    signal_fired = true;
  });

  // Remove 1 row from Model B (at index 0)
  // Upstream offset: Model A (3 rows)
  // Expected proxy coordinate: 0 + 3 = 3
  modelB->triggerRemoveRows(0, 0);

  EXPECT_TRUE(signal_fired);
  EXPECT_EQ(intercepted_start, 3);
  EXPECT_EQ(intercepted_end, 3);
  EXPECT_EQ(proxy.rowCount(), 8);
}

TEST_F(ConcatProxyModelTest, MasterResetForwardingCleansViews) {
  proxy.setSourceModels({modelA, modelB}, Orientation::Vertical);

  bool begin_fired = false;
  bool end_fired = false;

  proxy.beginResetModel.connect([&]() { begin_fired = true; });
  proxy.endResetModel.connect([&]() { end_fired = true; });

  // Trigger reset on a sub-model
  modelA->triggerReset();

  EXPECT_TRUE(begin_fired);
  EXPECT_TRUE(end_fired);
}

TEST_F(ConcatProxyModelTest, IndexBoundaryEdgeCasesReturnInvalid) {
  proxy.setSourceModels({modelA, modelB}, Orientation::Vertical);

  // Negative boundary constraints
  EXPECT_FALSE(proxy.index(-1, 0).isValid());
  EXPECT_FALSE(proxy.index(0, -1).isValid());

  // Columns overflow beyond maximum model width footprint (Max width is 2)
  EXPECT_FALSE(proxy.index(0, 2).isValid());
  EXPECT_FALSE(proxy.index(4, 5).isValid());

  // Total rows overflow constraint (Model A has 3, Model B has 2 -> Total 5)
  EXPECT_FALSE(proxy.index(5, 0).isValid());
  EXPECT_FALSE(proxy.index(10, 1).isValid());
}

TEST_F(ConcatProxyModelTest, ParentIndicesAreUnconditionallyRejected) {
  proxy.setSourceModels({modelA, modelB}, Orientation::Vertical);

  // ConcatProxy presents a completely flat 1D/2D structural matrix layout.
  // Any query passing a valid parent must be rejected with an invalid return
  // handle.
  ModelIndex fake_parent = proxy.index(0, 0);
  EXPECT_TRUE(fake_parent.isValid());

  ModelIndex nested_child = proxy.index(0, 0, fake_parent);
  EXPECT_FALSE(nested_child.isValid());
}

TEST_F(ConcatProxyModelTest, MapFromProxyUnpacksSourcePointersFlawlessly) {
  proxy.setSourceModels({modelA, modelB}, Orientation::Vertical);

  // 1. Map an index pointing into Model A's zone
  ModelIndex proxy_idx_A = proxy.index(2, 1);
  auto [src_idx_A, src_model_A] = proxy.mapFromProxy(proxy_idx_A);

  EXPECT_TRUE(src_idx_A.isValid());
  EXPECT_EQ(src_model_A, modelA.get());
  EXPECT_EQ(src_idx_A.row(), 2);
  EXPECT_EQ(src_idx_A.column(), 1);
  EXPECT_EQ(src_idx_A.internalPointer(), proxy_idx_A.internalPointer());

  // 2. Map an index pointing into Model B's zone (Proxy row 4 -> Model B row 1)
  ModelIndex proxy_idx_B = proxy.index(4, 0);
  auto [src_idx_B, src_model_B] = proxy.mapFromProxy(proxy_idx_B);

  EXPECT_TRUE(src_idx_B.isValid());
  EXPECT_EQ(src_model_B, modelB.get());
  EXPECT_EQ(src_idx_B.row(), 1);
  EXPECT_EQ(src_idx_B.column(), 0);
  EXPECT_EQ(src_idx_B.internalPointer(), proxy_idx_B.internalPointer());
}

TEST_F(ConcatProxyModelTest, MapFromSourceCalculatesReverseOffsetsFlawlessly) {
  proxy.setSourceModels({modelA, modelB, modelC}, Orientation::Vertical);

  // Generate an index originating deep inside Model C (Row 2, Col 1)
  ModelIndex src_idx_C = modelC->index(2, 1);

  // Convert it backward into Proxy visual space
  // Target row calculation: Model A (3 rows) + Model B (2 rows) + Model C (2
  // rows) = Row index 7
  ModelIndex proxy_idx = proxy.mapFromSource(src_idx_C);

  EXPECT_TRUE(proxy_idx.isValid());
  EXPECT_EQ(proxy_idx.model(),
            &proxy);  // The proxy MUST own this index authority
  EXPECT_EQ(proxy_idx.row(), 7);
  EXPECT_EQ(proxy_idx.column(), 1);
  EXPECT_EQ(proxy_idx.internalPointer(), src_idx_C.internalPointer());
}

TEST_F(ConcatProxyModelTest,
       HorizontalMapFromProxyUnpacksSourcePointersFlawlessly) {
  proxy.setSourceModels({modelA, modelB}, Orientation::Horizontal);

  // Proxy index at Row 1, Column 3.
  // Model A has 2 columns (0 and 1). Model B has 2 columns (2 and 3).
  // Column 3 maps to Model B, Column 1 (3 - 2 = 1)
  ModelIndex proxy_idx = proxy.index(1, 3);
  auto [src_idx, src_model] = proxy.mapFromProxy(proxy_idx);

  EXPECT_TRUE(src_idx.isValid());
  EXPECT_EQ(src_model, modelB.get());
  EXPECT_EQ(src_idx.row(), 1);
  EXPECT_EQ(src_idx.column(), 1);
}

TEST_F(ConcatProxyModelTest,
       HorizontalMapFromSourceCalculatesReverseOffsetsFlawlessly) {
  proxy.setSourceModels({modelA, modelB, modelC}, Orientation::Horizontal);

  // Generate index inside Model C (Row 1, Col 0)
  ModelIndex src_idx_C = modelC->index(1, 0);

  // Convert backward into Proxy space
  // Column calculation: Model A (2 cols) + Model B (2 cols) + Model C (0 cols)
  // = Proxy Column 4
  ModelIndex proxy_idx = proxy.mapFromSource(src_idx_C);

  EXPECT_TRUE(proxy_idx.isValid());
  EXPECT_EQ(proxy_idx.row(), 1);
  EXPECT_EQ(proxy_idx.column(), 4);
}

TEST_F(ConcatProxyModelTest, ReciprocalTranslationInvarianceIsGuaranteed) {
  proxy.setSourceModels({modelA, modelB, modelC}, Orientation::Vertical);

  // Loop through every single coordinate cell in the proxy layout
  for (int r = 0; r < proxy.rowCount(); ++r) {
    for (int c = 0; c < proxy.columnCount(); ++c) {
      ModelIndex proxy_idx = proxy.index(r, c);

      // If the underlying child model doesn't have a row here (e.g., asymmetric
      // horizontal shapes), skip
      auto [src_idx, src_model] = proxy.mapFromProxy(proxy_idx);
      if (!src_idx.isValid()) {
        continue;
      }

      // Invariance Law: mapFromSource(mapFromProxy(X)) MUST equal X perfectly
      ModelIndex round_trip_idx = proxy.mapFromSource(src_idx);

      ASSERT_TRUE(round_trip_idx.isValid());
      ASSERT_EQ(round_trip_idx.row(), proxy_idx.row());
      ASSERT_EQ(round_trip_idx.column(), proxy_idx.column());
      ASSERT_EQ(round_trip_idx.internalPointer(), proxy_idx.internalPointer());
      ASSERT_EQ(round_trip_idx.model(), &proxy);
    }
  }
}

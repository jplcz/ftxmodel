#include <gtest/gtest.h>
#include <ftxmodel/abstract_item_model.hpp>
#include <ftxmodel/concat_proxy_model.hpp>
#include <ftxmodel/model_debugging.hpp>
#include <memory>
#include <string>
#include <vector>

using namespace ftxmodel;

// A simple node structure for testing dynamic mutations
struct MockNode {
  std::string name;
  bool is_container = false;
  bool fetched = false;
  std::vector<std::unique_ptr<MockNode>> children;
};

class LazyMockModel : public AbstractItemModel {
 public:
  std::unique_ptr<MockNode> root;

  LazyMockModel() {
    root = std::make_unique<MockNode>("Root", true, true);
    // Add one unloaded placeholder folder
    auto folder = std::make_unique<MockNode>("lazy_folder", true, false);
    root->children.push_back(std::move(folder));
  }

  int rowCount(const ModelIndex& parent = ModelIndex()) const override {
    if (!parent.isValid()) {
      return static_cast<int>(root->children.size());
    }
    auto* node = static_cast<MockNode*>(parent.internalPointer());
    return static_cast<int>(node->children.size());
  }

  int columnCount(const ModelIndex& = ModelIndex()) const override { return 1; }

  ModelIndex index(int row,
                   int column,
                   const ModelIndex& parent = ModelIndex()) const override {
    if (row < 0 || row >= rowCount(parent)) {
      return {};
    }
    if (!parent.isValid()) {
      return createIndex(row, column, root->children[row].get());
    }
    auto* pNode = static_cast<MockNode*>(parent.internalPointer());
    return createIndex(row, column, pNode->children[row].get());
  }

  ModelIndex parent(const ModelIndex&) const override {
    return {}; /* Flat root for mock simplicity */
  }
  std::any data(const ModelIndex& idx, ItemRole) const override {
    if (!idx.isValid()) {
      return {};
    }
    return static_cast<MockNode*>(idx.internalPointer())->name;
  }

  // --- Dynamic Fetching Contract Implementation ---
  bool hasChildren(const ModelIndex& parent) const override {
    if (!parent.isValid()) {
      return true;
    }
    return static_cast<MockNode*>(parent.internalPointer())->is_container;
  }

  bool canFetchMore(const ModelIndex& parent) const override {
    if (!parent.isValid()) {
      return false;
    }
    auto* node = static_cast<MockNode*>(parent.internalPointer());
    return node->is_container && !node->fetched;
  }

  void fetchMore(const ModelIndex& parent) override {
    if (!canFetchMore(parent)) {
      return;
    }
    auto* node = static_cast<MockNode*>(parent.internalPointer());

    // Notify attached views that 2 rows are arriving under this parent
    beginInsertRows(parent, 0, 1);

    node->children.push_back(
        std::make_unique<MockNode>("fetched_child_A", false, true));
    node->children.push_back(
        std::make_unique<MockNode>("fetched_child_B", false, true));
    node->fetched = true;

    endInsertRows();
  }
};
class FetchableTableMock : public AbstractItemModel {
 public:
  std::vector<std::string> m_rows;
  int m_max_available_database_rows = 5;

  FetchableTableMock() {
    // Start with 2 initial data rows cached in memory
    m_rows = {"row_0", "row_1"};
  }

  int rowCount(const ModelIndex& parent = ModelIndex()) const override {
    // Table constraint: parent must be invalid for a flat layout matrix
    if (parent.isValid()) {
      return 0;
    }
    return static_cast<int>(m_rows.size());
  }

  int columnCount(const ModelIndex& = ModelIndex()) const override { return 1; }

  ModelIndex index(int row,
                   int column,
                   const ModelIndex& parent = ModelIndex()) const override {
    if (parent.isValid() || row < 0 || row >= rowCount()) {
      return {};
    }
    // Use row index integer directly as our dummy internal data tag identifier
    return createIndex(row, column,
                       reinterpret_cast<void*>(static_cast<uintptr_t>(row)));
  }

  ModelIndex parent(const ModelIndex&) const override { return {}; }

  std::any data(const ModelIndex& idx, ItemRole) const override {
    if (!idx.isValid()) {
      return {};
    }
    return m_rows[static_cast<size_t>(idx.row())];
  }

  // --- Flat Table Dynamic Fetching Implementations ---

  bool canFetchMore(const ModelIndex& parent) const override {
    if (parent.isValid()) {
      return false;  // Roots only
    }
    return m_rows.size() < static_cast<size_t>(m_max_available_database_rows);
  }

  void fetchMore(const ModelIndex& parent) override {
    if (!canFetchMore(parent)) {
      return;
    }

    int currentCount = static_cast<int>(m_rows.size());

    // Signal view layer that 2 new sequential items are arriving at the tail
    // edge
    beginInsertRows(ModelIndex(), currentCount, currentCount + 1);

    m_rows.push_back("row_" + std::to_string(currentCount));
    m_rows.push_back("row_" + std::to_string(currentCount + 1));

    endInsertRows();
  }
};

class DynamicFetchingTest : public ::testing::Test {
 protected:
  std::shared_ptr<LazyMockModel> model;
  bool aboutToInsertFired = false;
  bool insertedFired = false;
  int itemsInsertedCount = 0;

  void SetUp() override {
    model = std::make_shared<LazyMockModel>();
    aboutToInsertFired = false;
    insertedFired = false;
    itemsInsertedCount = 0;

    // Bind lambda spies to inspect signaling metrics
    model->beginInsertRows.connect(
        [this](const ModelIndex&, int first, int last) {
          aboutToInsertFired = true;
          itemsInsertedCount = (last - first) + 1;
        });
    model->endInsertRows.connect([this]() { insertedFired = true; });
  }
};

TEST_F(DynamicFetchingTest, ReportsHasChildrenButRowCountIsZeroBeforeFetch) {
  ModelIndex folderIdx = model->index(0, 0);
  ASSERT_TRUE(folderIdx.isValid());

  // It is a directory, so it tells the UI it has branches...
  EXPECT_TRUE(model->hasChildren(folderIdx));
  EXPECT_TRUE(model->canFetchMore(folderIdx));

  // ...but the memory cache vector size is still completely empty!
  EXPECT_EQ(model->rowCount(folderIdx), 0);
}

TEST_F(DynamicFetchingTest, ExecutingFetchFiresSignalsAndMutatesDimensions) {
  ModelIndex folderIdx = model->index(0, 0);

  // Run the extraction routine
  model->fetchMore(folderIdx);

  // Verify the architectural event notifications executed in order
  EXPECT_TRUE(aboutToInsertFired);
  EXPECT_TRUE(insertedFired);
  EXPECT_EQ(itemsInsertedCount, 2);

  // Check that memory matrices have structural children now
  EXPECT_EQ(model->rowCount(folderIdx), 2);
  EXPECT_FALSE(model->canFetchMore(folderIdx));  // Flag toggles off

  // Verify index addresses map to the new values
  ModelIndex childA = model->index(0, 0, folderIdx);
  EXPECT_EQ(
      std::any_cast<std::string>(model->data(childA, ItemRole::DisplayRole)),
      "fetched_child_A");
}

class FlatTableFetchingTest : public ::testing::Test {
 protected:
  std::shared_ptr<FetchableTableMock> tableModel;
  bool signalAboutToInsertFired = false;
  bool signalInsertedFired = false;
  int firstInsertedRowIdx = -1;
  int lastInsertedRowIdx = -1;

  void SetUp() override {
    tableModel = std::make_shared<FetchableTableMock>();
    signalAboutToInsertFired = false;
    signalInsertedFired = false;
    firstInsertedRowIdx = -1;
    lastInsertedRowIdx = -1;

    // Spy on structural changes via lambda hooks
    tableModel->beginInsertRows.connect(
        [this](const ModelIndex&, int first, int last) {
          signalAboutToInsertFired = true;
          firstInsertedRowIdx = first;
          lastInsertedRowIdx = last;
        });
    tableModel->endInsertRows.connect([this]() { signalInsertedFired = true; });
  }
};

TEST_F(FlatTableFetchingTest,
       VerifiesInitialStateAndIncrementalLoadingHandshake) {
  // 1. Initial dimensions validation pass
  ASSERT_EQ(tableModel->rowCount(), 2);
  EXPECT_TRUE(tableModel->canFetchMore(ModelIndex()));

  // 2. Fire the database fetch operation execution pass
  tableModel->fetchMore(ModelIndex());

  // 3. Evaluate signal properties
  EXPECT_TRUE(signalAboutToInsertFired);
  EXPECT_TRUE(signalInsertedFired);
  EXPECT_EQ(firstInsertedRowIdx, 2);  // Insertion starts at trailing edge Row 2
  EXPECT_EQ(lastInsertedRowIdx, 3);   // Slices up to Row 3 (2 items total)

  // 4. Evaluate physical state adjustments
  EXPECT_EQ(tableModel->rowCount(), 4);
  EXPECT_EQ(std::any_cast<std::string>(tableModel->data(tableModel->index(2, 0),
                                                        ItemRole::DisplayRole)),
            "row_2");
  EXPECT_EQ(std::any_cast<std::string>(tableModel->data(tableModel->index(3, 0),
                                                        ItemRole::DisplayRole)),
            "row_3");
}

TEST_F(FlatTableFetchingTest, TogglesCanFetchMoreOffOnceMaxLimitsAreReached) {
  // Initial rows = 2. Max = 5.
  tableModel->fetchMore(
      ModelIndex());  // Increments count from 2 to 4. Still can fetch more.
  EXPECT_TRUE(tableModel->canFetchMore(ModelIndex()));

  tableModel->fetchMore(
      ModelIndex());  // Increments count from 4 to 6 (caps at array pushing
                      // logic context thresholds)
  EXPECT_EQ(tableModel->rowCount(), 6);

  // Limits exceeded, flag must accurately assert false
  EXPECT_FALSE(tableModel->canFetchMore(ModelIndex()));
}

// Assuming your original ConcatProxyModel implementation from previous
// conversations
TEST_F(FlatTableFetchingTest,
       ConcatProxyCorrectlyOffsetsFlatTableDynamicFetches) {
  auto proxy = std::make_shared<ConcatProxyModel>();

  // Setup a static upper table model mapping 3 fixed fields
  auto upperStaticModel = std::make_shared<FetchableTableMock>();
  upperStaticModel->m_rows = {"static_A", "static_B",
                              "static_C"};  // Occupies Global Rows 0, 1, 2
  upperStaticModel->m_max_available_database_rows = 3;

  // Stitch the models sequentially into the proxy container grid
  proxy->setSourceModels({upperStaticModel, tableModel},
                         Orientation::Vertical);  // tableModel has 2 rows;
                                                  // occupies Global Rows 3, 4

  ASSERT_EQ(proxy->rowCount(), 5);

  // Bind a tracker to capture translated coordinates emitted from the Proxy
  int proxyInjectedTargetFirstIdx = -1;
  proxy->beginInsertRows.connect([&](const ModelIndex&, const int first, int) {
    proxyInjectedTargetFirstIdx = first;
  });

  // Assert parent model can fetch and command proxy interaction
  ASSERT_TRUE(proxy->canFetchMore(ModelIndex()));
  proxy->fetchMore(ModelIndex());

  // CRITICAL MATRIX OFFSET VERIFICATION:
  // tableModel internally reports an injection mapping index frame starting at
  // Local Row 2. ConcatProxy must calculate the 3 rows from upperStaticModel
  // and report an absolute global insertion offset starting at Global Row 5!
  EXPECT_EQ(proxyInjectedTargetFirstIdx, 5);
  EXPECT_EQ(proxy->rowCount(), 7);  // 3 static + 2 local + 2 newly fetched rows
}

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

#include <ftxmodel/vector_table_model.hpp>

namespace {

struct MockRowItem {
  int id;
  std::string name;
  bool active;
};

}  // namespace

using namespace ftxmodel;

class VectorTableModelTest : public ::testing::Test {
 protected:
  void SetUp() override {
    model = std::make_unique<VectorTableModel<MockRowItem>>();

    // Add standard columns to verify multi-track configurations
    model->addColumn("ID",
                     [](const MockRowItem& item, ItemRole role) -> std::any {
                       if (role == ItemRole::DisplayRole) {
                         return std::to_string(item.id);
                       }
                       if (role == ItemRole::EditRole) {
                         return item.id;
                       }
                       return {};
                     });

    // Add an editable column
    model->addColumn(
        "Name",
        [](const MockRowItem& item, ItemRole role) -> std::any {
          if (role == ItemRole::DisplayRole || role == ItemRole::EditRole) {
            return item.name;
          }
          return {};
        },
        [](MockRowItem& item, const std::any& value, ItemRole role) -> bool {
          if (role == ItemRole::EditRole &&
              value.type() == typeid(std::string)) {
            item.name = std::any_cast<std::string>(value);
            return true;
          }
          return false;
        });

    // Configure a stable key extractor
    model->setKeyExtractor([](const MockRowItem& item) -> UniqueNodeId {
      return static_cast<std::int64_t>(item.id);
    });

    // Mock dataset populate
    sampleData = {
        {10, "Alpha", true}, {20, "Beta", false}, {30, "Gamma", true}};
  }

  std::unique_ptr<VectorTableModel<MockRowItem>> model;
  std::vector<MockRowItem> sampleData;
};

TEST_F(VectorTableModelTest, InitialEmptyStateLayout) {
  auto emptyModel = std::make_unique<VectorTableModel<MockRowItem>>();
  EXPECT_EQ(emptyModel->rowCount(), 0);
  EXPECT_EQ(emptyModel->columnCount(), 0);
  EXPECT_FALSE(emptyModel->index(0, 0).isValid());
}

TEST_F(VectorTableModelTest, MatrixDimensionsMatchDataAndColumns) {
  model->setVectorData(sampleData);

  EXPECT_EQ(model->rowCount(), 3);
  EXPECT_EQ(model->columnCount(), 2);  // ID and Name
  EXPECT_TRUE(model->hasChildren());
}

TEST_F(VectorTableModelTest, IndexBoundsChecking) {
  model->setVectorData(sampleData);

  // Valid indices
  ModelIndex validTopLeft = model->index(0, 0);
  ModelIndex validBottomRight = model->index(2, 1);
  EXPECT_TRUE(validTopLeft.isValid());
  EXPECT_TRUE(validBottomRight.isValid());
  EXPECT_EQ(validTopLeft.row(), 0);
  EXPECT_EQ(validTopLeft.column(), 0);

  // Invalid coordinates out of bounds
  EXPECT_FALSE(model->index(-1, 0).isValid());
  EXPECT_FALSE(model->index(3, 0).isValid());  // rowCount is 3 (0-2)
  EXPECT_FALSE(model->index(0, 2).isValid());  // columnCount is 2 (0-1)

  // Flat structures must return invalid models if a parent context is supplied
  EXPECT_FALSE(model->index(0, 0, validTopLeft).isValid());
}

TEST_F(VectorTableModelTest, RetrieveDisplayAndCustomRoleData) {
  model->setVectorData(sampleData);

  ModelIndex idxId = model->index(0, 0);    // Row 0, Col 0 ("10")
  ModelIndex idxName = model->index(1, 1);  // Row 1, Col 1 ("Beta")

  EXPECT_EQ(model->textData(idxId), "10");
  EXPECT_EQ(model->textData(idxName), "Beta");

  // Test EditRole payload extraction directly via any_cast
  std::any editData = model->data(idxId, ItemRole::EditRole);
  EXPECT_EQ(std::any_cast<int>(editData), 10);
}

TEST_F(VectorTableModelTest, UniqueNodeIdFallbacksAndResolutions) {
  model->setVectorData(sampleData);

  ModelIndex targetIdx = model->index(1, 0);  // Row 1 ("Beta", ID 20)
  UniqueNodeId expectedId = std::int64_t(20);

  EXPECT_EQ(model->uniqueId(targetIdx), expectedId);

  ModelIndex foundIdx = model->findIndexById(expectedId);
  EXPECT_TRUE(foundIdx.isValid());
  EXPECT_EQ(foundIdx.row(), 1);
}

TEST_F(VectorTableModelTest, ReadOnlyVersusEditableItemFlags) {
  model->setVectorData(sampleData);

  ModelIndex readOnlyCell =
      model->index(0, 0);  // ID Column (No mutator provided)
  ModelIndex editableCell = model->index(0, 1);  // Name Column (Has mutator)

  EXPECT_FALSE(model->flags(readOnlyCell) & ItemFlag::ItemIsEditable);
  EXPECT_TRUE(model->flags(editableCell) & ItemFlag::ItemIsEditable);
}

TEST_F(VectorTableModelTest, HeaderMetadataExtraction) {
  EXPECT_EQ(
      std::any_cast<std::string>(model->headerData(0, Orientation::Horizontal)),
      "ID");
  EXPECT_EQ(
      std::any_cast<std::string>(model->headerData(1, Orientation::Horizontal)),
      "Name");

  // Vertical sections fallback safely to stringified index counts
  EXPECT_EQ(
      std::any_cast<std::string>(model->headerData(4, Orientation::Vertical)),
      "4");
}

TEST_F(VectorTableModelTest, DataMutationEmitsSignalsAndAltersState) {
  model->setVectorData(sampleData);
  ModelIndex targetCell = model->index(1, 1);  // Row 1, Col 1 ("Beta")

  bool signalEmitted = false;
  model->dataChanged.connect(
      [&](const ModelIndex& topLeft, const ModelIndex& bottomRight) {
        if (topLeft == targetCell && bottomRight == targetCell) {
          signalEmitted = true;
        }
      });

  // Attempt mutation via valid matching types
  std::string newName = "Beta-Updated";
  bool success = model->setData(targetCell, newName, ItemRole::EditRole);

  EXPECT_TRUE(success);
  EXPECT_TRUE(signalEmitted);
  EXPECT_EQ(model->textData(targetCell), "Beta-Updated");

  // Attempt mutation on a read-only field (Should fail cleanly)
  ModelIndex readOnlyCell = model->index(1, 0);
  EXPECT_FALSE(model->setData(readOnlyCell, 999, ItemRole::EditRole));
}

TEST_F(VectorTableModelTest, InsertRowLifecycleSignals) {
  model->setVectorData(sampleData);  // Starts with 3 items

  bool beginCalled = false;
  bool endCalled = false;
  int targetInsertPos = 1;

  model->beginInsertRows.connect(
      [&](const ModelIndex& parent, int start, int end) {
        EXPECT_FALSE(parent.isValid());
        EXPECT_EQ(start, targetInsertPos);
        EXPECT_EQ(end, targetInsertPos);
        beginCalled = true;
      });

  model->endInsertRows.connect([&]() { endCalled = true; });

  MockRowItem newItem{99, "Omega", true};
  model->insertRowItem(targetInsertPos, newItem);

  EXPECT_TRUE(beginCalled);
  EXPECT_TRUE(endCalled);
  EXPECT_EQ(model->rowCount(), 4);
  EXPECT_EQ(model->textData(model->index(1, 1)),
            "Omega");  // Newly placed index slot
}

TEST_F(VectorTableModelTest, RemoveRowLifecycleSignals) {
  model->setVectorData(sampleData);  // Starts with 3 items

  bool beginCalled = false;
  bool endCalled = false;
  int targetRemovePos = 0;  // Removing "Alpha"

  model->beginRemoveRows.connect(
      [&](const ModelIndex& parent, int start, int end) {
        EXPECT_FALSE(parent.isValid());
        EXPECT_EQ(start, targetRemovePos);
        EXPECT_EQ(end, targetRemovePos);
        beginCalled = true;
      });

  model->endRemoveRows.connect([&]() { endCalled = true; });

  bool success = model->removeRowItem(targetRemovePos);

  EXPECT_TRUE(success);
  EXPECT_TRUE(beginCalled);
  EXPECT_TRUE(endCalled);
  EXPECT_EQ(model->rowCount(), 2);
  EXPECT_EQ(model->textData(model->index(0, 1)),
            "Beta");  // "Beta" rolled up into index 0
}

TEST_F(VectorTableModelTest, ModelResetLifecycleSignals) {
  model->setVectorData(sampleData);

  bool beginResetCalled = false;
  bool endResetCalled = false;

  model->beginResetModel.connect([&]() { beginResetCalled = true; });
  model->endResetModel.connect([&]() { endResetCalled = true; });

  model->clear();

  EXPECT_TRUE(beginResetCalled);
  EXPECT_TRUE(endResetCalled);
  EXPECT_EQ(model->rowCount(), 0);
}

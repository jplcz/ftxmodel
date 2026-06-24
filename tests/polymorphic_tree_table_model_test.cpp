#include <gtest/gtest.h>
#include <any>
#include <memory>
#include <string>

#include "ftxmodel/polymorphic_tree_table_model.hpp"

using namespace ftxmodel;

namespace {
struct CategoryNode {
  std::string title;
};

struct DeviceAsset {
  int id;
  std::string status;
};
}  // namespace

class PolymorphicTreeTableModelTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Instantiate a 2-column polymorphic tree model
    model = std::make_unique<PolymorphicTreeTableModel>(
        std::vector<std::string>{"Item Title", "Technical Value"});

    // Register runtime handler rules for CategoryNode
    model->registerTypeHandler<CategoryNode>(
        [](const std::any& rawAny, int column, ItemRole role) -> std::any {
          if (role != ItemRole::DisplayRole) {
            return {};
          }
          const auto& cat = std::any_cast<const CategoryNode&>(rawAny);
          if (column == 0) {
            return cat.title;
          }
          if (column == 1) {
            return std::string("Structural Branch");
          }
          return {};
        }
        // CategoryNode remains read-only (mutator parameter defaults to
        // nullptr)
    );

    // Register runtime handler rules for DeviceAsset (including a mutator)
    model->registerTypeHandler<DeviceAsset>(
        [](const std::any& rawAny, int column, ItemRole role) -> std::any {
          if (role != ItemRole::DisplayRole && role != ItemRole::EditRole) {
            return {};
          }
          const auto& dev = std::any_cast<const DeviceAsset&>(rawAny);

          if (column == 0) {
            return dev.status;
          }
          if (column == 1) {
            return dev.id;
          }
          return {};
        },
        [](std::any& rawAny, int column, const std::any& incomingVal,
           ItemRole role) -> bool {
          if (role != ItemRole::EditRole) {
            return false;
          }
          auto& dev = std::any_cast<DeviceAsset&>(rawAny);

          if (column == 0 && incomingVal.type() == typeid(std::string)) {
            dev.status = std::any_cast<std::string>(incomingVal);
            return true;
          }
          if (column == 1 && incomingVal.type() == typeid(int)) {
            dev.id = std::any_cast<int>(incomingVal);
            return true;
          }
          return false;
        });

    // Configure a simple universal key extractor for stability
    model->setKeyExtractor(
        [](const TreeTableModel<std::any>::RowData& var) -> UniqueNodeId {
          const std::any& rawAny = std::get<std::any>(var);
          if (rawAny.type() == typeid(CategoryNode)) {
            return std::any_cast<const CategoryNode&>(rawAny).title;
          }
          if (rawAny.type() == typeid(DeviceAsset)) {
            return static_cast<std::int64_t>(
                std::any_cast<const DeviceAsset&>(rawAny).id);
          }
          return {nullptr};
        });
  }

  std::unique_ptr<PolymorphicTreeTableModel> model;
};

TEST_F(PolymorphicTreeTableModelTest, ResolvesDisjointTypesPolymorphically) {
  auto* root = model->rootNode();

  // Append CategoryNode as a parent branch, and DeviceAsset as a child leaf
  // node
  auto* parentNode =
      model->appendChildItem(root, CategoryNode{"Sensors Subsystem"});
  model->appendChildItem(parentNode, DeviceAsset{4001, "Operational"});

  ModelIndex parentIdx0 = model->index(0, 0);
  ModelIndex parentIdx1 = model->index(0, 1);
  ModelIndex childIdx0 = model->index(0, 0, parentIdx0);
  ModelIndex childIdx1 = model->index(0, 1, parentIdx0);

  // Verify parent extraction routes correctly via typeid
  EXPECT_EQ(model->textData(parentIdx0), "Sensors Subsystem");
  EXPECT_EQ(model->textData(parentIdx1), "Structural Branch");

  // Verify child extraction routes correctly via typeid
  EXPECT_EQ(model->textData(childIdx0), "Operational");
  EXPECT_EQ(model->textData(childIdx1), "4001");
}

TEST_F(PolymorphicTreeTableModelTest, UnregisteredTypeGracefulFallback) {
  struct UnregisteredType {
    int value = 42;
  };

  auto* root = model->rootNode();
  model->appendChildItem(root, UnregisteredType{});

  ModelIndex idx = model->index(0, 0);
  EXPECT_TRUE(idx.isValid());

  // Extraction should return empty `std::any` gracefully rather than triggering
  // a bad_any_cast panic
  EXPECT_TRUE(model->textData(idx).empty());
}

TEST_F(PolymorphicTreeTableModelTest, DataMutationModifiesVariantPayload) {
  auto* root = model->rootNode();
  auto* node = model->appendChildItem(root, DeviceAsset{77, "Offline"});
  ModelIndex targetIdx = model->index(node->rowInParent(), 0);

  bool signalEmitted = false;
  model->dataChanged.connect(
      [&](const ModelIndex& topLeft, const ModelIndex& bottomRight) {
        if (topLeft == targetIdx && bottomRight == targetIdx) {
          signalEmitted = true;
        }
      });

  // Mutate the string status field
  bool statusSuccess =
      model->setData(targetIdx, std::string("Online"), ItemRole::EditRole);
  EXPECT_TRUE(statusSuccess);
  EXPECT_TRUE(signalEmitted);
  EXPECT_EQ(model->textData(targetIdx), "Online");

  // Mutate the numerical ID field on column 1
  ModelIndex idIdx = model->index(node->rowInParent(), 1);
  bool idSuccess = model->setData(idIdx, int(88), ItemRole::EditRole);
  EXPECT_TRUE(idSuccess);
  EXPECT_EQ(model->textData(idIdx), "88");
}

TEST_F(PolymorphicTreeTableModelTest, ReadOnlyTypesRejectDataEdits) {
  auto* root = model->rootNode();
  auto* node = model->appendChildItem(root, CategoryNode{"Constant Branch"});
  ModelIndex targetIdx = model->index(node->rowInParent(), 0);

  // CategoryNode does not have a mutator registered, so this must reject edits
  // cleanly
  bool success =
      model->setData(targetIdx, std::string("New Title"), ItemRole::EditRole);
  EXPECT_FALSE(success);
  EXPECT_EQ(model->textData(targetIdx), "Constant Branch");
}

TEST_F(PolymorphicTreeTableModelTest, MutationRejectsMismatchedValueTypes) {
  auto* root = model->rootNode();
  auto* node = model->appendChildItem(root, DeviceAsset{12, "Active"});
  ModelIndex targetIdx =
      model->index(node->rowInParent(), 1);  // Column 1 expects an `int`

  // Attempting to pass a double type down to an int field handler must fail
  // gracefully
  bool success = model->setData(targetIdx, double(34.5), ItemRole::EditRole);
  EXPECT_FALSE(success);
  EXPECT_EQ(model->textData(targetIdx), "12");
}

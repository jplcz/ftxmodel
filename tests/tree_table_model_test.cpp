#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

#include <ftxmodel/tree_table_model.hpp>

namespace {
// Mock structures to act as varied nodes in the multi-type tree pack
struct GroupFolder {
  std::string folderPath;
};

struct FileAsset {
  int systemId;
  std::string fileName;
};

}  // namespace

using namespace ftxmodel;

class TreeTableModelTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Instantiate a 2-column layout tree model targeting our distinct mock
    // types
    model = std::make_unique<TreeTableModel<GroupFolder, FileAsset>>(
        std::vector<std::string>{"Item Title", "Metadata Attribute"});

    // Column 0: Extract visual labels based on the variant type matching rows
    model->setColumnLogic(
        0, [](const auto& row, int, const ItemRole role) -> std::any {
          if (role != ItemRole::DisplayRole) {
            return {};
          }
          return std::visit(
              []<typename T0>(const T0& actual) -> std::any {
                using T = std::decay_t<T0>;
                if constexpr (std::is_same_v<T, GroupFolder>) {
                  return actual.folderPath;
                }
                if constexpr (std::is_same_v<T, FileAsset>) {
                  return actual.fileName;
                }
                return std::string("");
              },
              row);
        });

    // Column 1: Metadata Extraction (Editable for FileAsset types)
    model->setColumnLogic(
        1,
        [](const auto& row, int, ItemRole role) -> std::any {
          if (role != ItemRole::DisplayRole && role != ItemRole::EditRole) {
            return {};
          }
          return std::visit(
              []<typename T0>(const T0& actual) -> std::any {
                using T = std::decay_t<T0>;
                if constexpr (std::is_same_v<T, GroupFolder>) {
                  return std::string("Directory Context");
                }
                if constexpr (std::is_same_v<T, FileAsset>) {
                  return std::to_string(actual.systemId);
                }
                return std::string("");
              },
              row);
        },
        [](auto& row, int, const std::any& val, ItemRole role) -> bool {
          if (role != ItemRole::EditRole || val.type() != typeid(int)) {
            return false;
          }
          return std::visit(
              [&val]<typename T0>(T0& actual) -> bool {
                using T = std::decay_t<T0>;
                if constexpr (std::is_same_v<T, FileAsset>) {
                  actual.systemId = std::any_cast<int>(val);
                  return true;
                }
                return false;  // Group folders are read-only in this config
              },
              row);
        });

    // Apply custom dynamic type unique ID rules
    model->setKeyExtractor([](const auto& row) -> UniqueNodeId {
      return std::visit(
          []<typename T0>(const T0& actual) -> UniqueNodeId {
            using T = std::decay_t<T0>;
            if constexpr (std::is_same_v<T, GroupFolder>) {
              return actual.folderPath;  // string key
            }
            if constexpr (std::is_same_v<T, FileAsset>) {
              return static_cast<std::int64_t>(
                  actual.systemId);  // numerical key
            }
            return {nullptr};
          },
          row);
    });
  }

  std::unique_ptr<TreeTableModel<GroupFolder, FileAsset>> model;
};

TEST_F(TreeTableModelTest, FreshStateIsEmpty) {
  EXPECT_EQ(model->rowCount(), 0);
  EXPECT_EQ(model->columnCount(), 2);
  EXPECT_FALSE(model->index(0, 0).isValid());
}

TEST_F(TreeTableModelTest, BasicParentChildIndexResolution) {
  auto* rootNode = model->rootNode();
  auto* branch = model->appendChildItem(rootNode, GroupFolder{"/var/log"});
  auto* childFile = model->appendChildItem(branch, FileAsset{1043, "syslog"});

  // Row counts under structural vertices
  EXPECT_EQ(model->rowCount(), 1);  // Root has 1 folder child
  ModelIndex branchIdx = model->index(0, 0);
  EXPECT_TRUE(branchIdx.isValid());
  EXPECT_EQ(model->rowCount(branchIdx),
            1);  // Folder branch has 1 nested child file

  // Traversal routes downwards
  ModelIndex fileIdx = model->index(0, 0, branchIdx);
  EXPECT_TRUE(fileIdx.isValid());
  EXPECT_EQ(model->textData(fileIdx), "syslog");

  // Traversal coordinates upwards
  ModelIndex resolvedParentIdx = model->parent(fileIdx);
  EXPECT_TRUE(resolvedParentIdx.isValid());
  EXPECT_EQ(resolvedParentIdx.row(), 0);
  EXPECT_EQ(model->textData(resolvedParentIdx), "/var/log");
}

TEST_F(TreeTableModelTest, InstantCachedReverseLookups) {
  auto* rootNode = model->rootNode();
  model->appendChildItem(rootNode, GroupFolder{"/bin"});
  auto* targetBranch =
      model->appendChildItem(rootNode, GroupFolder{"/usr/local/bin"});
  model->appendChildItem(targetBranch, FileAsset{998, "cmake"});

  // Query constant-time lookup maps matching disjoint types natively
  UniqueNodeId folderKey = std::string("/usr/local/bin");
  UniqueNodeId fileKey = std::int64_t(998);

  ModelIndex foundFolder = model->findIndexById(folderKey);
  ModelIndex foundFile = model->findIndexById(fileKey);

  EXPECT_TRUE(foundFolder.isValid());
  EXPECT_EQ(foundFolder.row(), 1);  // /usr/local/bin is the second row on root

  EXPECT_TRUE(foundFile.isValid());
  EXPECT_EQ(foundFile.row(),
            0);  // cmake is index 0 under its parent branch context
  EXPECT_EQ(model->parent(foundFile),
            foundFolder);  // Verify parent maps accurately
}

TEST_F(TreeTableModelTest, InsertChildAtTargetPositions) {
  auto* root = model->rootNode();
  model->appendChildItem(root, GroupFolder{"Branch-0"});
  model->appendChildItem(root, GroupFolder{"Branch-2"});

  bool beginSignalEmitted = false;
  model->beginInsertRows.connect(
      [&](const ModelIndex& parent, int start, int end) {
        EXPECT_FALSE(parent.isValid());  // Top level target
        EXPECT_EQ(start, 1);
        EXPECT_EQ(end, 1);
        beginSignalEmitted = true;
      });

  // Inject Branch-1 cleanly between 0 and 2 (Index 1)
  model->insertChildItem(root, 1, GroupFolder{"Branch-1"});

  EXPECT_TRUE(beginSignalEmitted);
  EXPECT_EQ(model->rowCount(), 3);
  EXPECT_EQ(model->textData(model->index(1, 0)), "Branch-1");
  EXPECT_EQ(model->textData(model->index(2, 0)), "Branch-2");
}

TEST_F(TreeTableModelTest, RemoveChildCleansCacheSubtrees) {
  auto* root = model->rootNode();
  auto* branch = model->appendChildItem(root, GroupFolder{"/home"});
  model->appendChildItem(branch, FileAsset{55, "config.json"});

  // Verify elements exist in the constant-time cache tracking maps
  EXPECT_TRUE(model->findIndexById(std::string("/home")).isValid());
  EXPECT_TRUE(model->findIndexById(std::int64_t(55)).isValid());

  bool beginRemoveEmitted = false;
  model->beginRemoveRows.connect(
      [&](const ModelIndex& parent, int start, int end) {
        EXPECT_FALSE(parent.isValid());
        EXPECT_EQ(start, 0);
        EXPECT_EQ(end, 0);
        beginRemoveEmitted = true;
      });

  // Extract /home root row (Index 0)
  bool success = model->removeChildItem(root, 0);

  EXPECT_TRUE(success);
  EXPECT_TRUE(beginRemoveEmitted);
  EXPECT_EQ(model->rowCount(), 0);

  // Assert items and their sub-children are pruned from the cache map to defend
  // against leaks
  EXPECT_FALSE(model->findIndexById(std::string("/home")).isValid());
  EXPECT_FALSE(model->findIndexById(std::int64_t(55)).isValid());
}

TEST_F(TreeTableModelTest, MoveChildShiftsVisualSequences) {
  auto* root = model->rootNode();
  model->appendChildItem(root, GroupFolder{"First"});
  model->appendChildItem(root, GroupFolder{"Second"});

  bool dataChangedEmitted = false;
  model->dataChanged.connect(
      [&](const ModelIndex& topLeft, const ModelIndex& bottomRight) {
        EXPECT_EQ(topLeft.row(), 0);
        EXPECT_EQ(bottomRight.row(), 1);
        dataChangedEmitted = true;
      });

  // Swap Row 0 and Row 1 positions
  bool moved = model->moveChildItem(root, 0, 1);

  EXPECT_TRUE(moved);
  EXPECT_TRUE(dataChangedEmitted);
  EXPECT_EQ(model->textData(model->index(0, 0)), "Second");
  EXPECT_EQ(model->textData(model->index(1, 0)), "First");
}

TEST_F(TreeTableModelTest, MutatingIdentityUpdatesCacheKeys) {
  auto* root = model->rootNode();
  auto* branch = model->appendChildItem(root, GroupFolder{"/var"});

  // Directly append the FileAsset child row beneath the /var branch folder
  auto* fileNode = model->appendChildItem(branch, FileAsset{100, "init.sh"});

  // Resolve the ModelIndex coordinate targeting Column 1 (the SystemId
  // attribute field)
  ModelIndex fileMetaIdx = model->index(
      fileNode->rowInParent(), 1, model->findIndexById(std::string("/var")));
  EXPECT_TRUE(fileMetaIdx.isValid());

  // Verify lookups work under the historical initialization key
  EXPECT_TRUE(model->findIndexById(std::int64_t(100)).isValid());

  // Modify the variable property that acts as the item's unique key identity
  bool editSuccess = model->setData(fileMetaIdx, int(500), ItemRole::EditRole);
  EXPECT_TRUE(editSuccess);

  // The historical key must be erased from lookups to prevent leakage
  EXPECT_FALSE(model->findIndexById(std::int64_t(100)).isValid());

  // 4. Ensure the cache map index has smoothly transitioned to the new
  // identifier
  ModelIndex reassignedIdx = model->findIndexById(std::int64_t(500));
  EXPECT_TRUE(reassignedIdx.isValid());
  EXPECT_EQ(model->textData(reassignedIdx, ItemRole::DisplayRole),
            std::string("init.sh"));
}

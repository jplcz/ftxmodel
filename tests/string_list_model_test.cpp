#include <gtest/gtest.h>
#include <ftxmodel/string_list_model.hpp>

using namespace ftxmodel;

class StringListModelTest : public ::testing::Test {
 protected:
  std::unique_ptr<StringListModel> model;

  void SetUp() override {
    std::vector<std::string> initialItems = {"Alpha", "Beta", "Gamma"};
    model = std::make_unique<StringListModel>(initialItems);
  }
};

TEST_F(StringListModelTest, QuantitiesAndDataPayloadsMatchInitialization) {
  EXPECT_EQ(model->rowCount(), 3);

  ModelIndex idx = model->index(1, 0);
  ASSERT_TRUE(idx.isValid());
  EXPECT_EQ(model->textData(idx), "Beta");
}

TEST_F(StringListModelTest, SetsDataCorrectlyAndFiresSignals) {
  ModelIndex idx = model->index(0, 0);
  bool signalFired = false;

  auto connection = model->dataChanged.connect(
      [&](const ModelIndex& top, const ModelIndex& bottom) {
        signalFired = true;
        EXPECT_EQ(top.row(), 0);
        EXPECT_EQ(bottom.row(), 0);
      });

  bool success =
      model->setData(idx, std::string("Updated_Alpha"), ItemRole::EditRole);
  EXPECT_TRUE(success);
  EXPECT_EQ(model->textData(idx), "Updated_Alpha");
  EXPECT_TRUE(signalFired);
}

TEST_F(StringListModelTest, UniqueIdsAreValidAndDistinct) {
  ModelIndex idxAlpha = model->index(0, 0);
  ModelIndex idxBeta = model->index(1, 0);

  UniqueNodeId idAlpha = model->uniqueId(idxAlpha);
  UniqueNodeId idBeta = model->uniqueId(idxBeta);

  // OPAQUE RULE: We do not check holds_alternative<const void*>().
  // We strictly check the operational contract rules.
  EXPECT_NE(idAlpha, UniqueNodeId{nullptr});
  EXPECT_NE(idBeta, UniqueNodeId{nullptr});

  // Two different items MUST yield different opaque identity tokens
  EXPECT_NE(idAlpha, idBeta);
}

TEST_F(StringListModelTest, IdentityRemainsStableAcrossLayoutMutations) {
  ModelIndex idxGammaBefore = model->index(2, 0);  // "Gamma" at Row 2
  UniqueNodeId idGammaBefore = model->uniqueId(idxGammaBefore);

  // Mutate the list layout by deleting an element ABOVE Gamma
  model->removeAt(0);  // Strips "Alpha", "Gamma" shifts up to Row 1

  ModelIndex idxGammaAfter = model->index(1, 0);  // "Gamma" is now at Row 1
  UniqueNodeId idGammaAfter = model->uniqueId(idxGammaAfter);

  // OPAQUE RULE: Regardless of internal pointer addresses or index shifts,
  // the framework contract guarantees the identity remains identical!
  EXPECT_EQ(idGammaBefore, idGammaAfter);
  EXPECT_EQ(model->textData(idxGammaAfter), "Gamma");
}

TEST_F(StringListModelTest, ReverseLookupSuccessfullyResolvesOpaqueIdentity) {
  ModelIndex originalIdx = model->index(2, 0);  // "Gamma"
  UniqueNodeId opaqueId = model->uniqueId(originalIdx);

  // Query the data-first engine using nothing but the token
  ModelIndex discoveredIdx = model->findIndexById(opaqueId);

  ASSERT_TRUE(discoveredIdx.isValid());
  EXPECT_EQ(discoveredIdx.row(), 2);
  EXPECT_EQ(model->textData(discoveredIdx), "Gamma");
}

TEST_F(StringListModelTest, AppendTriggersRowInsertionSignalChain) {
  bool beginFired = false;
  bool endFired = false;

  auto conn1 = model->beginInsertRows.connect(
      [&](const ModelIndex& parent, int start, int end) {
        beginFired = true;
        EXPECT_FALSE(parent.isValid());
        EXPECT_EQ(start, 3);
        EXPECT_EQ(end, 3);
      });

  auto conn2 = model->endInsertRows.connect([&]() { endFired = true; });

  model->append("Delta");

  EXPECT_EQ(model->rowCount(), 4);
  EXPECT_EQ(model->textData(model->index(3, 0)), "Delta");
  EXPECT_TRUE(beginFired);
  EXPECT_TRUE(endFired);
}

TEST_F(StringListModelTest, InsertAtArbitraryPositionCorrectlyShiftsElements) {
  bool beginFired = false;
  bool endFired = false;

  auto conn1 = model->beginInsertRows.connect(
      [&](const ModelIndex& parent, int start, int end) {
        beginFired = true;
        EXPECT_FALSE(parent.isValid());
        EXPECT_EQ(start, 1);
        EXPECT_EQ(end, 1);
      });
  auto conn2 = model->endInsertRows.connect([&]() { endFired = true; });

  // Insert "Inserted" right between "Alpha" and "Beta"
  bool success = model->insertAt(1, "Inserted");

  EXPECT_TRUE(success);
  EXPECT_EQ(model->rowCount(), 4);
  EXPECT_EQ(model->textData(model->index(0, 0)), "Alpha");
  EXPECT_EQ(model->textData(model->index(1, 0)), "Inserted");
  EXPECT_EQ(model->textData(model->index(2, 0)), "Beta");
  EXPECT_TRUE(beginFired);
  EXPECT_TRUE(endFired);
}

TEST_F(StringListModelTest, InsertAtOutOfBoundsRowReturnsFalse) {
  bool successNegative = model->insertAt(-1, "Invalid");
  bool successOverflow =
      model->insertAt(4, "Invalid");  // Max valid is rowCount() (3)

  EXPECT_FALSE(successNegative);
  EXPECT_FALSE(successOverflow);
  EXPECT_EQ(model->rowCount(), 3);
}

TEST_F(StringListModelTest, AppendBatchFiresContinuousRangeSignals) {
  bool beginFired = false;
  bool endFired = false;

  auto conn1 = model->beginInsertRows.connect(
      [&](const ModelIndex& parent, int start, int end) {
        beginFired = true;
        EXPECT_FALSE(parent.isValid());
        EXPECT_EQ(start, 3);  // Spans from index 3...
        EXPECT_EQ(end, 4);    // ...to index 4 inclusively
      });
  auto conn2 = model->endInsertRows.connect([&]() { endFired = true; });

  std::vector<std::string> batch = {"Delta", "Epsilon"};
  model->appendBatch(batch);

  EXPECT_EQ(model->rowCount(), 5);
  EXPECT_EQ(model->textData(model->index(3, 0)), "Delta");
  EXPECT_EQ(model->textData(model->index(4, 0)), "Epsilon");
  EXPECT_TRUE(beginFired);
  EXPECT_TRUE(endFired);
}

TEST_F(StringListModelTest, InsertBatchAtSplicesDataAndDispatchesSignals) {
  bool beginFired = false;
  bool endFired = false;

  auto conn1 = model->beginInsertRows.connect(
      [&](const ModelIndex& parent, int start, int end) {
        beginFired = true;
        EXPECT_EQ(start, 1);
        EXPECT_EQ(end, 2);  // 2 elements starting at 1 means rows 1 and 2
      });
  auto conn2 = model->endInsertRows.connect([&]() { endFired = true; });

  std::vector<std::string> batch = {"Batch1", "Batch2"};
  bool success = model->insertBatchAt(1, batch);

  EXPECT_TRUE(success);
  EXPECT_EQ(model->rowCount(), 5);
  EXPECT_EQ(model->textData(model->index(0, 0)), "Alpha");
  EXPECT_EQ(model->textData(model->index(1, 0)), "Batch1");
  EXPECT_EQ(model->textData(model->index(2, 0)), "Batch2");
  EXPECT_EQ(model->textData(model->index(3, 0)), "Beta");
  EXPECT_TRUE(beginFired);
  EXPECT_TRUE(endFired);
}

TEST_F(StringListModelTest, RemoveBatchAtDropsContiguousSlices) {
  // Let's add elements first to make a wider target layout
  model->appendBatch(
      {"Delta", "Epsilon"});  // {"Alpha", "Beta", "Gamma", "Delta", "Epsilon"}

  bool beginFired = false;
  bool endFired = false;

  auto conn1 = model->beginRemoveRows.connect(
      [&](const ModelIndex& parent, int start, int end) {
        beginFired = true;
        EXPECT_FALSE(parent.isValid());
        EXPECT_EQ(start, 1);  // Removing "Beta" (1)...
        EXPECT_EQ(end, 3);    // ...through "Delta" (3) inclusively
      });
  auto conn2 = model->endRemoveRows.connect([&]() { endFired = true; });

  // Remove 3 elements starting at index 1 ("Beta", "Gamma", "Delta")
  bool success = model->removeBatchAt(1, 3);

  EXPECT_TRUE(success);
  EXPECT_EQ(model->rowCount(), 2);
  EXPECT_EQ(model->textData(model->index(0, 0)), "Alpha");
  EXPECT_EQ(model->textData(model->index(1, 0)), "Epsilon");
  EXPECT_TRUE(beginFired);
  EXPECT_TRUE(endFired);
}

TEST_F(StringListModelTest, RemoveBatchAtWithOutOfBoundsInputsReturnsFalse) {
  bool successOverflow = model->removeBatchAt(1, 5);  // Exceeds array limits
  bool successNegative = model->removeBatchAt(-1, 2);

  EXPECT_FALSE(successOverflow);
  EXPECT_FALSE(successNegative);
  EXPECT_EQ(model->rowCount(), 3);  // Unchanged
}

TEST_F(StringListModelTest, ClearTriggersModelResetLifecycle) {
  bool resetBegun = false;
  bool resetEnded = false;

  auto conn1 = model->beginResetModel.connect([&]() { resetBegun = true; });
  auto conn2 = model->endResetModel.connect([&]() { resetEnded = true; });

  model->clear();

  EXPECT_EQ(model->rowCount(), 0);
  EXPECT_TRUE(resetBegun);
  EXPECT_TRUE(resetEnded);
}

TEST_F(StringListModelTest, DefaultSortArrangesDataAlphabetically) {
  // Setup unsorted state
  model->replaceAll(std::vector<std::string>{"Gamma", "Alpha", "Beta"});

  bool resetBegun = false;
  bool resetEnded = false;
  model->beginResetModel.connect([&]() { resetBegun = true; });
  model->endResetModel.connect([&]() { resetEnded = true; });

  // Default execution uses std::less<> natively
  model->sort();

  EXPECT_EQ(model->textData(model->index(0, 0)), "Alpha");
  EXPECT_EQ(model->textData(model->index(1, 0)), "Beta");
  EXPECT_EQ(model->textData(model->index(2, 0)), "Gamma");
  EXPECT_TRUE(resetBegun);
  EXPECT_TRUE(resetEnded);
}

TEST_F(StringListModelTest, SortAcceptsCustomFunctorPredicates) {
  model->replaceAll(std::vector<std::string>{"A", "LongestString", "Medium"});

  // Sort by text length using custom lambda
  model->sort([](const std::string& a, const std::string& b) {
    return a.length() < b.length();
  });

  EXPECT_EQ(model->textData(model->index(0, 0)), "A");
  EXPECT_EQ(model->textData(model->index(1, 0)), "Medium");
  EXPECT_EQ(model->textData(model->index(2, 0)), "LongestString");
}

TEST_F(StringListModelTest, ToVectorExtractsValuesAccurately) {
  std::vector<std::string> extracted = model->toVector();

  ASSERT_EQ(extracted.size(), 3);
  EXPECT_EQ(extracted[0], "Alpha");
  EXPECT_EQ(extracted[1], "Beta");
  EXPECT_EQ(extracted[2], "Gamma");
}

TEST_F(StringListModelTest, ReplaceAllWithRawVectorsRewritesModelAtomically) {
  bool resetBegun = false;
  bool resetEnded = false;
  model->beginResetModel.connect([&]() { resetBegun = true; });
  model->endResetModel.connect([&]() { resetEnded = true; });

  std::vector<std::string> newDataset = {"One", "Two"};
  model->replaceAll(newDataset);

  EXPECT_EQ(model->rowCount(), 2);
  EXPECT_EQ(model->textData(model->index(0, 0)), "One");
  EXPECT_EQ(model->textData(model->index(1, 0)), "Two");
  EXPECT_TRUE(resetBegun);
  EXPECT_TRUE(resetEnded);
}

TEST_F(StringListModelTest, ReplaceAllWithSharedPtrVectorPreservesUniqueIds) {
  // Extract tracking reference layout snapshot
  auto sharedVectors = model->toSharedPtrVector();
  ASSERT_EQ(sharedVectors.size(), 3);

  UniqueNodeId originalIdBeta = model->uniqueId(model->index(1, 0));  // "Beta"

  // Create an alternate reordered sequence of those exact shared pointers
  // manually
  std::vector<std::shared_ptr<std::string>> reordered = {
      sharedVectors[1],  // "Beta" moves to index 0
      sharedVectors[0],  // "Alpha" moves to index 1
      sharedVectors[2]   // "Gamma" stays at index 2
  };

  model->replaceAll(reordered);

  EXPECT_EQ(model->rowCount(), 3);
  EXPECT_EQ(model->textData(model->index(0, 0)), "Beta");

  // Verify that unique selection keys remain completely valid and trackable
  UniqueNodeId newIdBeta = model->uniqueId(model->index(0, 0));
  EXPECT_EQ(originalIdBeta, newIdBeta);
}

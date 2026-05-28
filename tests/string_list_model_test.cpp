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

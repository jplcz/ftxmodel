#include <gtest/gtest.h>
#include <ftxmodel/standard_item_model.hpp>
#include <memory>
#include <string>

using namespace ftxmodel;

class StandardItemModelTest : public ::testing::Test {
 protected:
  std::unique_ptr<StandardItemModel> model;

  void SetUp() override {
    // Initialize a baseline 3x3 table grid matrix layout
    model = std::make_unique<StandardItemModel>(3, 3);

    // Seed initial display text values
    for (int r = 0; r < 3; ++r) {
      for (int c = 0; c < 3; ++c) {
        std::string value =
            "Cell_" + std::to_string(r) + "_" + std::to_string(c);
        model->setData(model->index(r, c), value, ItemRole::EditRole);
      }
    }
  }
};

// ========================================================================
// 1. DIMENSION & BOUNDARY CORRECTNESS
// ========================================================================

TEST_F(StandardItemModelTest, InitialDimensionsMatchSetupParameters) {
  EXPECT_EQ(model->rowCount(ModelIndex()), 3);
  EXPECT_EQ(model->columnCount(ModelIndex()), 3);

  // Flat grids must always report 0 rows/cols if queried with a valid parent
  ModelIndex valid_cell = model->index(0, 0);
  EXPECT_EQ(model->rowCount(valid_cell), 0);
  EXPECT_EQ(model->columnCount(valid_cell), 0);
}

TEST_F(StandardItemModelTest, IndexGenerationReturnsInvalidOnOutofBounds) {
  // Negative bounds check
  EXPECT_FALSE(model->index(-1, 0).isValid());
  EXPECT_FALSE(model->index(0, -1).isValid());

  // Upper limits bounds check
  EXPECT_FALSE(model->index(3, 0).isValid());
  EXPECT_FALSE(model->index(0, 3).isValid());
}

// ========================================================================
// 2. DATA READ/WRITE OPERATIONS
// ========================================================================

TEST_F(StandardItemModelTest, DataExtractionRetrievesCorrectPayloadStrings) {
  ModelIndex idx = model->index(1, 2);
  std::any data_val = model->data(idx, ItemRole::DisplayRole);

  ASSERT_TRUE(data_val.type() == typeid(std::string));
  EXPECT_EQ(std::any_cast<std::string>(data_val), "Cell_1_2");
}

TEST_F(StandardItemModelTest,
       SetDataMutatesCellStateAndFiresDataChangedSignal) {
  ModelIndex target_idx = model->index(1, 1);
  bool signal_fired = false;

  // Connect listener handle to verify frame signaling integrity
  model->dataChanged.connect([&](const ModelIndex& tl, const ModelIndex& br) {
    if (tl == target_idx && br == target_idx) {
      signal_fired = true;
    }
  });

  std::string new_value = "Mutated_Value";
  bool success = model->setData(target_idx, new_value, ItemRole::EditRole);

  EXPECT_TRUE(success);
  EXPECT_TRUE(signal_fired);
  EXPECT_EQ(std::any_cast<std::string>(
                model->data(target_idx, ItemRole::DisplayRole)),
            "Mutated_Value");
}

// ========================================================================
// 3. MATRIX MUTATIONAL LAYOUT LIFECYCLES
// ========================================================================

TEST_F(StandardItemModelTest, AppendRowGrowsVerticalGridBoundaries) {
  std::vector<StandardItem> new_row_data(3);
  new_row_data[0].display_text = "New_0";
  new_row_data[1].display_text = "New_1";
  new_row_data[2].display_text = "New_2";

  model->appendRow(new_row_data);

  ASSERT_EQ(model->rowCount(), 4);
  EXPECT_EQ(std::any_cast<std::string>(model->data(model->index(3, 0))),
            "New_0");
  EXPECT_EQ(std::any_cast<std::string>(model->data(model->index(3, 2))),
            "New_2");
}

TEST_F(StandardItemModelTest, RemoveRowsShrinksGridAndShiftsRemainingLayout) {
  // Remove row index 1 ("Cell_1_X")
  model->removeRows(1, 1);

  ASSERT_EQ(model->rowCount(), 2);

  // The original row 2 ("Cell_2_X") must now shift up to occupy row index 1
  ModelIndex shifted_idx = model->index(1, 0);
  EXPECT_EQ(std::any_cast<std::string>(model->data(shifted_idx)), "Cell_2_0");
}

// ========================================================================
// 4. THE UNIQUE POINTER STABILITY CONTRACT (CRITICAL MATRIX PROPERTY)
// ========================================================================

TEST_F(StandardItemModelTest, UniqueNodeIdRemainsIdenticalAcrossRowDeletions) {
  // Capture initial tracker identities for items inside the last row
  ModelIndex original_row2_col0 = model->index(2, 0);
  UniqueNodeId initial_id = model->uniqueId(original_row2_col0);

  // Verify lookup capability before layout alterations
  ModelIndex lookup_before = model->findIndexById(initial_id);
  ASSERT_EQ(lookup_before.row(), 2);

  // Trigger a deletion that physically shifts Row 2 up into Row 1's position
  model->removeRows(1, 1);

  // The item is now visually located at Row 1
  ModelIndex current_visual_idx = model->index(1, 0);
  EXPECT_EQ(std::any_cast<std::string>(model->data(current_visual_idx)),
            "Cell_2_0");

  // CRITICAL INVARIANT ASSERTION:
  // The uniqueId of the physical data MUST remain strictly identical to its
  // initial identity token, despite changing spatial grid addresses.
  UniqueNodeId post_mutation_id = model->uniqueId(current_visual_idx);
  EXPECT_EQ(initial_id, post_mutation_id);

  // Reverse lookup map translation test:
  // findIndexById must now successfully track the cell to its updated position
  // (Row 1)
  ModelIndex lookup_after = model->findIndexById(initial_id);
  EXPECT_EQ(lookup_after.row(), 1);
  EXPECT_EQ(lookup_after.column(), 0);
}

TEST_F(StandardItemModelTest, FindIndexByIdReturnsInvalidTokenForDeletedCells) {
  ModelIndex target_cell = model->index(1, 1);
  UniqueNodeId cell_id = model->uniqueId(target_cell);

  // Blow away the entire row container containing this cell
  model->removeRows(1, 1);

  // The identity mapping lookup cache engine must gracefully report failure
  ModelIndex dead_lookup = model->findIndexById(cell_id);
  EXPECT_FALSE(dead_lookup.isValid());
}

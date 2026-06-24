#include <gtest/gtest.h>
#include <ftxmodel/abstract_item_model.hpp>
#include <ftxmodel/view_coordinate_mapper.hpp>

using namespace ftxmodel;

class MockTestModel final : public AbstractItemModel {
 public:
  ModelIndex index(int r,
                   int c,
                   const ModelIndex& = ModelIndex()) const override {
    return createIndex(
        r, c,
        reinterpret_cast<void*>(static_cast<uintptr_t>((r * 100) + c + 1)));
  }

  ModelIndex parent(const ModelIndex&) const override { return {}; }
  int rowCount(const ModelIndex&) const override { return 10; }
  int columnCount(const ModelIndex&) const override { return 10; }
  std::any data(const ModelIndex&, ItemRole) const override { return {}; }
};

class ViewCoordinateMapperTest : public ::testing::Test {
 protected:
  MockTestModel model;
};

// --- Test Case 1: Verification of Basic Lifecycle Reset Anchors ---
TEST_F(ViewCoordinateMapperTest,
       ResetClearsActiveCountersWithoutDroppingCapacity) {
  ViewCoordinateMapper mapper;
  ModelIndex idx1 = model.index(0, 0);
  [[maybe_unused]] ModelIndex idx2 = model.index(0, 1);

  // Register spatial bounding regions
  ftxui::Box& box1 = mapper.registerCell(idx1);
  box1.x_min = 10;
  box1.x_max = 20;
  box1.y_min = 5;
  box1.y_max = 6;

  // Confirm standard retrieval loop pass matches cleanly
  auto match_before = mapper.findIndexAt(15, 5);
  ASSERT_TRUE(match_before.has_value());
  EXPECT_EQ(match_before->row(), 0);

  // Fire reset loop trigger
  mapper.reset();

  // The counter is zeroed, so looking up the exact same coordinate must now
  // fail
  auto match_after = mapper.findIndexAt(15, 5);
  EXPECT_FALSE(match_after.has_value());
}

// --- Test Case 2: Verification of Precise Spatial Intersection Lookups ---
TEST_F(ViewCoordinateMapperTest,
       FindIndexAtCorrectlyIdentifiesContainingBoxes) {
  ViewCoordinateMapper mapper;

  ModelIndex topLeftIdx = model.index(2, 3);
  ModelIndex bottomRightIdx = model.index(8, 9);

  ftxui::Box& b1 = mapper.registerCell(topLeftIdx);
  b1.x_min = 0;
  b1.x_max = 10;
  b1.y_min = 0;
  b1.y_max = 2;

  ftxui::Box& b2 = mapper.registerCell(bottomRightIdx);
  b2.x_min = 11;
  b2.x_max = 25;
  b2.y_min = 0;
  b2.y_max = 2;

  // Test precise hitting inside Box 1 boundaries
  auto lookup1 = mapper.findIndexAt(5, 1);
  ASSERT_TRUE(lookup1.has_value());
  EXPECT_EQ(lookup1->row(), 2);
  EXPECT_EQ(lookup1->column(), 3);

  // Test precise hitting inside Box 2 boundaries
  auto lookup2 = mapper.findIndexAt(15, 1);
  ASSERT_TRUE(lookup2.has_value());
  EXPECT_EQ(lookup2->row(), 8);
  EXPECT_EQ(lookup2->column(), 9);

  // Test edge failure checking parameters (Outside target blocks)
  auto lookup_miss = mapper.findIndexAt(50, 50);
  EXPECT_FALSE(lookup_miss.has_value());
}

// --- Test Case 3: CRITICAL INVARIANT - Pointer Stability Validation ---
TEST_F(ViewCoordinateMapperTest,
       AddressesRemainCompletelyStableAcrossChunkSpillovers) {
  ViewCoordinateMapper mapper;

  // 1. Grab reference address to the very first registered element in Chunk 0
  ModelIndex initial_idx = model.index(0, 0);
  ftxui::Box& primary_box_ref = mapper.registerCell(initial_idx);

  primary_box_ref.x_min = 100;
  primary_box_ref.y_min = 200;
  primary_box_ref.x_max = 110;
  primary_box_ref.y_max = 210;

  const ftxui::Box* primary_address_before_spill = &primary_box_ref;

  // 2. Force memory allocation spillover by registering more items than
  // CHUNK_SIZE (64) This pushes registration through Chunk 1, Chunk 2, and into
  // Chunk 3.
  for (int i = 0; i < 200; ++i) {
    ModelIndex filler_idx = model.index(i % 10, 0);
    mapper.registerCell(filler_idx);
  }

  // 3. Extract the address of our primary box again after massive allocations
  // wrapped up
  auto confirmed_lookup = mapper.findIndexAt(100, 200);
  ASSERT_TRUE(confirmed_lookup.has_value());

  // Re-fetch reference bounds
  mapper.reset();  // Stop counter lookup tracking pass
  ftxui::Box& primary_box_ref_post_spill = mapper.registerCell(initial_idx);
  const ftxui::Box* primary_address_after_spill = &primary_box_ref_post_spill;

  // CRITICAL ASSERTION: Memory addresses MUST remain absolutely frozen.
  // Any shift indicates vector reallocations occurred, which breaks
  // ftxui::reflect contracts!
  EXPECT_EQ(primary_address_before_spill, primary_address_after_spill);
}

// --- Test Case 4: Overlapping Box Layer Priority Checking ---
TEST_F(ViewCoordinateMapperTest, FirstRegisteredBoxWinsInOverlappingScenarios) {
  ViewCoordinateMapper mapper;

  ModelIndex first_idx = model.index(1, 1);
  ModelIndex second_idx = model.index(2, 2);

  // Register two completely overlapping bounding elements sequentially
  ftxui::Box& b1 = mapper.registerCell(first_idx);
  b1.x_min = 5;
  b1.x_max = 15;
  b1.y_min = 5;
  b1.y_max = 15;

  ftxui::Box& b2 = mapper.registerCell(second_idx);
  b2.x_min = 5;
  b2.x_max = 15;
  b2.y_min = 5;
  b2.y_max = 15;

  // A coordinate click hitting both blocks should prioritize the earliest
  // registration entry
  auto result = mapper.findIndexAt(10, 10);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->row(), 1);  // Row 1 (first_idx) wins over Row 2
}

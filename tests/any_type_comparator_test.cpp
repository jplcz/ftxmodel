#include <gtest/gtest.h>
#include <any>
#include <string>
#include <string_view>
#include <vector>

#include <ftxmodel/any_type_comparator.hpp>
#include "ftxmodel/abstract_item_model.hpp"

#include "string_matrix_model.hpp"

using namespace ftxmodel;

// Custom User Type for registration tests
struct CustomPrice {
  double value;
};

// ============================================================================
// TEST FIXTURE
// ============================================================================
class AnyTypeComparatorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    AnyTypeComparator::Unregister<CustomPrice>();
    AnyToStringTranslator::Unregister<CustomPrice>();
  }

  void TearDown() override {
    AnyTypeComparator::Unregister<CustomPrice>();
    AnyToStringTranslator::Unregister<CustomPrice>();
  }
};

// ============================================================================
// CORE COMPARISON ENGINE TESTS
// ============================================================================

TEST_F(AnyTypeComparatorTest, PrimaryPrimitiveTypesLessThanEvaluation) {
  EXPECT_TRUE(AnyTypeComparator::Compare(10, 20));
  EXPECT_FALSE(AnyTypeComparator::Compare(20, 10));
  EXPECT_FALSE(AnyTypeComparator::Compare(15, 15));

  EXPECT_TRUE(AnyTypeComparator::Compare(5.5, 10.5));
  EXPECT_TRUE(AnyTypeComparator::Compare(false, true));
  EXPECT_TRUE(AnyTypeComparator::Compare('A', 'B'));
}

TEST_F(AnyTypeComparatorTest, NativeStringTypesLessThanEvaluation) {
  EXPECT_TRUE(
      AnyTypeComparator::Compare(std::string("alpha"), std::string("omega")));
  EXPECT_TRUE(AnyTypeComparator::Compare(std::string_view("abc"),
                                         std::string_view("xyz")));
  EXPECT_TRUE(AnyTypeComparator::Compare("first", "second"));
}

TEST_F(AnyTypeComparatorTest, HandlesEmptyContainersSafely) {
  std::any empty;
  std::any activeInt = 100;

  // Empty variants always bubble up to the top (less than valid data states)
  EXPECT_TRUE(AnyTypeComparator::Compare(empty, activeInt));
  EXPECT_FALSE(AnyTypeComparator::Compare(activeInt, empty));
  EXPECT_FALSE(AnyTypeComparator::Compare(empty, empty));
}

// ============================================================================
// MISMATCH FALLBACK ROUTINES
// ============================================================================

TEST_F(AnyTypeComparatorTest, TypesMismatchTriggersStringTranslatorFallback) {
  // Comparing an integer 100 and a string "50"
  // Lexicographically, stringified "100" comes BEFORE "50"
  std::any intVal = 100;
  std::any stringVal = std::string("50");

  EXPECT_TRUE(AnyTypeComparator::Compare(intVal, stringVal));
  EXPECT_FALSE(AnyTypeComparator::Compare(stringVal, intVal));
}

// ============================================================================
// CUSTOM TYPE REGSITRY OPERATIONS
// ============================================================================

TEST_F(AnyTypeComparatorTest, UsesRegisteredCustomTypeComparators) {
  std::any p1 = CustomPrice{19.99};
  std::any p2 = CustomPrice{49.99};

  AnyTypeComparator::Register<CustomPrice>(
      [](const CustomPrice& lhs, const CustomPrice& rhs) {
        return lhs.value < rhs.value;
      });

  EXPECT_TRUE(AnyTypeComparator::Compare(p1, p2));
  EXPECT_FALSE(AnyTypeComparator::Compare(p2, p1));
}

TEST_F(AnyTypeComparatorTest,
       UnregisteredCustomTypesGracefullyFallbackToStrings) {
  std::any p1 = CustomPrice{9.0};
  std::any p2 = CustomPrice{15.0};

  // Provide string serialization logic for fallback verification
  AnyToStringTranslator::Register<CustomPrice>(
      [](const CustomPrice& p) { return std::format("${:.2f}", p.value); });

  // "$9.00" < "$15.00" lexicographically ('1' comes before '9')
  EXPECT_FALSE(AnyTypeComparator::Compare(p1, p2));
  EXPECT_TRUE(AnyTypeComparator::Compare(p2, p1));
}

// ============================================================================
// PROXY INTEGRATION HOOKS (FACTORY CLOSURE)
// ============================================================================

TEST_F(AnyTypeComparatorTest,
       SortingCallbackFactoryHandlesModelIndicesAndDirections) {
  StringMatrixModel model(2, 1);
  model.setCell(0, 0, 500);  // Row 0
  model.setCell(1, 0, 100);  // Row 1

  ModelIndex idx0 = model.index(0, 0);
  ModelIndex idx1 = model.index(1, 0);

  auto asc_sorter =
      AnyTypeComparator::MakeSortCallback(ItemRole::DisplayRole, true);
  auto desc_sorter =
      AnyTypeComparator::MakeSortCallback(ItemRole::DisplayRole, false);

  // Ascending Evaluation: idx1 (100) < idx0 (500)
  EXPECT_TRUE(asc_sorter(idx1, idx0));
  EXPECT_FALSE(asc_sorter(idx0, idx1));

  // Descending Evaluation: Flipped results
  EXPECT_TRUE(desc_sorter(idx0, idx1));
  EXPECT_FALSE(desc_sorter(idx1, idx0));
}

TEST_F(AnyTypeComparatorTest, SortingCallbackProtectsAgainstInvalidIndices) {
  StringMatrixModel model(1, 1);
  model.setCell(0, 0, 42);

  ModelIndex validIdx = model.index(0, 0);
  ModelIndex invalidIdx;

  auto sorter =
      AnyTypeComparator::MakeSortCallback(ItemRole::DisplayRole, true);

  // Invalid parameters are sorted lower than active data nodes
  EXPECT_TRUE(sorter(invalidIdx, validIdx));
  EXPECT_FALSE(sorter(validIdx, invalidIdx));
  EXPECT_FALSE(sorter(invalidIdx, invalidIdx));
}

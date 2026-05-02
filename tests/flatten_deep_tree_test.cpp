#include <gtest/gtest.h>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include <ftxmodel/flatten_tree_proxy_model.hpp>
#include <ftxmodel/json_property_tree_model.hpp>

using namespace ftxmodel;

class FlattenStringLiteralProxyTest : public ::testing::Test {
 protected:
  std::shared_ptr<JsonPropertyTreeModel> sourceModel;
  std::unique_ptr<FlattenTreeProxyModel> proxyModel;

  void SetUp() override {
    // 1. Define a large, multi-branch cluster topology using a C++ raw string
    // literal Contains complex nested structures: deep configurations,
    // multi-element arrays, and multiple object layers
    auto rawJsonLiteral = R"(
        {
            "environment": "production",
            "networking": {
                "domain": "internal.lan",
                "dns_servers": ["10.0.0.2", "10.0.0.3"],
                "firewall": {
                    "enabled": true,
                    "policy": "DROP"
                }
            },
            "services": [
                {
                    "name": "auth-service",
                    "replicas": 3
                },
                {
                    "name": "gateway-service",
                    "replicas": 2
                }
            ],
            "version_hash": "a8f3c12"
        }
        )";

    // 2. Parse the string literal directly via nlohmann::json
    nlohmann::json parsedJson = nlohmann::json::parse(rawJsonLiteral);

    // 3. Assemble the pipeline model architecture
    sourceModel =
        std::make_shared<JsonPropertyTreeModel>(std::move(parsedJson));
    proxyModel = std::make_unique<FlattenTreeProxyModel>();
    proxyModel->setSourceModel(sourceModel);
  }

  // Helper helper to match a row index back to its string display key easily
  std::string keyAt(int row) {
    ModelIndex idx = proxyModel->index(row, 0);
    return std::any_cast<std::string>(
        proxyModel->data(idx, ItemRole::DisplayRole));
  }

  std::string valAt(int row) {
    ModelIndex idx = proxyModel->index(row, 2);
    return std::any_cast<std::string>(
        proxyModel->data(idx, ItemRole::DisplayRole));
  }
};

// =========================================================================
// TEST 1: COMPLETE INITIAL TIMELINE FLAT VERIFICATION
// =========================================================================
TEST_F(FlattenStringLiteralProxyTest, InitialRootLevelIsFlatAndCorrect) {
  // Top level contains exactly 4 properties: "environment", "networking",
  // "services", "version_hash"
  ASSERT_EQ(proxyModel->rowCount(), 4);

  EXPECT_EQ(keyAt(0), "environment");
  EXPECT_EQ(valAt(0), "production");
  EXPECT_EQ(keyAt(1), "networking");
  EXPECT_EQ(valAt(1), "{3 fields}");
  EXPECT_EQ(keyAt(2), "services");
  EXPECT_EQ(valAt(2), "[2 items]");
  EXPECT_EQ(keyAt(3), "version_hash");
  EXPECT_EQ(valAt(3), "a8f3c12");
}

// =========================================================================
// TEST 2: STEPPED RECURSIVE EXPANSION LOOPS (DEEP DRILLING)
// =========================================================================
TEST_F(FlattenStringLiteralProxyTest,
       SequentialExpansionExpandsViewportCorrectly) {
  // --- Step A: Expand "networking" (Row 1) ---
  // Has 3 fields: "domain", "dns_servers", "firewall"
  // Total should go from 4 -> (4 + 3) = 7
  proxyModel->expand(1);
  ASSERT_EQ(proxyModel->rowCount(), 7);

  // Timeline layout check:
  // 0: environment
  // 1: networking (Expanded)
  // 2:   ├─ dns_servers
  // 3:   ├─ domain
  // 4:   └─ firewall
  // 5: services
  // 6: version_hash
  EXPECT_EQ(keyAt(2), "dns_servers");
  EXPECT_EQ(valAt(2), "[2 items]");
  EXPECT_EQ(keyAt(3), "domain");
  EXPECT_EQ(keyAt(4), "firewall");
  EXPECT_EQ(valAt(4), "{2 fields}");
  EXPECT_EQ(keyAt(5), "services");  // Pushed down properly

  // --- Step B: Drill deeper into the nested "firewall" object (Row 4) ---
  // Has 2 fields: "enabled", "policy"
  // Total should go from 7 -> (7 + 2) = 9
  proxyModel->expand(4);
  ASSERT_EQ(proxyModel->rowCount(), 9);

  // Timeline layout check:
  // 4: firewall (Expanded)
  // 5:   ├─ enabled
  // 6:   └─ policy
  // 7: services
  EXPECT_EQ(keyAt(5), "enabled");
  EXPECT_EQ(valAt(5), "true");
  EXPECT_EQ(keyAt(6), "policy");
  EXPECT_EQ(valAt(6), "DROP");
  EXPECT_EQ(keyAt(7), "services");

  // --- Step C: Drill into "dns_servers" string array (Row 3) ---
  // Contains 2 elements: ["10.0.0.2", "10.0.0.3"]
  // Total should go from 9 -> (9 + 2) = 11
  proxyModel->expand(2);
  ASSERT_EQ(proxyModel->rowCount(), 11);

  // Check that array index sub-keys are generated and formatted
  EXPECT_EQ(keyAt(3), "[0]");
  EXPECT_EQ(valAt(3), "10.0.0.2");
  EXPECT_EQ(keyAt(4), "[1]");
  EXPECT_EQ(valAt(4), "10.0.0.3");

  EXPECT_EQ(keyAt(5), "domain");
}

// =========================================================================
// TEST 3: FULL COLLAPSE BRANCH RECOVERY TESTS
// =========================================================================
TEST_F(FlattenStringLiteralProxyTest,
       CollapsingRootBranchInstantlyHidesAllDescendants) {
  // 1. Fully expand the networking segment out down to its maximum footprint
  proxyModel->expand(1);  // expand networking
  proxyModel->expand(2);  // expand dns_servers
  proxyModel->expand(6);  // expand firewall
  ASSERT_EQ(proxyModel->rowCount(), 11);

  // 2. Collapse the parent anchor node "networking" at Row 1
  // This should erase all nested sub-configurations from the linear list in one
  // step, returning the master timeline immediately back to 4 base rows.
  proxyModel->collapse(1);

  EXPECT_FALSE(proxyModel->isExpanded(1));
  ASSERT_EQ(proxyModel->rowCount(), 4);

  // Confirm that indices snapped back to their original configuration order
  EXPECT_EQ(keyAt(1), "networking");
  EXPECT_EQ(keyAt(2), "services");
  EXPECT_EQ(keyAt(3), "version_hash");
}

// =========================================================================
// TEST 4: COMPLEX ARRAY COMPOSITE NODE VALIDATION
// =========================================================================
TEST_F(FlattenStringLiteralProxyTest,
       ExpandArrayObjectsAndVerifyBijectiveMapping) {
  // "services" is initially located at Row 2. Let's expand the array.
  proxyModel->expand(2);

  // Contains two objects: [0] and [1]. Expected rows: 4 + 2 = 6.
  ASSERT_EQ(proxyModel->rowCount(), 6);
  EXPECT_EQ(keyAt(3), "[0]");
  EXPECT_EQ(valAt(3), "{2 fields}");
  EXPECT_EQ(keyAt(4), "[1]");
  EXPECT_EQ(valAt(4), "{2 fields}");

  // Expand the first service object profile at Row 3 ("auth-service")
  // Contains: "name" and "replicas". Expected rows: 6 + 2 = 8.
  proxyModel->expand(3);
  ASSERT_EQ(proxyModel->rowCount(), 8);

  // Final check on structure mapping precision:
  // 2: services
  // 3:   ├─ [0] (Expanded)
  // 4:   │    ├─ name
  // 5:   │    └─ replicas
  // 6:   └─ [1] (Collapsed)
  // 7: version_hash
  EXPECT_EQ(keyAt(4), "name");
  EXPECT_EQ(valAt(4), "auth-service");
  EXPECT_EQ(keyAt(5), "replicas");
  EXPECT_EQ(valAt(5), "3");
  EXPECT_EQ(keyAt(6), "[1]");
  EXPECT_EQ(valAt(6), "{2 fields}");
  EXPECT_EQ(keyAt(7), "version_hash");

  // Ensure round-trip model matrix conversions remain bijectively unbroken
  for (int i = 0; i < proxyModel->rowCount(); ++i) {
    ModelIndex proxyIdx = proxyModel->index(i, 0);
    ModelIndex sourceIdx = proxyModel->mapToSource(proxyIdx);

    ASSERT_TRUE(sourceIdx.isValid());
    EXPECT_EQ(proxyModel->mapFromSource(sourceIdx).row(), i);
  }
}

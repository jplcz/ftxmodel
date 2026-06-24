#include <gtest/gtest.h>
#include <any>
#include <memory>
#include <string>

#include <ftxmodel/polymorphic_vector_table_model.hpp>

using namespace ftxmodel;

namespace {
struct ServiceMetric {
  std::string serviceName;
  double cpuLoad;
};

struct AuditLog {
  int eventCode;
  std::string details;
};
}  // namespace

class PolymorphicVectorTableModelTest : public ::testing::Test {
 protected:
  void SetUp() override {
    model = std::make_unique<PolymorphicVectorTableModel>();

    // Declare horizontal column boundaries
    model->declareColumn("Primary Identifier");
    model->declareColumn("Metric / Context Attributes");

    //  Register ServiceMetric globally per model
    model->registerTypeHandler<ServiceMetric>(
        [](const std::any& rawAny, int column, ItemRole role) -> std::any {
          if (role != ItemRole::DisplayRole) {
            return {};
          }
          const auto& metric = std::any_cast<const ServiceMetric&>(rawAny);

          if (column == 0) {
            return metric.serviceName;
          }
          if (column == 1) {
            return std::to_string(metric.cpuLoad) + "% CPU";
          }
          return {};
        });

    // Register AuditLog globally per model with mutation support
    model->registerTypeHandler<AuditLog>(
        [](const std::any& rawAny, int column, ItemRole role) -> std::any {
          if (role != ItemRole::DisplayRole && role != ItemRole::EditRole) {
            return {};
          }
          const auto& log = std::any_cast<const AuditLog&>(rawAny);

          if (column == 0) {
            return log.eventCode;
          }
          if (column == 1) {
            return log.details;
          }
          return {};
        },
        [](std::any& rawAny, int column, const std::any& incomingVal,
           ItemRole role) -> bool {
          if (role != ItemRole::EditRole) {
            return false;
          }
          auto& log = std::any_cast<AuditLog&>(rawAny);

          if (column == 0 && incomingVal.type() == typeid(int)) {
            log.eventCode = std::any_cast<int>(incomingVal);
            return true;
          }
          if (column == 1 && incomingVal.type() == typeid(std::string)) {
            log.details = std::any_cast<std::string>(incomingVal);
            return true;
          }
          return false;
        });

    // Set custom type unique tracking lookups
    model->setKeyExtractor([](const std::any& rowAny) -> UniqueNodeId {
      if (rowAny.type() == typeid(ServiceMetric)) {
        return std::any_cast<const ServiceMetric&>(rowAny).serviceName;
      }
      if (rowAny.type() == typeid(AuditLog)) {
        return static_cast<std::int64_t>(
            std::any_cast<const AuditLog&>(rowAny).eventCode);
      }
      return {nullptr};
    });
  }

  std::unique_ptr<PolymorphicVectorTableModel> model;
};

TEST_F(PolymorphicVectorTableModelTest, EmptyStateDimensions) {
  auto emptyModel = std::make_unique<PolymorphicVectorTableModel>();
  EXPECT_EQ(emptyModel->rowCount(), 0);
  EXPECT_EQ(emptyModel->columnCount(), 0);
}

TEST_F(PolymorphicVectorTableModelTest,
       RoutesColumnsPolymorphicallyByGlobalType) {
  // Append completely different types to the raw flat list sequentially
  model->appendRowItem(ServiceMetric{"nginx-router", 12.4});
  model->appendRowItem(AuditLog{501, "DB Connection Timeout"});

  EXPECT_EQ(model->rowCount(), 2);
  EXPECT_EQ(model->columnCount(), 2);

  ModelIndex metricRowCol0 = model->index(0, 0);
  ModelIndex metricRowCol1 = model->index(0, 1);
  ModelIndex logRowCol0 = model->index(1, 0);
  ModelIndex logRowCol1 = model->index(1, 1);

  // Verify first row extracts fields matching ServiceMetric logic rules
  EXPECT_EQ(model->textData(metricRowCol0), "nginx-router");
  EXPECT_TRUE(model->textData(metricRowCol1).find("12.4") != std::string::npos);

  // Verify second row extracts fields matching AuditLog logic rules
  EXPECT_EQ(model->textData(logRowCol0), "501");
  EXPECT_EQ(model->textData(logRowCol1), "DB Connection Timeout");
}

TEST_F(PolymorphicVectorTableModelTest, UnhandledTypeReturnsEmptyPayload) {
  struct UnmanagedType {
    std::string value = "ghost";
  };
  model->appendRowItem(UnmanagedType{});

  ModelIndex idx = model->index(0, 0);
  EXPECT_TRUE(idx.isValid());

  // Unhandled type should match nothing inside lookups, returning a safe, empty
  // any wrapper
  EXPECT_TRUE(model->textData(idx).empty());
}

TEST_F(PolymorphicVectorTableModelTest, MutationUpdatesInternalTypePayload) {
  model->appendRowItem(AuditLog{200, "Initial Audit Handshake"});
  ModelIndex targetIdx = model->index(0, 1);  // Row 0, Column 1 ("Details")

  bool signalFired = false;
  model->dataChanged.connect(
      [&](const ModelIndex& topLeft, const ModelIndex& bottomRight) {
        if (topLeft == targetIdx && bottomRight == targetIdx) {
          signalFired = true;
        }
      });

  // Perform state write validation pass
  bool success = model->setData(targetIdx, std::string("Handshake Confirmed"),
                                ItemRole::EditRole);

  EXPECT_TRUE(success);
  EXPECT_TRUE(signalFired);
  EXPECT_EQ(model->textData(targetIdx), "Handshake Confirmed");
}

TEST_F(PolymorphicVectorTableModelTest, RejectsEditsOnReadOnlyRegisteredTypes) {
  model->appendRowItem(ServiceMetric{"ssh-daemon", 0.1});
  ModelIndex targetIdx = model->index(0, 0);

  // ServiceMetric was not configured with a mutator, so modifying it must fail
  // cleanly
  bool success = model->setData(targetIdx, std::string("malicious-process"),
                                ItemRole::EditRole);
  EXPECT_FALSE(success);
  EXPECT_EQ(model->textData(targetIdx), "ssh-daemon");  // Content unchanged
}

TEST_F(PolymorphicVectorTableModelTest, LazyIdentityCacheValidations) {
  model->appendRowItem(ServiceMetric{"redis-cache", 45.2});
  model->appendRowItem(AuditLog{404, "Page Not Found"});

  // Query constant-time dynamic key index map lookups
  UniqueNodeId serviceSearchKey = std::string("redis-cache");
  UniqueNodeId logSearchKey = std::int64_t(404);

  ModelIndex foundService = model->findIndexById(serviceSearchKey);
  ModelIndex foundLog = model->findIndexById(logSearchKey);

  EXPECT_TRUE(foundService.isValid());
  EXPECT_EQ(foundService.row(), 0);

  EXPECT_TRUE(foundLog.isValid());
  EXPECT_EQ(foundLog.row(), 1);
}

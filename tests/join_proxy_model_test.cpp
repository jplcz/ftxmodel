#include <gtest/gtest.h>
#include <ftxmodel/join_proxy_model.hpp>

using namespace ftxmodel;

class TableStubModel : public AbstractItemModel {
 public:
  int rows = 0;
  int cols = 0;
  std::vector<std::vector<std::any>> grid;
  std::vector<std::any> headers;

  TableStubModel(int r, int c) : rows(r), cols(c) {
    grid.resize(rows, std::vector<std::any>(cols, std::any()));
  }

  ModelIndex index(int r, int c, const ModelIndex& p) const override {
    if (p.isValid() || r < 0 || r >= rows || c < 0 || c >= cols) {
      return {};
    }
    return createIndex(
        r, c);  // Assuming your base model provides a way to forge indices
  }

  ModelIndex parent(const ModelIndex&) const override { return {}; }
  int rowCount(const ModelIndex& p) const override {
    return p.isValid() ? 0 : rows;
  }
  int columnCount(const ModelIndex& p) const override {
    return p.isValid() ? 0 : cols;
  }
  bool hasChildren(const ModelIndex& p) const override {
    return !p.isValid() && rows > 0 && cols > 0;
  }

  std::any data(const ModelIndex& idx, ItemRole role) const override {
    if (!idx.isValid() || role != ItemRole::DisplayRole) {
      return {};
    }
    return grid[idx.row()][idx.column()];
  }

  bool setData(const ModelIndex& idx,
               const std::any& val,
               ItemRole role) override {
    if (!idx.isValid() || role != ItemRole::DisplayRole) {
      return false;
    }
    grid[idx.row()][idx.column()] = val;
    return true;
  }

  std::any headerData(int sec, Orientation ori, ItemRole role) const override {
    if (ori == Orientation::Horizontal && sec < headers.size() &&
        role == ItemRole::DisplayRole) {
      return headers[sec];
    }
    return {};
  }
  // Stub remaining abstract methods...
};

class JoinProxyModelTest : public ::testing::Test {
 protected:
  void SetUp() override {
    proxyModel = std::make_unique<JoinProxyModel>();

    // Setup Left/Source Model A (3x2)
    modelA = std::make_shared<TableStubModel>(3, 2);
    modelA->grid[0][0] = std::string("A_00");
    modelA->grid[1][1] = std::string("A_11");
    modelA->headers = {std::string("H_A0"), std::string("H_A1")};

    // Setup Right/Source Model B (2x3) - purposefully asymmetric row/col count
    modelB = std::make_shared<TableStubModel>(2, 3);
    modelB->grid[0][0] = std::string("B_00");
    modelB->grid[1][2] = std::string("B_12");
    modelB->headers = {std::string("H_B0"), std::string("H_B1"),
                       std::string("H_B2")};

    proxyModel->addSourceModel(modelA);
    proxyModel->addSourceModel(modelB);
  }

  std::unique_ptr<JoinProxyModel> proxyModel;
  std::shared_ptr<TableStubModel> modelA;
  std::shared_ptr<TableStubModel> modelB;
};

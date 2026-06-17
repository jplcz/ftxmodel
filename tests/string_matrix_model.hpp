#pragma once
#include <ftxmodel/abstract_item_model.hpp>

class StringMatrixModel : public ftxmodel::AbstractItemModel {
 public:
  struct Cell {
    std::any display_data;
    std::any unique_id;
  };

  StringMatrixModel(int rows, int cols)
      : m_matrix(rows, std::vector<Cell>(cols)) {}

  void setCell(int row, int col, std::any display, std::any id = {}) {
    m_matrix[row][col].display_data = std::move(display);
    m_matrix[row][col].unique_id = std::move(id);
  }

  ftxmodel::ModelIndex index(int row,
                             int column,
                             const ftxmodel::ModelIndex& parent =
                                 ftxmodel::ModelIndex()) const override {
    if (parent.isValid() || row < 0 || row >= rowCount() || column < 0 ||
        column >= columnCount()) {
      return {};
    }
    return createIndex(row, column, const_cast<Cell*>(&m_matrix[row][column]));
  }

  ftxmodel::ModelIndex parent(const ftxmodel::ModelIndex&) const override {
    return {};
  }
  int rowCount(const ftxmodel::ModelIndex& parent =
                   ftxmodel::ModelIndex()) const override {
    return parent.isValid() ? 0 : static_cast<int>(m_matrix.size());
  }
  int columnCount(const ftxmodel::ModelIndex& parent =
                      ftxmodel::ModelIndex()) const override {
    return parent.isValid() ? 0 : static_cast<int>(m_matrix[0].size());
  }

  std::any data(const ftxmodel::ModelIndex& index,
                ftxmodel::ItemRole role) const override {
    if (!index.isValid()) {
      return {};
    }
    auto* cell = static_cast<Cell*>(index.internalPointer());
    if (role == ftxmodel::ItemRole::DisplayRole ||
        role == ftxmodel::ItemRole::EditRole) {
      return cell->display_data;
    }
    if (role == ftxmodel::ItemRole::UniqueIdentifierRole) {
      return cell->unique_id;
    }
    return {};
  }

 private:
  std::vector<std::vector<Cell>> m_matrix;
};

class SimpleGridModel : public ftxmodel::AbstractItemModel {
 public:
  SimpleGridModel(int rows, int cols, const std::string& prefix)
      : m_rows(rows), m_cols(cols), m_prefix(prefix) {
    m_data.resize(rows, std::vector<std::any>(cols));
    for (int r = 0; r < rows; ++r) {
      for (int c = 0; c < cols; ++c) {
        m_data[r][c] =
            prefix + "_" + std::to_string(r) + "x" + std::to_string(c);
      }
    }
  }

  ftxmodel::ModelIndex index(int row,
                             int column,
                             const ftxmodel::ModelIndex& parent =
                                 ftxmodel::ModelIndex()) const override {
    if (parent.isValid() || row < 0 || row >= m_rows || column < 0 ||
        column >= m_cols) {
      return {};
    }
    return createIndex(row, column, nullptr);
  }

  ftxmodel::ModelIndex parent(const ftxmodel::ModelIndex&) const override {
    return {};
  }
  int rowCount(const ftxmodel::ModelIndex& parent =
                   ftxmodel::ModelIndex()) const override {
    return parent.isValid() ? 0 : m_rows;
  }
  int columnCount(const ftxmodel::ModelIndex& parent =
                      ftxmodel::ModelIndex()) const override {
    return parent.isValid() ? 0 : m_cols;
  }

  std::any data(const ftxmodel::ModelIndex& index,
                ftxmodel::ItemRole role) const override {
    if (!index.isValid() || role != ftxmodel::ItemRole::DisplayRole) {
      return {};
    }
    return m_data[index.row()][index.column()];
  }

  bool setData(const ftxmodel::ModelIndex& index,
               const std::any& value,
               ftxmodel::ItemRole role) override {
    if (!index.isValid() || role != ftxmodel::ItemRole::EditRole) {
      return false;
    }
    m_data[index.row()][index.column()] = value;
    return true;
  }

  ftxmodel::UniqueNodeId uniqueId(
      const ftxmodel::ModelIndex& index) const override {
    if (!index.isValid()) {
      return {nullptr};
    }
    return {m_prefix + "_id_" + std::to_string(index.row()) + "x" +
            std::to_string(index.column())};
  }

 private:
  int m_rows;
  int m_cols;
  std::string m_prefix;
  std::vector<std::vector<std::any>> m_data;
};

#pragma once
#include <climits>
#include <memory>
#include <string>
#include <vector>
#include "abstract_item_model.hpp"

namespace ftxmodel {
/**
 * @struct StandardItem
 * @brief Represents a single data cell inside a 2D Grid matrix layout.
 */
struct StandardItem {
  std::string display_text;
  std::any custom_data;
  // Add styling metadata parameters here later (e.g., color, alignment)
};

/**
 * @class StandardItemModel
 * @brief An all-purpose, multi-column 2D table grid data store.
 *
 * This class is not meant for large data sets as it uses
 * "static" vector<vector<T>> storage (e.g. 1000x1000 grid would
 * allocate 1000000 nodes)
 */
class StandardItemModel : public AbstractItemModel {
 public:
  ~StandardItemModel() override = default;

  StandardItemModel(const int rows, const int columns) {
    if (rows > 0 && columns > 0) {
      setDimensions(rows, columns);
    }
  }

  ModelIndex index(const int row,
                   const int column,
                   const ModelIndex& parent = ModelIndex()) const override {
    if (parent.isValid() || row < 0 || column < 0 || row >= rows_ ||
        column >= columns_) {
      return {};
    }
    return createIndex(row, column, &(*standardItems_[row])[column]);
  }

  ModelIndex parent(const ModelIndex&) const override { return {}; }

  int rowCount(const ModelIndex& parent = ModelIndex()) const override {
    if (parent.isValid()) {
      return 0;
    }
    return rows_;
  }

  int columnCount(const ModelIndex& parent = ModelIndex()) const override {
    if (parent.isValid()) {
      return 0;
    }
    return columns_;
  }

  bool hasChildren(const ModelIndex& parent) const override {
    return !parent.isValid() && rows_ > 0 && columns_ > 0;
  }

  std::any data(const ModelIndex& index,
                ItemRole role = ItemRole::DisplayRole) const override {
    if (!index.isValid() || index.row() >= rows_ ||
        index.column() >= columns_) {
      return {};
    }
    const auto& item = (*standardItems_[static_cast<size_t>(
        index.row())])[static_cast<size_t>(index.column())];
    if (role == ItemRole::DisplayRole || role == ItemRole::EditRole) {
      return item.display_text;
    }
    return {};
  }

  bool setData(const ModelIndex& index,
               const std::any& value,
               const ItemRole role) override {
    if (!index.isValid() || index.row() >= rows_ ||
        index.column() >= columns_ || role != ItemRole::EditRole) {
      return false;
    }
    auto& item = (*standardItems_[static_cast<size_t>(
        index.row())])[static_cast<size_t>(index.column())];
    if (value.type() == typeid(std::string)) {
      item.display_text = std::any_cast<std::string>(value);
      dataChanged(index, index);
      return true;
    }
    return false;
  }

  UniqueNodeId uniqueId(const ModelIndex& index) const override {
    if (!index.isValid()) {
      return {nullptr};
    }
    // Return the direct, stable cell internal pointer address token handle.
    return {index.internalPointer()};
  }

  ModelIndex findIndexById(
      const UniqueNodeId& targetId,
      const ModelIndex& parent = ModelIndex()) const override {
    if (parent.isValid() || !std::holds_alternative<const void*>(targetId)) {
      return {};
    }

    const void* targetPtr = std::get<const void*>(targetId);
    if (!targetPtr) {
      return {};
    }

    // Fast Cache Check
    auto it = id_to_index_cache_.find(targetPtr);
    if (it != id_to_index_cache_.end()) {
      const auto& coord = it->second;
      return index(coord.row, coord.column, parent);
    }

    // Lazy-Load Fallback (Only runs if cache was wiped or
    // uninitialized)
    if (id_to_index_cache_.empty() && rows_ > 0) {
      for (int r = 0; r < rows_; ++r) {
        updateCacheForRow(r);
      }
      // Re-check after populating
      it = id_to_index_cache_.find(targetPtr);
      if (it != id_to_index_cache_.end()) {
        const auto& coord = it->second;
        return index(coord.row, coord.column, parent);
      }
    }

    return {};  // Item doesn't exist in memory layout anymore
  }

  void setDimensions(const int rows, const int columns) {
    if (rows == rows_ && columns == columns_) {
      return;
    }

    beginResetModel();

    id_to_index_cache_.clear();
    standardItems_.resize(static_cast<size_t>(rows));

    for (int r = 0; r < rows; ++r) {
      standardItems_[static_cast<size_t>(r)] =
          std::make_unique<std::vector<StandardItem>>(
              static_cast<size_t>(columns));
    }

    columns_ = columns;
    rows_ = rows;

    endResetModel();
  }

  void appendRow(const std::vector<StandardItem>& rowItems) {
    int nextRow = rows_;
    beginInsertRows(ModelIndex(), nextRow, nextRow);

    auto row_vec = std::make_unique<std::vector<StandardItem>>(
        static_cast<size_t>(columns_));
    for (size_t c = 0; c < static_cast<size_t>(columns_) && c < rowItems.size();
         ++c) {
      (*row_vec)[c] = rowItems[c];
    }

    standardItems_.emplace_back(std::move(row_vec));
    rows_++;

    // UPDATE CACHE: Register the new row cells immediately
    updateCacheForRow(nextRow);

    endInsertRows();
  }

  void removeRows(int row, int count) {
    if (row < 0 || count <= 0 || (row + count) > rows_) {
      return;
    }

    beginRemoveRows(ModelIndex(), row, row + count - 1);

    // Clear tracking references for cells being deleted to release map
    // allocations
    for (int r = row; r < row + count; ++r) {
      const auto& rowVector = *standardItems_[static_cast<size_t>(r)];
      for (int c = 0; c < columns_; ++c) {
        id_to_index_cache_.erase(
            static_cast<const void*>(&rowVector[static_cast<size_t>(c)]));
      }
    }

    // Remove rows from vector structure
    const auto startIt = standardItems_.begin() + row;
    standardItems_.erase(startIt, startIt + count);
    rows_ -= count;

    // CRITICAL INVALIDATION: Any rows *below* the deleted section shifted
    // upward! Their pointer addresses are still identical, but their row index
    // coordinates changed. We re-index those specific shifted tracks to ensure
    // synchronization.
    for (int r = row; r < rows_; ++r) {
      updateCacheForRow(r);
    }

    endRemoveRows();
  }

 private:
  void updateCacheForRow(const int row) const {
    const auto& rowVector = *standardItems_[static_cast<size_t>(row)];
    for (int c = 0; c < columns_; ++c) {
      auto cellPtr =
          static_cast<const void*>(&rowVector[static_cast<size_t>(c)]);
      id_to_index_cache_[cellPtr] = GridCoordinate{row, c};
    }
  }

  std::vector<std::unique_ptr<std::vector<StandardItem>>> standardItems_;
  int rows_ = 0;
  int columns_ = 0;
  struct GridCoordinate {
    int row;
    int column;
  };
  mutable std::unordered_map<const void*, GridCoordinate> id_to_index_cache_;
};

}  // namespace ftxmodel

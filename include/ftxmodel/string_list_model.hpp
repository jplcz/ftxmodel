#pragma once
#include <algorithm>
#include <string>
#include <vector>
#include "abstract_list_model.hpp"

namespace ftxmodel {

class StringListModel : public AbstractListModel {
 public:
  explicit StringListModel(std::vector<std::string> items) {
    items_.reserve(items.size());
    for (auto&& item : items) {
      items_.emplace_back(std::make_shared<std::string>(std::move(item)));
    }
  }

  explicit StringListModel(std::vector<std::shared_ptr<std::string>> items)
      : items_(std::move(items)) {}

  // Only mandatory structural override left
  int rowCount(const ModelIndex& parent = ModelIndex()) const override {
    if (parent.isValid()) {
      return 0;  // Guard against malicious view queries
    }
    return static_cast<int>(items_.size());
  }

  // Mandatory data read override
  std::any data(const ModelIndex& index, ItemRole role) const override {
    if (!index.isValid() || index.row() >= rowCount()) {
      return {};
    }

    if (role == ItemRole::DisplayRole || role == ItemRole::EditRole) {
      return *items_[(size_t)index.row()];
    }

    // Explicit role lookup fallback matching our base architecture
    if (role == ItemRole::UniqueIdentifierRole) {
      return uniqueId(index);
    }

    return {};
  }

  // Optional data write override
  bool setData(const ModelIndex& index,
               const std::any& value,
               ItemRole role) override {
    if (!index.isValid() || index.row() >= rowCount() ||
        role != ItemRole::EditRole) {
      return false;
    }

    if (value.type() == typeid(std::string)) {
      *items_[static_cast<size_t>(index.row())] =
          std::any_cast<std::string>(value);

      // Notify views to repaint this specific index slot
      dataChanged(index, index);
      return true;
    }
    return false;
  }

  // ========================================================================
  // Dynamic List Mutations (Using standard Qt-style notification rules)
  // ========================================================================

  /**
   * @brief Appends a single item to the end of array.
   */
  void append(std::string item) {
    int nextRow = rowCount();

    // Notify observers to prepare layout adjustments
    beginInsertRows(ModelIndex(), nextRow, nextRow);
    items_.emplace_back(std::make_shared<std::string>(std::move(item)));
    endInsertRows();
  }

  /**
   * @brief Inserts a new string item at a specific target index position.
   * @param row The target index to occupy (0 to rowCount()).
   * @param item String to insert
   */
  bool insertAt(int row, std::string item) {
    if (row < 0 || row > rowCount()) {
      return false;
    }

    beginInsertRows(ModelIndex(), row, row);
    items_.insert(items_.begin() + row,
                  std::make_shared<std::string>(std::move(item)));
    endInsertRows();
    return true;
  }

  /**
   * @brief Appends a batch vector of items efficiently in a single view update
   * pass.
   */
  void appendBatch(std::vector<std::string> newItems) {
    if (newItems.empty()) {
      return;
    }

    int firstNewRow = rowCount();
    int lastNewRow = firstNewRow + static_cast<int>(newItems.size()) - 1;

    beginInsertRows(ModelIndex(), firstNewRow, lastNewRow);
    items_.reserve(items_.size() + newItems.size());
    for (auto&& item : newItems) {
      items_.emplace_back(std::make_shared<std::string>(std::move(item)));
    }
    endInsertRows();
  }

  /**
   * @brief Erases a single entry from the array structure.
   */
  void removeAt(int row) {
    if (row < 0 || row >= rowCount()) {
      return;
    }

    beginRemoveRows(ModelIndex(), row, row);
    items_.erase(items_.begin() + row);
    endRemoveRows();
  }

  /**
   * @brief Erases everything, returning the model state instantly back to
   * blank.
   */
  void clear() {
    if (items_.empty()) {
      return;
    }

    beginResetModel();
    items_.clear();
    endResetModel();
  }

  /**
   * @brief Inserts a batch vector of strings at a specific target index
   * position.
   * @param row The starting row index where the new batch will be injected.
   * @param newItems The collection of string payloads to insert.
   * @return true if the injection was within bounds and successful, false
   * otherwise.
   */
  bool insertBatchAt(const int row, std::vector<std::string> newItems) {
    if (newItems.empty()) {
      return true;  // No-op, technically successful
    }

    if (row < 0 || row > rowCount()) {
      return false;
    }

    int firstNewRow = row;
    int lastNewRow = row + static_cast<int>(newItems.size()) - 1;

    // Alert observers of the expanding layout boundaries
    beginInsertRows(ModelIndex(), firstNewRow, lastNewRow);

    // Build the vector iterator slice
    std::vector<std::shared_ptr<std::string>> batchToInject;
    batchToInject.reserve(newItems.size());
    for (auto&& item : newItems) {
      batchToInject.emplace_back(
          std::make_shared<std::string>(std::move(item)));
    }

    // Splice the batch directly into the master tracking vector
    items_.insert(items_.begin() + row,
                  std::make_move_iterator(batchToInject.begin()),
                  std::make_move_iterator(batchToInject.end()));

    // Finalize handshake pass to trigger a single, atomic UI repaint
    endInsertRows();
    return true;
  }

  /**
   * @brief Removes a continuous block/slice of rows from the array.
   * @param row The starting row index where deletion begins.
   * @param count The number of elements to delete downstream.
   * @return true if the slice target is completely within bounds, false
   * otherwise.
   */
  bool removeBatchAt(const int row, const int count) {
    if (count <= 0) {
      return true;  // No-op
    }

    int firstRowToRemove = row;
    int lastRowToRemove = row + count - 1;

    // Bound check: ensure the target slice fits completely inside our current
    // layout
    if (row < 0 || lastRowToRemove >= rowCount()) {
      return false;
    }

    // Freeze views and alert layout structures of the collapsing slice
    // window
    beginRemoveRows(ModelIndex(), firstRowToRemove, lastRowToRemove);

    // Erase the continuous memory block from the tracking vector
    auto startIt = items_.begin() + firstRowToRemove;
    auto endIt = startIt + count;
    items_.erase(startIt, endIt);

    // Safely unfreeze the UI loop and adjust view selections
    endRemoveRows();
    return true;
  }

  UniqueNodeId uniqueId(const ModelIndex& index) const override {
    if (!index.isValid() || index.row() >= rowCount()) {
      return UniqueNodeId{nullptr};
    }

    // Because the shared_ptr's heap address is perfectly stable,
    // the raw pointer is now a 100% reliable Unique ID!
    return UniqueNodeId{static_cast<const void*>(
        items_[static_cast<size_t>(index.row())].get())};
  }

  /**
   * @brief Sorts the string list using a custom comparison predicate.
   * @details This allows sorting by custom rules (e.g., length,
   * case-insensitivity) while safely shifting view cursor highlights and proxy
   * caches.
   * @param comp A binary predicate that takes two const std::string& arguments
   * and returns true if the first argument should precede the second.
   */
  template <typename Compare = std::less<>>
  void sort(Compare comp = Compare()) {
    if (items_.size() <= 1) {
      return;  // No-op for empty or single-item arrays
    }

    beginResetModel();
    std::sort(items_.begin(), items_.end(),
              [&](const auto& a, const auto& b) { return comp(*a, *b); });
    endResetModel();
  }

  /**
   * @brief Extracts a deep copy of all string values currently managed by the
   * model.
   * @return A flat std::vector containing the raw string payloads.
   */
  [[nodiscard]] std::vector<std::string> toVector() const {
    std::vector<std::string> rawValues;
    rawValues.reserve(items_.size());

    for (const auto& itemPtr : items_) {
      if (itemPtr) {
        rawValues.push_back(*itemPtr);
      }
    }
    return rawValues;
  }

  /**
   * @brief Returns a copy of the underlying shared_ptr storage vector.
   * @details Ideal for passing immutable dataset snapshots to worker threads
   * while preserving uniqueId stable addresses.
   */
  [[nodiscard]] std::vector<std::shared_ptr<std::string>> toSharedPtrVector()
      const {
    return items_;  // Leverages standard vector copy constructor optimizations
  }

  /**
   * @brief Completely replaces the current dataset with a new collection of raw
   * strings.
   * @details Uses an atomic model reset transaction to ensure attached views
   * reflow their viewports and re-bind cursor highlights via uniqueId mapping
   * efficiently.
   * @param newItems A vector of new string payloads to ingest.
   */
  void replaceAll(std::vector<std::string> newItems) {
    // Freeze view rendering layers cleanly before wiping data structures
    beginResetModel();

    items_.clear();
    items_.reserve(newItems.size());
    for (auto&& item : newItems) {
      items_.emplace_back(std::make_shared<std::string>(std::move(item)));
    }

    // Unfreeze and trigger a single, atomic viewport redraw pass
    endResetModel();
  }

  /**
   * @brief Completely replaces the current dataset with an existing collection
   * of shared_ptrs.
   * @param newSharedItems A vector of shared_ptr string payloads.
   */
  void replaceAll(std::vector<std::shared_ptr<std::string>> newSharedItems) {
    beginResetModel();

    items_ = std::move(newSharedItems);

    endResetModel();
  }

 protected:
  void* internalPointerAt(const int row) const override {
    // Return the stable address of the actual string payload
    return static_cast<void*>(items_.at(static_cast<size_t>(row)).get());
  }

 private:
  std::vector<std::shared_ptr<std::string>> items_;
};

}  // namespace ftxmodel

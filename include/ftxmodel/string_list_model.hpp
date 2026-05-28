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
      *items_[static_cast<size_t>(index.row())] = std::any_cast<std::string>(value);

      // Notify views to repaint this specific index slot
      dataChanged(index, index);
      return true;
    }
    return false;
  }

  // ========================================================================
  // Dynamic List Mutations (Using standard Qt-style notification rules)
  // ========================================================================

  void append(std::string item) {
    int nextRow = rowCount();

    // Notify observers to prepare layout adjustments
    beginInsertRows(ModelIndex(), nextRow, nextRow);
    items_.emplace_back(std::make_shared<std::string>(std::move(item)));
    endInsertRows();
  }

  void removeAt(int row) {
    if (row < 0 || row >= rowCount()) {
      return;
    }

    beginRemoveRows(ModelIndex(), row, row);
    items_.erase(items_.begin() + row);
    endRemoveRows();
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

 protected:
  void* internalPointerAt(int row) const override {
    // Return the stable address of the actual string payload
    return static_cast<void*>(items_[static_cast<size_t>(row)].get());
  }

 private:
  std::vector<std::shared_ptr<std::string>> items_;
};

}  // namespace ftxmodel

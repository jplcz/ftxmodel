#pragma once
#include <ranges>

#include "abstract_list_model.hpp"
#include "ftxui/component/event.hpp"

namespace ftxmodel {

class ShortcutActionModel : public AbstractListModel {
 public:
  struct ShortcutItem {
    std::string name;
    std::string key;
    ftxui::Event event;
    std::function<void()> trigger;
    bool enabled = true;
  };

 private:
  // To keep pointers stable
  std::vector<std::unique_ptr<ShortcutItem>> m_items;

 public:
  explicit ShortcutActionModel(std::vector<ShortcutItem> items) {
    setItems(std::move(items));
  }

  ShortcutItem* itemAt(const int index) const noexcept {
    if (index < 0 || index >= static_cast<int>(m_items.size())) {
      return nullptr;
    } else {
      return m_items[static_cast<size_t>(index)].get();
    }
  }

  void addItem(const ShortcutItem& item) {
    const int count = static_cast<int>(m_items.size());
    beginInsertRows(ModelIndex(), count, count);
    m_items.emplace_back(std::make_unique<ShortcutItem>(std::move(item)));
    endInsertRows();
  }

  void removeItem(int row) {
    if (row < 0 || row >= static_cast<int>(m_items.size())) {
      return;
    }
    beginRemoveRows(ModelIndex(), row, row);
    m_items.erase(m_items.begin() + static_cast<ssize_t>(row));
    endRemoveRows();
  }

  void setItems(std::vector<ShortcutItem> items) {
    beginResetModel();
    m_items.clear();
    m_items.reserve(items.size());
    for (auto&& item : items) {
      m_items.emplace_back(std::make_unique<ShortcutItem>(std::move(item)));
    }
    endResetModel();
  }

  int rowCount(const ModelIndex& parent = ModelIndex()) const override {
    return parent.isValid() ? 0 : static_cast<int>(m_items.size());
  }

  bool hasChildren(const ModelIndex& parent = ModelIndex()) const override {
    return parent.isValid() ? false : !m_items.empty();
  }

  std::any data(const ModelIndex& index, const ItemRole role) const override {
    if (!index.isValid() || index.row() >= static_cast<int>(m_items.size())) {
      return {};
    }

    const auto* item = m_items[static_cast<size_t>(index.row())].get();
    switch (role) {
      case ItemRole::UniqueIdentifierRole:
        return index.row();
      case ItemRole::DisplayRole:
        return item->name;
      case ItemRole::ShortcutRole:
        return item->event;
      case ItemRole::ShortcutTextRole:
        return item->key;
      default:
        return {};
    }
  }

  bool setData(const ModelIndex& index,
               const std::any&,
               ItemRole role) override {
    if (role != ItemRole::EditRole) {
      return false;
    }
    if (!index.isValid() || index.row() >= static_cast<int>(m_items.size())) {
      return false;
    }
    const auto item = m_items[static_cast<size_t>(index.row())].get();
    if (!item->enabled) {
      return false;
    }
    if (const auto& func = item->trigger) {
      func();
      return true;
    } else {
      return false;
    }
  }

  ItemFlags flags(const ModelIndex& index) const override {
    if (!index.isValid() || index.row() >= static_cast<int>(m_items.size())) {
      return ItemFlag::NoItemFlags;
    }
    ItemFlags f = ItemFlag::ItemIsSelectable;
    if (m_items[static_cast<size_t>(index.row())]->enabled) {
      f |= ItemFlag::ItemIsEnabled | ItemFlag::ItemIsEditable;
    }
    return f;
  }

  ModelIndex findIndexById(const UniqueNodeId& targetId,
                           const ModelIndex& parent) const override {
    if (parent.isValid()) {
      return {};
    }
    if (!std::holds_alternative<int64_t>(targetId)) {
      return {};
    }
    const int row = static_cast<int>(std::get<int64_t>(targetId));
    if (row < 0 || row >= static_cast<int>(m_items.size())) {
      return {};
    }
    return index(row, 0, parent);
  }

 protected:
  void* internalPointerAt(int row) const override {
    return m_items[static_cast<size_t>(row)].get();
  }
};

}  // namespace ftxmodel

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
    bool enabled = true;
    std::function<void()> trigger;
    ftxui::Event event;
  };

 private:
  // To keep pointers stable
  std::vector<std::unique_ptr<ShortcutItem>> m_items;

 public:
  static constexpr ItemRole KeyRole = AsItemRole<1>;
  static constexpr ItemRole EventRole = AsItemRole<2>;

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
    m_items.erase(m_items.begin() + static_cast<size_t>(row));
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
    if (!index.isValid() || index.row() >= rowCount(index)) {
      return {};
    }

    const auto* item = m_items[static_cast<size_t>(index.row())].get();
    switch (role) {
      case ItemRole::UniqueIdentifierRole:
        return index.row();
      case ItemRole::DisplayRole:
        return item->name;
      case KeyRole:
        return item->key;
      case EventRole:
        return item->event;
      default:
        return {};
    }
  }

  ItemFlags flags(const ModelIndex& index) const override {
    if (!index.isValid() || index.row() >= static_cast<int>(m_items.size())) {
      return ItemFlag::NoItemFlags;
    }
    ItemFlags f = ItemFlag::ItemIsSelectable;
    if (m_items[static_cast<size_t>(index.row())]->enabled) {
      f |= ItemFlag::ItemIsEditable;
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

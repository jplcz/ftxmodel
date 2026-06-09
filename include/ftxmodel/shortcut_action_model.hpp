#pragma once
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

  void setItems(std::vector<ShortcutItem> items) {
    beginResetModel();
    m_items.clear();
    m_items.reserve(items.size());
    for (auto&& item : items) {
      m_items.emplace_back(std::make_unique<ShortcutItem>(std::move(item)));
    }
    endResetModel();
  }

  int rowCount(const ModelIndex& parent) const override {
    return parent.isValid() ? 0 : static_cast<int>(m_items.size());
  }

  bool hasChildren(const ModelIndex& parent) const override {
    return parent.isValid() ? false : !m_items.empty();
  }

  std::any data(const ModelIndex& index, ItemRole role) const override {
    if (!index.isValid() || index.row() >= rowCount(index)) {
      return {};
    }

    const auto* item = m_items[static_cast<size_t>(index.row())].get();
    switch (role) {
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

 protected:
  void* internalPointerAt(int row) const override {
    return m_items[static_cast<size_t>(row)].get();
  }
};

}  // namespace ftxmodel

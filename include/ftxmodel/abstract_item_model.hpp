#pragma once
#include <any>
#include <format>
#include <sigslot/signal.hpp>
#include <string>
#include <string_view>
#include "model_index.hpp"

namespace ftxmodel {

class AbstractItemModel {
 public:
  virtual ~AbstractItemModel() = default;

  // Returns the index of the item in the model specified by the given row,
  // column and parent index.
  virtual ModelIndex index(int row,
                           int column,
                           const ModelIndex& parent = ModelIndex()) const = 0;

  // Returns the parent of the model item with the given child index.
  virtual ModelIndex parent(const ModelIndex& child) const = 0;

  // Returns the number of rows under the given parent.
  virtual int rowCount(const ModelIndex& parent = ModelIndex()) const = 0;

  // Returns the number of columns for the children of the given parent.
  virtual int columnCount(const ModelIndex& parent = ModelIndex()) const = 0;

  // Returns true if parent has any children; optimized shortcut so views don't
  // call rowCount() blindly.
  virtual bool hasChildren(const ModelIndex& parent = ModelIndex()) const {
    return rowCount(parent) > 0;
  }

  virtual std::any data(const ModelIndex& index,
                        ItemRole role = ItemRole::DisplayRole) const = 0;
  virtual bool setData(const ModelIndex& index,
                       const std::any& value,
                       ItemRole role = ItemRole::EditRole) {
    std::ignore = index;
    std::ignore = value;
    std::ignore = role;
    return false;
  }

  // Returns the data for the given role and section in the header with the
  // specified orientation.
  virtual std::any headerData(int section,
                              Orientation orientation,
                              ItemRole role = ItemRole::DisplayRole) const {
    std::ignore = orientation;
    if (role == ItemRole::DisplayRole) {
      return std::to_string(section);
    }
    return {};
  }

  // Allows updating header data dynamically
  virtual bool setHeaderData(int section,
                             Orientation orientation,
                             const std::any& value,
                             ItemRole role = ItemRole::EditRole) {
    std::ignore = section;
    std::ignore = orientation;
    std::ignore = value;
    std::ignore = role;
    return false;
  }

  // Returns the item flags for the given index (tells the View if an item is
  // greyed out, clickable, or editable)
  virtual ItemFlags flags(const ModelIndex& index) const {
    if (!index.isValid()) {
      return ItemFlag::NoItemFlags;
    }
    return ItemFlag::ItemIsEnabled |
           ItemFlag::ItemIsSelectable;  // Default sensible fallback
  }

  virtual UniqueNodeId uniqueId(const ModelIndex& index) const {
    if (!index.isValid()) {
      return {nullptr};
    }
    const auto roleIdData = data(index, ItemRole::UniqueIdentifierRole);
    if (roleIdData.type() == typeid(UniqueNodeId)) {
      return std::any_cast<UniqueNodeId>(roleIdData);
    }
    if (roleIdData.type() == typeid(std::string)) {
      return {std::any_cast<std::string>(roleIdData)};
    }
    if (roleIdData.type() == typeid(int)) {
      return {std::any_cast<int>(roleIdData)};
    }
    // Fall back to internal pointer address first
    if (index.internalPointer()) {
      return {index.internalPointer()};
    }
    // Final fallback via path
    std::string pathString = "path:";
    ModelIndex current = index;
    while (current.isValid()) {
      pathString = std::format("{}/{}", current.row(), pathString);
      current = parent(current);
    }
    return {pathString};
  }

  virtual ModelIndex findIndexById(
      const UniqueNodeId& targetId,
      const ModelIndex& parent = ModelIndex()) const {
    if (targetId == UniqueNodeId{nullptr}) {
      return {};
    }
    const int rows = rowCount(parent);
    for (int r = 0; r < rows; ++r) {
      ModelIndex currentIdx = index(r, 0, parent);
      if (!currentIdx.isValid()) {
        continue;
      }
      if (uniqueId(currentIdx) == targetId) {
        return currentIdx;
      }
      if (rowCount(currentIdx) > 0) {
        ModelIndex childMath = findIndexById(targetId, currentIdx);
        if (childMath.isValid()) {
          return childMath;
        }
      }
    }
    return ModelIndex();
  }

  // Helper to extract raw text safely for terminal rendering
  std::string textData(const ModelIndex& index,
                       ItemRole role = ItemRole::DisplayRole) const {
    auto val = data(index, role);
    if (val.type() == typeid(std::string)) {
      return std::any_cast<std::string>(val);
    }
    if (val.type() == typeid(std::string_view)) {
      return std::string(std::any_cast<std::string_view>(val));
    }
    if (val.type() == typeid(const char*)) {
      return std::any_cast<const char*>(val);
    }
    return "";
  }

 protected:
  // Helper method for concrete classes to cleanly build indexes
  ModelIndex createIndex(int row, int column, void* ptr = nullptr) const {
    return ModelIndex(row, column, ptr, this);
  }

 public:
  // --- SigSlot API Notifications ---
  sigslot::signal_st<const ModelIndex&, const ModelIndex&> dataChanged;
  sigslot::signal_st<int, int>
      headerDataChanged;  // Notifies when column names change

  // Structural Change Signals
  sigslot::signal_st<const ModelIndex&, int, int> beginInsertRows;
  sigslot::signal_st<> endInsertRows;
  sigslot::signal_st<const ModelIndex&, int, int> beginRemoveRows;
  sigslot::signal_st<> endRemoveRows;

  sigslot::signal_st<const ModelIndex&, int, int> beginInsertColumns;
  sigslot::signal_st<> endInsertColumns;
  sigslot::signal_st<const ModelIndex&, int, int> beginRemoveColumns;
  sigslot::signal_st<> endRemoveColumns;
};

inline ModelIndex ModelIndex::parent() const {
  return m_model ? m_model->parent(*this) : ModelIndex();
}

inline std::any ModelIndex::data(ItemRole role) const {
  return m_model ? m_model->data(*this, role) : std::any();
}

inline ItemFlags ModelIndex::flags() const {
  return m_model ? m_model->flags(*this) : ItemFlag::NoItemFlags;
}

inline UniqueNodeId ModelIndex::uniqueId() const {
  return m_model ? m_model->uniqueId(*this) : UniqueNodeId{nullptr};
}

}  // namespace ftxmodel

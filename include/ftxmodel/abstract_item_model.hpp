#pragma once
#include <any>
#include <sigslot/signal.hpp>
#include <string>
#include <string_view>
#include "model_index.hpp"

namespace ftxmodel {

enum class ItemRole : int { DisplayRole, EditRole, ToolTipRole, CheckedRole };

enum ItemFlag : int {
  NoItemFlags = 0,
  ItemIsEnabled = 1 << 0,
  ItemIsSelectable = 1 << 1,
  ItemIsEditable = 1 << 2,
  ItemIsUserCheckable = 1 << 3
};
using ItemFlags = int;

enum class Orientation {
  Horizontal,  // Horizontal headers (Table column names)
  Vertical     // Vertical headers (Table row numbers/names)
};

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
    return false;
  }

  // Returns the data for the given role and section in the header with the
  // specified orientation.
  virtual std::any headerData(int section,
                              Orientation orientation,
                              ItemRole role = ItemRole::DisplayRole) const {
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

}  // namespace ftxmodel

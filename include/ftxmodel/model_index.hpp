#pragma once
#include <cstdint>
#include <variant>

namespace ftxmodel {

class AbstractItemModel;

enum class ItemRole : int {
  DisplayRole,
  EditRole,
  ToolTipRole,
  CheckedRole,
  UniqueIdentifierRole
};

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

// Unique node ID for each node in container to improve refresh stability
using UniqueNodeId = std::variant<const void*, std::int64_t, std::string>;

struct ModelIndex {
  constexpr ModelIndex() noexcept = default;

  constexpr ModelIndex(int r,
                       int c,
                       void* ptr,
                       const AbstractItemModel* m) noexcept
      : m_row(r), m_column(c), m_internal_pointer(ptr), m_model(m) {}

  constexpr int row() const noexcept { return m_row; }
  constexpr int column() const noexcept { return m_column; }
  constexpr void* internalPointer() const noexcept {
    return m_internal_pointer;
  }
  constexpr bool isValid() const noexcept {
    return m_row >= 0 && m_column >= 0 && m_model != nullptr;
  }

  constexpr bool operator==(const ModelIndex& rhs) const noexcept {
    return m_row == rhs.m_row && m_column == rhs.m_column &&
           m_internal_pointer == rhs.m_internal_pointer &&
           m_model == rhs.m_model;
  }

  ModelIndex parent() const;
  std::any data(ItemRole role = ItemRole::DisplayRole) const;
  ItemFlags flags() const;
  UniqueNodeId uniqueId() const;

 private:
  int m_row = -1;
  int m_column = -1;
  void* m_internal_pointer = nullptr;
  const AbstractItemModel* m_model = nullptr;
};

}  // namespace ftxmodel

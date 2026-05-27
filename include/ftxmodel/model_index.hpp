#pragma once
#include <variant>

namespace ftxmodel {

class AbstractItemModel;

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

 private:
  int m_row = -1;
  int m_column = -1;
  void* m_internal_pointer = nullptr;
  const AbstractItemModel* m_model = nullptr;
};

}  // namespace ftxmodel

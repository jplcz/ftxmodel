#pragma once
#include <algorithm>
#include <any>
#include <cassert>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "abstract_item_model.hpp"

namespace ftxmodel {

/**
 * @class VectorTableModel
 * @brief A concrete 2D table model that wraps a std::vector of custom data
 * structures.
 * VectorTableModel allows developers to bind an array of
 * objects (e.g., structs) directly to a tabular view by defining columns and
 * mapping data roles through lambdas.
 * @tparam T The element type stored within the underlying std::vector.
 */
template <typename T>
class VectorTableModel : public AbstractItemModel {
 public:
  using DataExtractor = std::function<std::any(const T&, ItemRole)>;
  using DataMutator = std::function<bool(T&, const std::any&, ItemRole)>;
  using KeyExtractor = std::function<UniqueNodeId(const T&)>;

  /**
   * @struct ColumnDefinition
   * @brief Describes a single vertical track mapping configuration within the
   * table grid.
   */
  struct ColumnDefinition {
    std::string headerTitle;
    DataExtractor extractor;
    // Optional configuration for editable fields
    DataMutator mutator = nullptr;
  };

  VectorTableModel() = default;
  ~VectorTableModel() override = default;

  // Disallow copying to prevent visual view-cache signal breakages
  VectorTableModel(const VectorTableModel&) = delete;
  VectorTableModel& operator=(const VectorTableModel&) = delete;
  VectorTableModel(VectorTableModel&&) noexcept = default;
  VectorTableModel& operator=(VectorTableModel&&) noexcept = default;

  // ========================================================================
  // AbstractItemModel Interface Contract Implementations
  // ========================================================================

  ModelIndex index(int row,
                   int column,
                   const ModelIndex& parent = ModelIndex()) const override {
    if (parent.isValid()) {
      return {};
    }

    if (row >= 0 && row < static_cast<int>(m_data.size()) && column >= 0 &&
        column < static_cast<int>(m_columns.size())) {
      return createIndex(row, column, const_cast<T*>(&m_data[row]));
    }
    return {};
  }

  ModelIndex parent(const ModelIndex& child) const override {
    // Tables have no hierarchical parents; all valid fields exist at the root
    // level
    std::ignore = child;
    return {};
  }

  int rowCount(const ModelIndex& parent = ModelIndex()) const override {
    if (parent.isValid()) {
      return 0;
    }
    return static_cast<int>(m_data.size());
  }

  int columnCount(const ModelIndex& parent = ModelIndex()) const override {
    if (parent.isValid()) {
      return 0;  // Flat structure
    }
    return static_cast<int>(m_columns.size());
  }

  std::any data(const ModelIndex& index,
                ItemRole role = ItemRole::DisplayRole) const override {
    if (!index.isValid()) {
      return {};
    }

    const int row = index.row();
    const int col = index.column();

    if (row < 0 || row >= static_cast<int>(m_data.size()) || col < 0 ||
        col >= static_cast<int>(m_columns.size())) {
      return {};
    }

    // Intercept UniqueIdentifierRole if a custom key extractor was provided
    if (role == ItemRole::UniqueIdentifierRole && m_keyExtractor) {
      return m_keyExtractor(m_data[row]);
    }

    // Delegate lookup to the column configuration lambda asset
    return m_columns[col].extractor(m_data[row], role);
  }

  bool setData(const ModelIndex& index,
               const std::any& value,
               ItemRole role = ItemRole::EditRole) override {
    if (!index.isValid() || index.parent().isValid()) {
      return false;
    }

    const int row = index.row();
    const int col = index.column();

    if (row < 0 || row >= static_cast<int>(m_data.size()) || col < 0 ||
        col >= static_cast<int>(m_columns.size())) {
      return false;
    }

    const auto& colDef = m_columns[col];
    if (!colDef.mutator) {
      return false;  // Column is read-only
    }

    if (colDef.mutator(m_data[row], value, role)) {
      // Alert views that a cell range has been successfully modified
      this->dataChanged(index, index);
      return true;
    }

    return false;
  }

  std::any headerData(int section,
                      Orientation orientation,
                      ItemRole role = ItemRole::DisplayRole) const override {
    if (orientation == Orientation::Horizontal) {
      if (section >= 0 && section < static_cast<int>(m_columns.size())) {
        if (role == ItemRole::DisplayRole) {
          return m_columns[section].headerTitle;
        }
      }
      return {};
    }

    // Vertical headers default to structural stringified numbers via base class
    // implementation
    return AbstractItemModel::headerData(section, orientation, role);
  }

  ItemFlags flags(const ModelIndex& index) const override {
    if (!index.isValid()) {
      return ItemFlag::NoItemFlags;
    }

    ItemFlags f = ItemFlag::ItemIsEnabled | ItemFlag::ItemIsSelectable;

    const int col = index.column();
    if (col >= 0 && col < static_cast<int>(m_columns.size())) {
      if (m_columns[col].mutator != nullptr) {
        f |= ItemFlag::ItemIsEditable;
      }
    }
    return f;
  }

  // ========================================================================
  // Layout Metric Management Schema APIs
  // ========================================================================

  /**
   * @brief Appends a column configuration mapping rule target onto the right
   * side of the matrix.
   */
  void addColumn(const std::string& title,
                 DataExtractor extractor,
                 DataMutator mutator = nullptr) {
    const int targetColumn = static_cast<int>(m_columns.size());

    this->beginInsertColumns(ModelIndex(), targetColumn, targetColumn);
    m_columns.emplace_back(title, std::move(extractor), std::move(mutator));
    this->endInsertColumns();
  }

  /**
   * @brief Assigns an identity-extraction lambda allowing stable selection
   * tracking across matrix re-sorts.
   */
  void setKeyExtractor(KeyExtractor extractor) {
    m_keyExtractor = std::move(extractor);
  }

  // ========================================================================
  // Source Vector Vector Modification Gateways
  // ========================================================================

  /**
   * @brief Completely replaces the backend buffer contents, triggering a clean
   * grid refresh.
   */
  void setVectorData(std::vector<T> newData) {
    this->beginResetModel();
    m_data = std::move(newData);
    this->endResetModel();
  }

  /**
   * @brief Safely references the internal storage vector. Modifying this
   * directly requires manual signaling.
   */
  [[nodiscard]] const std::vector<T>& vectorData() const noexcept {
    return m_data;
  }

  /**
   * @brief Inserts a single structured item row element at a targeted slot
   * destination position.
   */
  void insertRowItem(int position, const T& item) {
    if (position < 0 || position > static_cast<int>(m_data.size())) {
      position = static_cast<int>(m_data.size());
    }

    this->beginInsertRows(ModelIndex(), position, position);
    m_data.insert(m_data.begin() + position, item);
    this->endInsertRows();
  }

  /**
   * @brief Appends a structured item row element directly onto the bottom
   * layout bounds.
   */
  void appendRowItem(const T& item) {
    insertRowItem(static_cast<int>(m_data.size()), item);
  }

  /**
   * @brief Drops a target block sequence segment slice out of internal data
   * memory layout tracking.
   */
  bool removeRowItem(int position) {
    if (position < 0 || position >= static_cast<int>(m_data.size())) {
      return false;
    }

    this->beginRemoveRows(ModelIndex(), position, position);
    m_data.erase(m_data.begin() + position);
    this->endRemoveRows();
    return true;
  }

  /**
   * @brief Wipes the internal tracking array clear.
   */
  void clear() {
    if (m_data.empty()) {
      return;
    }
    this->beginResetModel();
    m_data.clear();
    this->endResetModel();
  }

 private:
  std::vector<T> m_data;
  std::vector<ColumnDefinition> m_columns;
  KeyExtractor m_keyExtractor = nullptr;
};

}  // namespace ftxmodel

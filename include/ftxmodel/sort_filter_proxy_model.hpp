#pragma once
#include <algorithm>
#include <functional>
#include <memory>
#include <unordered_map>
#include <utility>
#include "abstract_proxy_model.hpp"

namespace ftxmodel {

//! Helper callback which tells whether row or column is visible
using SortProxyModelFilter = std::function<bool(ModelIndex)>;

//! Helper which compares two ModelIndex values
using SortProxyModelLessThan = std::function<bool(ModelIndex, ModelIndex)>;

namespace detail {

struct SortFilterProxyModelImpl {
  /**
   * @brief Manages index translation vectors for a specific parent node
   * context.
   * @details In a hierarchical or grid-based proxy model, filtering and sorting
   * breaks the linear 1:1 relationship between visual (proxy) indices and
   * underlying (source) indices.
   * * A `Mapping` struct stores the state for a single **parent node
   * container**. It handles bidirectional, $O(1)$ lookups to translate rows and
   * columns between the proxy view and the source data engine.
   */
  struct Mapping {
    /** @brief The absolute coordinate of this container node inside the
     * source model.
     * @note If this model represents a flat, 2D grid/list, this will be an
     * invalid root index.
     */
    ModelIndex sourceParent;
    //! Maps a visible visual column index to its actual index in the backing
    //! source model. Layout: proxy_column_to_source[proxy_col] -> source_col
    std::vector<int> proxy_column_to_source;
    //! Maps a visible visual row index to its actual index in the backing
    //! source model. Layout: proxy_row_to_source[proxy_row] -> source_row
    std::vector<int> proxy_row_to_source;
    //! Inverse lookup cache: Maps a raw source column back to its visible proxy
    //! slot (-1 if filtered out). Layout: source_column_to_proxy[source_col] ->
    //! proxy_col
    std::vector<int> source_column_to_proxy;
    //! Inverse lookup cache: Maps a raw source row back to its visible proxy
    //! slot (-1 if filtered out). Layout: source_row_to_proxy[source_row] ->
    //! proxy_row
    std::vector<int> source_row_to_proxy;
  };

  struct InternalIndex {
    int row = -1;
    int column = -1;
    const void* internalPointer = nullptr;

    constexpr InternalIndex() noexcept = default;
    constexpr InternalIndex(const InternalIndex&) noexcept = default;
    constexpr InternalIndex(const ModelIndex& idx)
        : row(idx.row()),
          column(idx.column()),
          internalPointer(idx.internalPointer()) {}

    constexpr bool operator==(const InternalIndex&) const noexcept = default;
  };

  struct InternalIndexHasher {
    std::size_t operator()(const InternalIndex& index) const noexcept {
      if (index.row < 0 || index.column < 0) {
        return 0;
      }

      const std::size_t h1 = std::hash<int>{}(index.row);
      const std::size_t h2 = std::hash<int>{}(index.column);
      const std::size_t h3 = std::hash<const void*>{}(index.internalPointer);

      std::size_t seed = h1;
      seed ^= h2 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
      seed ^= h3 + 0x9e3779b9 + (seed << 6) + (seed >> 2);

      return seed;
    }
  };

  void rebuildParent(const AbstractProxyModel* model,
                     const ModelIndex& sourceParent);

  /**
   * @brief The master structural cache mapping a source parent node to its
   * visual mapping frames.
   */
  std::unordered_map<InternalIndex,
                     std::unique_ptr<Mapping>,
                     InternalIndexHasher>
      mappings;

  SortProxyModelFilter filter;
  SortProxyModelLessThan lessThan;
  bool ascending = true;
  int sort_column = -1;
};

}  // namespace detail

class SortFilterProxyModel : public AbstractProxyModel {
 public:
  SortFilterProxyModel()
      : impl(std::make_unique<detail::SortFilterProxyModelImpl>()) {}

  ~SortFilterProxyModel() override = default;

  void setFilterCallback(SortProxyModelFilter filter);
  void sort(int column, bool ascending = true);
  void setSortCallback(SortProxyModelLessThan lessThan);

  ModelIndex mapFromSource(const ModelIndex& child) const override;
  ModelIndex mapToSource(const ModelIndex& child) const override;
  ModelIndex index(int row,
                   int column,
                   const ModelIndex& parent) const override;
  ModelIndex parent(const ModelIndex& child) const override;
  int rowCount(const ModelIndex& parent) const override;
  int columnCount(const ModelIndex& parent) const override;
  bool hasChildren(const ModelIndex& parent) const override;

 protected:
  void invalidate() override;

 private:
  std::unique_ptr<detail::SortFilterProxyModelImpl> impl;
};

inline void detail::SortFilterProxyModelImpl::rebuildParent(
    const AbstractProxyModel* model,
    const ModelIndex& sourceParent) {
  assert(!sourceParent.isValid() || sourceParent.model() == model);
  // Check if we're done
  const auto sourceModel = model->sourceModel();
  assert(sourceModel != nullptr);

  const int sourceRows = sourceModel->rowCount(sourceParent);
  const int sourceColumns = sourceModel->columnCount(sourceParent);

  if (sourceRows <= 0 || sourceColumns <= 0) {
    return;
  }

  // Create node if needed
  auto node_it = mappings.find(sourceParent);
  if (node_it == mappings.end()) {
    auto node = std::make_unique<Mapping>();
    node->sourceParent = sourceParent;
    node_it =
        mappings.emplace(InternalIndex(sourceParent), std::move(node)).first;
  }

  auto* proxyNode = node_it->second.get();

  // If there are no children, we're done
  if (!sourceModel->hasChildren(sourceParent)) {
    return;
  }

  // If we're populated, there's nothing to do
  if (!proxyNode->proxy_row_to_source.empty() &&
      !proxyNode->proxy_column_to_source.empty()) {
    return;
  }

  // Construct column maps first
  proxyNode->proxy_column_to_source.resize(static_cast<size_t>(sourceColumns));
  proxyNode->source_column_to_proxy.resize(static_cast<size_t>(sourceColumns));
  for (int i = 0; i < sourceColumns; ++i) {
    proxyNode->proxy_column_to_source[static_cast<size_t>(i)] = i;
    proxyNode->source_column_to_proxy[static_cast<size_t>(i)] = i;
  }

  // Construct row map second
  proxyNode->proxy_row_to_source.resize(static_cast<size_t>(sourceRows));
  proxyNode->source_row_to_proxy.resize(static_cast<size_t>(sourceRows));
  for (int i = 0; i < sourceRows; ++i) {
    proxyNode->proxy_row_to_source[static_cast<size_t>(i)] = i;
  }

  // Apply row sort if needed
  if (sort_column >= 0 &&
      sort_column <
          static_cast<int>(proxyNode->proxy_column_to_source.size())) {
    const int sourceColumn =
        proxyNode->proxy_column_to_source[(size_t)sort_column];

    auto compareSource = [&](const ModelIndex& lhs, const ModelIndex& rhs) {
      if (lessThan) {
        if (ascending) {
          return lessThan(lhs, rhs);
        } else {
          return lessThan(rhs, lhs);
        }
      }

      const auto lhs_value = AnyToStringTranslator::Translate(lhs.data());
      const auto rhs_value = AnyToStringTranslator::Translate(rhs.data());

      if (ascending) {
        return lhs_value < rhs_value;
      } else {
        return rhs_value < lhs_value;
      }
    };

    std::ranges::stable_sort(proxyNode->proxy_row_to_source,
                             [&](const int lhs_row, const int rhs_row) {
                               const auto source_lhs = sourceModel->index(
                                   lhs_row, sourceColumn, sourceParent);
                               const auto source_rhs = sourceModel->index(
                                   rhs_row, sourceColumn, sourceParent);
                               return compareSource(source_lhs, source_rhs);
                             });
  }

  // Populate source to proxy map
  for (size_t i = 0; i < proxyNode->proxy_row_to_source.size(); ++i) {
    const int source_index = proxyNode->proxy_row_to_source[i];
    assert(source_index >= 0);
    proxyNode->source_row_to_proxy[static_cast<size_t>(source_index)] =
        static_cast<int>(i);
  }
}

inline void SortFilterProxyModel::setFilterCallback(
    SortProxyModelFilter filter) {
  beginResetModel();
  invalidate();
  impl->filter = std::move(filter);
  endResetModel();
}

inline void SortFilterProxyModel::sort(const int column, const bool ascending) {
  if (column == impl->sort_column && ascending == impl->ascending) {
    return;
  }
  beginResetModel();
  invalidate();
  impl->sort_column = column;
  impl->ascending = ascending;
  endResetModel();
}

inline void SortFilterProxyModel::setSortCallback(
    SortProxyModelLessThan lessThan) {
  beginResetModel();
  invalidate();
  impl->lessThan = std::move(lessThan);
  endResetModel();
}

inline ModelIndex SortFilterProxyModel::mapFromSource(
    const ModelIndex& child) const {
  // Invalid index or lack of source means no index
  if (!sourceModel()) {
    return {};
  }

  if (!child.isValid()) {
    impl->rebuildParent(this, ModelIndex());
    return {};
  }

  assert(child.model() == sourceModel());
  // Find parent of our child in source model. We'll get child coordinate inside
  // parent
  const auto sourceParent = sourceModel()->parent(child);

  // Make sure child mappings exist
  impl->rebuildParent(this, sourceParent);

  // Find mapping node of source child inside proxy
  const auto it = impl->mappings.find(sourceParent);

  // If no mapping has been found, just end now
  if (it == impl->mappings.end()) {
    return {};
  }

  auto* proxyMapping = it->second.get();
  if (child.row() >=
      static_cast<int>(proxyMapping->source_row_to_proxy.size())) {
    return {};
  }
  if (child.column() >=
      static_cast<int>(proxyMapping->source_column_to_proxy.size())) {
    return {};
  }

  return createIndex(
      proxyMapping->source_row_to_proxy[static_cast<size_t>(child.row())],
      proxyMapping->source_column_to_proxy[static_cast<size_t>(child.column())],
      proxyMapping);
}

inline ModelIndex SortFilterProxyModel::mapToSource(
    const ModelIndex& child) const {
  if (!sourceModel()) {
    return {};
  }

  if (!child.isValid()) {
    impl->rebuildParent(this, child);
    return {};
  }

  assert(child.model() == this);
  // Obtain internal mapping of our parent node
  const auto* mapping = static_cast<detail::SortFilterProxyModelImpl::Mapping*>(
      child.internalPointer());
  assert(mapping != nullptr);

  // Make sure child mappings exist
  impl->rebuildParent(this, mapping->sourceParent);

  if (child.row() >= static_cast<int>(mapping->proxy_row_to_source.size())) {
    return {};
  }
  if (child.column() >=
      static_cast<int>(mapping->proxy_column_to_source.size())) {
    return {};
  }
  // Translate index in proxy coordinates to source coordinates
  return sourceModel()->index(
      mapping->proxy_row_to_source[static_cast<size_t>(child.row())],
      mapping->proxy_column_to_source[static_cast<size_t>(child.column())],
      mapping->sourceParent);
}

inline ModelIndex SortFilterProxyModel::index(int row,
                                              int column,
                                              const ModelIndex& parent) const {
  if (row < 0 || column < 0 || !sourceModel()) {
    return {};
  }
  assert(!parent.isValid() || parent.model() == this);
  const auto sourceParent = mapToSource(parent);
  const auto it = impl->mappings.find(sourceParent);
  if (it == impl->mappings.end()) {
    return {};
  }
  auto* proxyMapping = it->second.get();
  if (row >= static_cast<int>(proxyMapping->proxy_row_to_source.size())) {
    return {};
  }
  if (column >= static_cast<int>(proxyMapping->proxy_column_to_source.size())) {
    return {};
  }
  return createIndex(row, column, proxyMapping);
}

inline ModelIndex SortFilterProxyModel::parent(const ModelIndex& child) const {
  if (!child.isValid() || !sourceModel()) {
    return {};
  }
  assert(child.model() == this);
  const auto sourceChild = mapToSource(child);
  if (!sourceChild.isValid()) {
    return {};
  }
  const auto sourceParent = sourceModel()->parent(sourceChild);
  return mapFromSource(sourceParent);
}

inline int SortFilterProxyModel::rowCount(const ModelIndex& parent) const {
  const auto sourceParent = mapToSource(parent);
  const auto it = impl->mappings.find(sourceParent);
  if (it == impl->mappings.end()) {
    return 0;
  }
  return static_cast<int>(it->second->proxy_row_to_source.size());
}

inline int SortFilterProxyModel::columnCount(const ModelIndex& parent) const {
  const auto sourceParent = mapToSource(parent);
  const auto it = impl->mappings.find(sourceParent);
  if (it == impl->mappings.end()) {
    return 0;
  }
  return static_cast<int>(it->second->proxy_column_to_source.size());
}

inline bool SortFilterProxyModel::hasChildren(const ModelIndex& parent) const {
  const auto sourceParent = mapToSource(parent);
  const auto it = impl->mappings.find(sourceParent);
  if (it == impl->mappings.end()) {
    return false;
  }
  const auto* proxyMapping = it->second.get();
  return !proxyMapping->proxy_row_to_source.empty() &&
         !proxyMapping->proxy_column_to_source.empty();
}

inline void SortFilterProxyModel::invalidate() {
  impl->mappings.clear();
}

}  // namespace ftxmodel

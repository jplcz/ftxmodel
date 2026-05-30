#pragma once
#include <algorithm>
#include <functional>
#include <memory>
#include "abstract_proxy_model.hpp"

namespace ftxmodel {

/**
 * @class SortFilterProxyModel
 * @brief Proxy model providing conditional tree-pruning filtration and
 * multi-column sorting.
 * SortFilterProxyModel intercepts structural tree and grid queries between
 * view layers and underlying raw data models. By managing an internal,
 * pointer-stable translation cache using the Pimpl idiom, it allows views to
 * display sorted and filtered slices of datasets without modifying, copying, or
 * duplicating the backend memory structures.
 * * Mappings are entirely data-first (@see UniqueNodeId), making this proxy
 * structurally immune to underlying vector reallocations or runtime index
 * drift.
 */
class SortFilterProxyModel : public AbstractProxyModel {
 public:
  /**
   * @brief Type alias for the dynamic row filtration callback predicate.
   * Receives a pristine, un-mapped **Source Model** Index coordinate handle and
   * must return true if the row passes the filter conditions to remain visible
   * in the view.
   */
  using FilterCallback = std::function<bool(const ModelIndex&)>;

  /**
   * @brief Constructs an empty SortFilterProxyModel with caching subsystems
   * initialized.
   */
  SortFilterProxyModel();

  /**
   * @brief Destructor explicitly declared to support clean teardown of the
   * incomplete Pimpl container.
   */
  ~SortFilterProxyModel() override;

  /**
   * @brief Configures a functional lambda evaluation pass for filtering rows
   * out of view.
   * Changing or updating the callback instantly purges the active visual
   * matrix cache and triggers view repaints.
   * @param callback A predicate function wrapper. Pass `nullptr` to
   * completely disable filtering.
   */
  void setFilterCallback(FilterCallback callback);

  /**
   * @brief Commands the proxy to sort its layout structures based on a target
   * data column track.
   * Automatically detects standard variants stored inside cells
   * (`std::string`, `int`, `std::int64_t`) and runs type-safe stable sorts
   * across them.
   * @param column The targeted data track column index. Pass `-1` to
   * completely disable sorting.
   * @param ascending True for low-to-high tracking, False for high-to-low
   * tracking loops.
   */
  void sort(int column, bool ascending = true);

  /**
   * @brief Translates an incoming visual Proxy Index back to its exact match
   * inside the Source Model.
   * @param proxyIndex A valid coordinate token owned by this proxy model.
   * @return ModelIndex A clean backend index handle bound to the source model
   * authority, or an invalid index if no match exists.
   */
  ModelIndex mapToSource(const ModelIndex& proxyIndex) const override;

  /**
   * @brief Translates a backend Source Index up to its active visible
   * coordinate slot inside this Proxy.
   * @param sourceIndex A valid coordinate token owned by the underlying
   * source data model.
   * @return ModelIndex A visual layout coordinate handle bound to this proxy,
   * or an invalid index if the source item was explicitly filtered out of view.
   */
  ModelIndex mapFromSource(const ModelIndex& sourceIndex) const override;

  /**
   * @brief Queries the filtered count of rows visible beneath a specific proxy
   * parent handle.
   */
  int rowCount(const ModelIndex& parent) const override;

  /**
   * @brief Generates a transient ModelIndex visual handle for a sorted/filtered
   * cell coordinate.
   */
  ModelIndex index(int row,
                   int column,
                   const ModelIndex& parent) const override;

  /**
   * @brief Evaluates the cache map upward to discover the proxy parent of a
   * given proxy child index.
   * @param child The visual proxy index handle whose parent is being
   * requested.
   * @return ModelIndex The proxy parent handle, or an invalid root token if the
   * child is a root node.
   */
  ModelIndex parent(const ModelIndex& child) const override;

 protected:
  /**
   * @brief Internal hook clearing the mapping caches and broadcasting
   * structural updates to views.
   * Automatically called by `setSourceModel()`, `sort()`, and
   * `setFilterCallback()`.
   */
  void invalidate() override;

 private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

// The actual structure where all state and cache mechanics live
struct SortFilterProxyModel::Impl {
  FilterCallback filter_callback = nullptr;
  int sort_column = -1;
  bool sort_ascending = true;

  struct StableNodeMapping {
    const void* source_node_ptr;
    const void* source_parent_ptr;
  };

  // The visual layout cache map
  std::map<const void*, std::vector<StableNodeMapping>> visual_cache;

  // Helper validation checkpoint
  void ensureCache(const SortFilterProxyModel* q) {
    if (visual_cache.empty() && q->sourceModel()) {
      rebuildCacheRecursively(q, ModelIndex());
    }
  }

  void rebuildCacheRecursively(const SortFilterProxyModel* q,
                               const ModelIndex& sourceParent) {
    auto* src = q->sourceModel();
    const int totalSourceRows = src->rowCount(sourceParent);
    const void* parentKey =
        sourceParent.isValid() ? sourceParent.internalPointer() : nullptr;

    std::vector<StableNodeMapping> allowedNodes;

    // Phase 1: Filter
    for (int r = 0; r < totalSourceRows; ++r) {
      ModelIndex currentSrcIdx = src->index(r, 0, sourceParent);
      bool accepted = filter_callback ? filter_callback(currentSrcIdx) : true;

      if (!accepted && src->hasChildren(currentSrcIdx)) {
        accepted = hasMatchingDescendants(q, currentSrcIdx);
      }

      if (accepted) {
        allowedNodes.push_back({currentSrcIdx.internalPointer(), parentKey});
      }
    }

    // Phase 2: Sort
    if (sort_column >= 0 && !allowedNodes.empty()) {
      std::ranges::stable_sort(
          allowedNodes,
          [this, src](const StableNodeMapping& a, const StableNodeMapping& b) {
            const ModelIndex srcParentA =
                src->findIndexById(UniqueNodeId{a.source_parent_ptr});
            ModelIndex idxA =
                src->findIndexById(UniqueNodeId{a.source_node_ptr}, srcParentA);
            idxA = src->index(idxA.row(), sort_column, srcParentA);

            const ModelIndex srcParentB =
                src->findIndexById(UniqueNodeId{b.source_parent_ptr});
            ModelIndex idxB =
                src->findIndexById(UniqueNodeId{b.source_node_ptr}, srcParentB);
            idxB = src->index(idxB.row(), sort_column, srcParentB);

            const std::any valA = src->data(idxA, ItemRole::DisplayRole);
            const std::any valB = src->data(idxB, ItemRole::DisplayRole);

            // Fast-path checking optimization: if both items share the exact
            // same type, extract and evaluate them natively to avoid string
            // conversion overhead.
            if (valA.type() == valB.type()) {
              if (valA.type() == typeid(int)) {
                const bool isLessThan =
                    std::any_cast<int>(valA) < std::any_cast<int>(valB);
                return sort_ascending ? isLessThan : !isLessThan;
              }
              if (valA.type() == typeid(std::int64_t)) {
                const bool isLessThan = std::any_cast<std::int64_t>(valA) <
                                        std::any_cast<std::int64_t>(valB);
                return sort_ascending ? isLessThan : !isLessThan;
              }
              if (valA.type() == typeid(std::string)) {
                const bool isLessThan =
                    std::any_cast<const std::string&>(valA) <
                    std::any_cast<const std::string&>(valB);
                return sort_ascending ? isLessThan : !isLessThan;
              }
            }

            // Unified Fallback Path: If types are mismatched (e.g., int vs
            // int64) or represent unknown/registered structures, seamlessly
            // flatten them via our translation engine.
            const std::string strA = AnyToStringTranslator::Translate(valA);
            const std::string strB = AnyToStringTranslator::Translate(valB);

            const bool isLessThan = strA < strB;
            return sort_ascending ? isLessThan : !isLessThan;
          });
    }

    if (!allowedNodes.empty() || parentKey == nullptr) {
      visual_cache[parentKey] = std::move(allowedNodes);
    }

    // Phase 3: Recurse
    for (int r = 0; r < totalSourceRows; ++r) {
      if (ModelIndex currentSrcIdx = src->index(r, 0, sourceParent);
          src->hasChildren(currentSrcIdx)) {
        rebuildCacheRecursively(q, currentSrcIdx);
      }
    }
  }

  bool hasMatchingDescendants(const SortFilterProxyModel* q,
                              const ModelIndex& parentSourceIdx) {
    const auto* src = q->sourceModel();
    const int rows = src->rowCount(parentSourceIdx);
    for (int r = 0; r < rows; ++r) {
      ModelIndex child = src->index(r, 0, parentSourceIdx);
      if (filter_callback && filter_callback(child)) {
        return true;
      }
      if (src->hasChildren(child) && hasMatchingDescendants(q, child)) {
        return true;
      }
    }
    return false;
  }
};

inline SortFilterProxyModel::SortFilterProxyModel()
    : m_impl(std::make_unique<Impl>()) {}

inline SortFilterProxyModel::~SortFilterProxyModel() {}

inline void SortFilterProxyModel::setFilterCallback(FilterCallback callback) {
  m_impl->filter_callback = std::move(callback);
  invalidate();
}

inline void SortFilterProxyModel::sort(int column, bool ascending) {
  m_impl->sort_column = column;
  m_impl->sort_ascending = ascending;
  invalidate();
}

inline void SortFilterProxyModel::invalidate() {
  m_impl->visual_cache.clear();
  headerDataChanged(0, 0);
}

inline ModelIndex SortFilterProxyModel::mapToSource(
    const ModelIndex& proxyIndex) const {
  if (!sourceModel() || !proxyIndex.isValid()) {
    return {};
  }

  const auto it = m_impl->visual_cache.find(proxyIndex.internalPointer());
  if (it == m_impl->visual_cache.end() ||
      proxyIndex.row() >= static_cast<int>(it->second.size())) {
    return {};
  }

  const auto& nodeMap = it->second[static_cast<size_t>(proxyIndex.row())];
  const ModelIndex sourceParent =
      sourceModel()->findIndexById(UniqueNodeId{nodeMap.source_parent_ptr});
  const ModelIndex sourceMatch = sourceModel()->findIndexById(
      UniqueNodeId{nodeMap.source_node_ptr}, sourceParent);

  if (!sourceMatch.isValid()) {
    return {};
  }
  return sourceModel()->index(sourceMatch.row(), proxyIndex.column(),
                              sourceParent);
}

inline ModelIndex SortFilterProxyModel::mapFromSource(
    const ModelIndex& sourceIndex) const {
  if (!sourceModel() || !sourceIndex.isValid()) {
    return {};
  }

  m_impl->ensureCache(this);

  ModelIndex sourceParent = sourceModel()->parent(sourceIndex);
  const void* parentKey =
      sourceParent.isValid() ? sourceParent.internalPointer() : nullptr;

  auto it = m_impl->visual_cache.find(parentKey);
  if (it != m_impl->visual_cache.end()) {
    const auto& visualRows = it->second;
    const void* targetNodePtr = sourceIndex.internalPointer();

    for (size_t i = 0; i < visualRows.size(); ++i) {
      if (visualRows[i].source_node_ptr == targetNodePtr) {
        return createIndex(static_cast<int>(i), sourceIndex.column(),
                           const_cast<void*>(parentKey));
      }
    }
  }
  return {};
}

inline int SortFilterProxyModel::rowCount(const ModelIndex& parent) const {
  if (!sourceModel()) {
    return 0;
  }
  m_impl->ensureCache(this);

  ModelIndex sourceParent = mapToSource(parent);
  const void* parentKey =
      sourceParent.isValid() ? sourceParent.internalPointer() : nullptr;

  auto it = m_impl->visual_cache.find(parentKey);
  return it != m_impl->visual_cache.end() ? static_cast<int>(it->second.size())
                                          : 0;
}

inline ModelIndex SortFilterProxyModel::index(int row,
                                              int column,
                                              const ModelIndex& parent) const {
  if (!sourceModel() || row < 0 || column < 0) {
    return {};
  }
  m_impl->ensureCache(this);

  ModelIndex sourceParent = mapToSource(parent);
  const void* parentKey =
      sourceParent.isValid() ? sourceParent.internalPointer() : nullptr;

  auto it = m_impl->visual_cache.find(parentKey);
  if (it != m_impl->visual_cache.end() &&
      row < static_cast<int>(it->second.size())) {
    return createIndex(row, column, const_cast<void*>(parentKey));
  }
  return {};
}

inline ModelIndex SortFilterProxyModel::parent(const ModelIndex& child) const {
  if (!sourceModel() || !child.isValid()) {
    return {};
  }

  // Look up the visual cache level where this child resides
  const auto it = m_impl->visual_cache.find(child.internalPointer());
  if (it == m_impl->visual_cache.end() ||
      child.row() >= static_cast<int>(it->second.size())) {
    return {};
  }

  //  Fetch the stable source parent pointer for this child node
  const auto& nodeMap = it->second[static_cast<size_t>(child.row())];
  const void* sourceParentPtr = nodeMap.source_parent_ptr;

  // If the source parent pointer is null, this child is a visual root node!
  if (sourceParentPtr == nullptr) {
    return {};
  }

  // Data-First Resolution: Ask the source model to find the actual layout
  // index handle for this parent so we can safely read its parent-chain
  // topology.
  ModelIndex sourceParentIdx =
      sourceModel()->findIndexById(UniqueNodeId{sourceParentPtr});
  if (!sourceParentIdx.isValid()) {
    return {};
  }

  // Transform the discovered source parent index back up to Proxy Workspace
  // coordinates
  return mapFromSource(sourceParentIdx);
}

}  // namespace ftxmodel

#pragma once
#include <ftxmodel/abstract_proxy_model.hpp>
#include <memory>
#include <unordered_set>

namespace ftxmodel {

class FlattenTreeProxyModel : public AbstractProxyModel {
 public:
  FlattenTreeProxyModel() : impl_(std::make_unique<Impl>()) {}
  ~FlattenTreeProxyModel() override = default;

  ModelIndex mapFromSource(const ModelIndex& child) const override {
    if (!sourceModel() || !child.isValid()) {
      return {};
    }
    impl_->ensureCache(this);
    const void* targetNodePtr = child.internalPointer();
    for (size_t i = 0; i < impl_->flat_lookup_cache.size(); ++i) {
      if (impl_->flat_lookup_cache[i].internalPointer() == targetNodePtr) {
        return createIndex(static_cast<int>(i), child.column(),
                           const_cast<void*>(targetNodePtr));
      }
    }
    return {};
  }

  ModelIndex mapToSource(const ModelIndex& child) const override {
    if (!sourceModel() || !child.isValid()) {
      return {};
    }
    impl_->ensureCache(this);
    if (child.row() >= static_cast<int>(impl_->flat_lookup_cache.size())) {
      return {};
    }
    return impl_->flat_lookup_cache[static_cast<size_t>(child.row())];
  }

  ModelIndex index(const int row,
                   const int column,
                   const ModelIndex& parent = ModelIndex()) const override {
    if (parent.isValid() || row < 0 || column < 0) {
      return {};
    }
    impl_->ensureCache(this);
    if (row >= static_cast<int>(impl_->flat_lookup_cache.size())) {
      return {};
    }

    return createIndex(
        row, column,
        impl_->flat_lookup_cache[static_cast<size_t>(row)].internalPointer());
  }

  ModelIndex parent(const ModelIndex&) const override {
    // Structure is flat so the only parent is root node
    return {};
  }

  int rowCount(const ModelIndex& parent = ModelIndex()) const override {
    if (parent.isValid() || !sourceModel()) {
      return 0;
    }
    impl_->ensureCache(this);
    return static_cast<int>(impl_->flat_lookup_cache.size());
  }

  int columnCount(const ModelIndex& parent = ModelIndex()) const override {
    if (parent.isValid() || !sourceModel()) {
      return 0;
    }
    return sourceModel()->columnCount(ModelIndex());
  }

  bool isExpanded(const int row) const {
    impl_->ensureCache(this);
    if (row < 0 || row >= static_cast<int>(impl_->flat_lookup_cache.size())) {
      return false;
    }
    const ModelIndex localIndex = index(row, 0, ModelIndex());
    const ModelIndex sourceIndex = mapToSource(localIndex);
    return impl_->expanded_nodes.contains(sourceIndex.uniqueId());
  }

  void expand(const int row) {
    impl_->ensureCache(this);
    if (row < 0 || row >= static_cast<int>(impl_->flat_lookup_cache.size())) {
      return;
    }
    beginResetModel();
    const ModelIndex localIndex = index(row, 0, ModelIndex());
    const ModelIndex sourceIndex = mapToSource(localIndex);
    const auto nodeId = sourceIndex.uniqueId();
    if (impl_->expanded_nodes.insert(nodeId).second) {
      invalidate();
    }
    endResetModel();
  }

  void collapse(const int row) {
    impl_->ensureCache(this);
    if (row < 0 || row >= static_cast<int>(impl_->flat_lookup_cache.size())) {
      return;
    }
    const ModelIndex localIndex = index(row, 0, ModelIndex());
    const ModelIndex sourceIndex = mapToSource(localIndex);
    const auto nodeId = sourceIndex.uniqueId();
    beginResetModel();
    if (impl_->expanded_nodes.erase(nodeId) > 0) {
      // Purge only if we'd overrun array size
      if (impl_->expanded_nodes.size() >= impl_->flat_lookup_cache.size()) {
        cleanStaleNodes(true);
      }
      invalidate();
    }
    endResetModel();
  }

  void collapseAll() {
    impl_->ensureCache(this);
    if (!impl_->expanded_nodes.empty()) {
      beginResetModel();
      impl_->expanded_nodes.clear();
      invalidate();
      endResetModel();
    }
  }

  /**
   * @brief Recursively expands every single parent branch node inside the
   * source model topology.
   */
  void expandAll() {
    beginResetModel();
    impl_->ensureCache(this);
    impl_->expanded_nodes.clear();
    impl_->gatherExpandAll(this, ModelIndex());
    invalidate();
    endResetModel();
  }
  /**
   * @brief Recursively expands a specific node and every single one of its
   * hidden structural descendants.
   * @param proxyRow The visual row position target inside the flat list space.
   */
  void expandBranch(const int proxyRow) {
    impl_->ensureCache(this);
    if (proxyRow < 0 ||
        proxyRow >= static_cast<int>(impl_->flat_lookup_cache.size())) {
      return;
    }
    // Map the visual list coordinate row down to its true nested Source handle
    // position
    ModelIndex proxyIdx = index(proxyRow, 0);
    ModelIndex sourceIdx = mapToSource(proxyIdx);

    if (!sourceIdx.isValid() || !sourceModel()->hasChildren(sourceIdx)) {
      return;  // Fast escape boundary if it's a leaf node or empty
    }

    beginResetModel();
    UniqueNodeId branchId = sourceModel()->uniqueId(sourceIdx);
    impl_->expanded_nodes.insert(branchId);

    impl_->gatherExpandAll(this, sourceIdx);
    invalidate();
    endResetModel();
  }

  /**
   * @brief Collapses a specific branch node, hiding it and all of its
   * descendants from view.
   * @param proxyRow The visual row position target inside the flat list space.
   */
  void collapseBranch(const int proxyRow) {
    impl_->ensureCache(this);
    if (proxyRow < 0 ||
        proxyRow >= static_cast<int>(impl_->flat_lookup_cache.size())) {
      return;
    }

    // Map visual list coordinate down to its true nested Source position
    ModelIndex proxyIdx = index(proxyRow, 0);
    ModelIndex sourceIdx = mapToSource(proxyIdx);

    if (!sourceIdx.isValid() || !sourceModel()->hasChildren(sourceIdx)) {
      return;  // Fast escape if it's a leaf node or empty
    }

    beginResetModel();

    UniqueNodeId branchId = sourceModel()->uniqueId(sourceIdx);

    if (impl_->expanded_nodes.erase(branchId) > 0) {
      cleanStaleNodes(true);
      invalidate();  // Rebuild the 1D visual timeline map exactly once
    }
    endResetModel();
  }

 protected:
  void invalidate() override { impl_->flat_lookup_cache.clear(); }

 private:
  void cleanStaleNodes(bool skipInvalidate) {
    if (!sourceModel()) {
      // If the source model was completely detached, all expanded node states
      // are stale!
      impl_->expanded_nodes.clear();
      if (!skipInvalidate) {
        invalidate();
      }
      return;
    }

    std::unordered_set<UniqueNodeId, UniqueNodeIdHash, UniqueNodeIdEqual>
        aliveIds;
    impl_->gatherAllUniqueIds(this, ModelIndex(), aliveIds);

    bool modified = false;
    for (auto it = impl_->expanded_nodes.begin();
         it != impl_->expanded_nodes.end();) {
      if (!aliveIds.contains(*it)) {
        // Safe erasure tracking during iteration loop
        it = impl_->expanded_nodes.erase(it);
        modified = true;
      } else {
        ++it;
      }
    }

    // If anything was actually swept away, refresh our visual layouts
    if (modified && !skipInvalidate) {
      invalidate();
    }
  }

  struct Impl {
    // Set of visible expanded nodes
    std::unordered_set<UniqueNodeId, UniqueNodeIdHash, UniqueNodeIdEqual>
        expanded_nodes;
    // Node index inside underlying model
    std::vector<ModelIndex> flat_lookup_cache;

    void ensureCache(const FlattenTreeProxyModel* q) {
      if (flat_lookup_cache.empty() && q->sourceModel()) {
        rebuildFlatListRecursively(q, ModelIndex());
      }
    }

    void rebuildFlatListRecursively(const FlattenTreeProxyModel* q,
                                    const ModelIndex& sourceParent) {
      const auto* src = q->sourceModel();
      const int rows = src->rowCount(sourceParent);

      for (int r = 0; r < rows; ++r) {
        const ModelIndex srcIdx = src->index(r, 0, sourceParent);
        if (!srcIdx.isValid()) {
          continue;
        }

        flat_lookup_cache.emplace_back(srcIdx);

        // Extract the true data-first identity token for this node
        // ReSharper disable once CppTooWideScopeInitStatement
        const UniqueNodeId nodeId = src->uniqueId(srcIdx);

        // Structural Descent check bound to UniqueNodeId matching contracts
        if (expanded_nodes.contains(nodeId) && src->hasChildren(srcIdx)) {
          rebuildFlatListRecursively(q, srcIdx);
        }
      }
    }

    void gatherExpandAll(const FlattenTreeProxyModel* q,
                         const ModelIndex& sourceParent) {
      const auto* src = q->sourceModel();
      const int rows = src->rowCount(sourceParent);
      for (int r = 0; r < rows; ++r) {
        ModelIndex srcIdx = src->index(r, 0, sourceParent);
        if (!srcIdx.isValid()) {
          continue;
        }
        if (src->hasChildren(srcIdx)) {
          expanded_nodes.insert(srcIdx.uniqueId());
          gatherExpandAll(q, srcIdx);
        }
      }
    }

    void gatherAllUniqueIds(
        const FlattenTreeProxyModel* q,
        const ModelIndex& sourceParent,
        std::unordered_set<UniqueNodeId, UniqueNodeIdHash, UniqueNodeIdEqual>&
            aliveIds) {
      const auto* src = q->sourceModel();
      const int rows = src->rowCount(sourceParent);
      for (int r = 0; r < rows; ++r) {
        const ModelIndex srcIdx = src->index(r, 0, sourceParent);
        if (!srcIdx.isValid()) {
          continue;
        }
        if (src->hasChildren(srcIdx)) {
          aliveIds.insert(src->uniqueId(srcIdx));

          // Keep drilling down to collect nested sub-folder identities
          gatherAllUniqueIds(q, srcIdx, aliveIds);
        }
      }
    }
  };

  std::unique_ptr<Impl> impl_;
};

}  // namespace ftxmodel

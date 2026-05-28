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
        return createIndex(static_cast<int>(i), child.column(), nullptr);
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
    const ModelIndex localIndex = index(row, 0, ModelIndex());
    const ModelIndex sourceIndex = mapToSource(localIndex);
    const auto nodeId = sourceIndex.uniqueId();
    if (impl_->expanded_nodes.insert(nodeId).second) {
      invalidate();
    }
  }

  void collapse(const int row) {
    impl_->ensureCache(this);
    if (row < 0 || row >= static_cast<int>(impl_->flat_lookup_cache.size())) {
      return;
    }
    const ModelIndex localIndex = index(row, 0, ModelIndex());
    const ModelIndex sourceIndex = mapToSource(localIndex);
    const auto nodeId = sourceIndex.uniqueId();
    if (impl_->expanded_nodes.erase(nodeId) > 0) {
      invalidate();
    }
  }

 protected:
  void invalidate() override {
    impl_->flat_lookup_cache.clear();
    headerDataChanged(0, 0);
  }

 private:
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
  };

  std::unique_ptr<Impl> impl_;
};

}  // namespace ftxmodel

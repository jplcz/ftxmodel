#pragma once
#include "abstract_proxy_model.hpp"

namespace ftxmodel {

class IdentityProxyModel : public AbstractProxyModel {
 public:
  ModelIndex index(const int row,
                   const int column,
                   const ModelIndex& parent) const override {
    if (!sourceModel()) {
      return {};
    }
    const auto sourceParent = mapToSource(parent);
    const auto sourceIndex = sourceModel()->index(row, column, sourceParent);
    return mapFromSource(sourceIndex);
  }

  ModelIndex parent(const ModelIndex& child) const override {
    const auto sourceChild = mapToSource(child);
    const auto sourceParent = sourceChild.parent();
    return mapFromSource(sourceParent);
  }

  ModelIndex mapFromSource(const ModelIndex& child) const override {
    if (!sourceModel() || !child.isValid()) {
      return {};
    }
    return createIndex(child.row(), child.column(), child.internalPointer());
  }

  ModelIndex mapToSource(const ModelIndex& child) const override {
    if (!sourceModel() || !child.isValid()) {
      return {};
    }
    return createSourceIndex(child.row(), child.column(),
                             child.internalPointer());
  }

 protected:
  void invalidate() override {}
};

}  // namespace ftxmodel

#pragma once
#include <cassert>
#include "abstract_item_model.hpp"

namespace ftxmodel {

class AbstractProxyModel : public AbstractItemModel {
  std::shared_ptr<AbstractItemModel> m_source;
  sigslot::scoped_connection m_header_changed_conn;
  sigslot::scoped_connection m_data_changed_conn;

 public:
  AbstractItemModel* sourceModel() const { return m_source.get(); }

  void setSourceModel(const std::shared_ptr<AbstractItemModel>& sourceModel) {
    m_header_changed_conn.disconnect();
    m_data_changed_conn.disconnect();
    m_source = sourceModel;
    invalidate();
    if (m_source) {
      m_header_changed_conn = sourceModel->headerDataChanged.connect(
          &AbstractProxyModel::sourceHeaderDataChanged, this);
      m_data_changed_conn = sourceModel->dataChanged.connect(
          &AbstractProxyModel::sourceDataChanged, this);
    }
  }

  int rowCount(const ModelIndex& parent = ModelIndex()) const override {
    return m_source ? m_source->rowCount(mapToSource(parent)) : 0;
  }

  int columnCount(const ModelIndex& parent = ModelIndex()) const override {
    return m_source ? m_source->columnCount(mapToSource(parent)) : 0;
  }

  bool hasChildren(const ModelIndex& parent) const override {
    return m_source ? m_source->hasChildren(mapToSource(parent)) : false;
  }

  std::any data(const ModelIndex& index,
                ItemRole role = ItemRole::DisplayRole) const override {
    return m_source ? m_source->data(mapToSource(index), role) : std::any();
  }

  bool setData(const ModelIndex& index,
               const std::any& value,
               ItemRole role) override {
    return m_source ? m_source->setData(mapToSource(index), value, role)
                    : false;
  }

  std::any headerData(int section,
                      Orientation orientation,
                      ItemRole role) const override {
    return m_source ? m_source->headerData(section, orientation, role)
                    : std::any();
  }

  bool setHeaderData(int section,
                     Orientation orientation,
                     const std::any& value,
                     ItemRole role) override {
    return m_source ? m_source->setHeaderData(section, orientation, value, role)
                    : false;
  }

  ItemFlags flags(const ModelIndex& index) const override {
    return m_source ? m_source->flags(mapToSource(index)) : NoItemFlags;
  }

  UniqueNodeId uniqueId(const ModelIndex& index) const override {
    return m_source ? m_source->uniqueId(mapToSource(index)) : UniqueNodeId();
  }

  ModelIndex findIndexById(
      const UniqueNodeId& targetId,
      const ModelIndex& parent = ModelIndex()) const override {
    const auto sourceParent = mapToSource(parent);
    const auto result = m_source
                            ? m_source->findIndexById(targetId, sourceParent)
                            : ModelIndex();
    return mapFromSource(result);
  }

  virtual ModelIndex mapFromSource(const ModelIndex& child) const = 0;
  virtual ModelIndex mapToSource(const ModelIndex& child) const = 0;

 protected:
  virtual void invalidate() = 0;

  ModelIndex createSourceIndex(int row, int col, void* internalPtr) const {
    assert(m_source != nullptr &&
           "Cannot create source index when source model is null!");
    if (!m_source) {
      return ModelIndex();  // Safe production fallback boundary
    }
    return ModelIndex(row, col, internalPtr, m_source.get());
  }

  void sourceHeaderDataChanged(int section, int role) {
    headerDataChanged(section, role);
  }

  void sourceDataChanged(const ModelIndex& topLeft,
                         const ModelIndex& bottomRight) {
    invalidate();
    dataChanged(mapFromSource(topLeft), mapFromSource(bottomRight));
  }
};

}  // namespace ftxmodel

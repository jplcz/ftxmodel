#pragma once
#include <cassert>
#include "abstract_item_model.hpp"

namespace ftxmodel {

/**
 * @class AbstractProxyModel
 * @brief Base class for intermediate data models that sort, filter, or
 * transform a source model.
 * AbstractProxyModel implements the standard @ref AbstractItemModel interface
 * contract while acting as a translation proxy for an underlying source
 * dataset. It establishes an observer relationship with the source model,
 * handles index mappings bi-directionally via @ref mapToSource() and
 * @ref mapFromSource(), and sanitizes and forwards structural alteration
 * signals cleanly to attached views.
 * Subclasses must override the coordinate mapping abstractions alongside the
 * local cache management strategy:
 * - @ref mapFromSource()
 * - @ref mapToSource()
 * - @ref invalidate()
 * - @ref index
 * - @ref parent
 * * @note All lifecycle events emitted by the source model (insertions,
 * deletions, resets) are caught, translated into proxy-relative dimensions, and
 * re-emitted to protect the structural integrity of the views.
 */
class AbstractProxyModel : public AbstractItemModel {
  std::shared_ptr<AbstractItemModel> m_source;

  // Dedicated scoped connection trackers to handle dynamic attachment lifetimes
  std::vector<sigslot::scoped_connection> m_connections;

 public:
  /**
   * @brief Accesses the native, un-proxied backend data model authority
   * instance.
   * @return AbstractItemModel* Pointer to the underlying model, or nullptr if
   * none is bound.
   */
  AbstractItemModel* sourceModel() const { return m_source.get(); }

  /**
   * @brief Binds a fresh source data model backend to this translation proxy
   * layer.
   * This method breaks any historical sigslot configurations, maps state
   * caches back to default parameters, and builds a complete structural signal
   * pipeline to catch downstream mutations.
   * @param[in] sourceModel The shared pointer reference to the incoming model
   * authority.
   */
  void setSourceModel(const std::shared_ptr<AbstractItemModel>& sourceModel) {
    m_connections.clear();
    m_source = sourceModel;
    // Wire the complete signal interception and forwarding grid
    if (m_source) {
      m_connections.reserve(12);

      m_connections.emplace_back(m_source->headerDataChanged.connect(
          &AbstractProxyModel::slotSourceHeaderDataChanged, this));
      m_connections.emplace_back(m_source->dataChanged.connect(
          &AbstractProxyModel::slotSourceDataChanged, this));

      // Row Structural Channel Assignments
      m_connections.emplace_back(m_source->beginInsertRows.connect(
          &AbstractProxyModel::slotSourceBeginInsertRows, this));
      m_connections.emplace_back(m_source->endInsertRows.connect(
          &AbstractProxyModel::slotSourceEndInsertRows, this));
      m_connections.emplace_back(m_source->beginRemoveRows.connect(
          &AbstractProxyModel::slotSourceBeginRemoveRows, this));
      m_connections.emplace_back(m_source->endRemoveRows.connect(
          &AbstractProxyModel::slotSourceEndRemoveRows, this));

      // Column Structural Channel Assignments
      m_connections.emplace_back(m_source->beginInsertColumns.connect(
          &AbstractProxyModel::slotSourceBeginInsertColumns, this));
      m_connections.emplace_back(m_source->endInsertColumns.connect(
          &AbstractProxyModel::slotSourceEndInsertColumns, this));
      m_connections.emplace_back(m_source->beginRemoveColumns.connect(
          &AbstractProxyModel::slotSourceBeginRemoveColumns, this));
      m_connections.emplace_back(m_source->endRemoveColumns.connect(
          &AbstractProxyModel::slotSourceEndRemoveColumns, this));

      // Master Reset Assignments
      m_connections.emplace_back(m_source->beginResetModel.connect(
          &AbstractProxyModel::slotSourceBeginResetModel, this));
      m_connections.emplace_back(m_source->endResetModel.connect(
          &AbstractProxyModel::slotSourceEndResetModel, this));
    }
    onSourceBeginResetModel();
    invalidate();
    onSourceEndResetModel();
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

  /**
   * @brief Translates a multi-dimensional source ModelIndex handle into a
   * proxy-relative coordinate index.
   * @param[in] child The valid source coordinate index.
   * @return ModelIndex The translated coordinate tracking index, or an invalid
   * entry if mapping rules fail.
   */
  virtual ModelIndex mapFromSource(const ModelIndex& child) const = 0;

  /**
   * @brief Translates a localized proxy ModelIndex coordinate layout map back
   * into its native source index handle.
   * @param[in] child The active proxy view coordinate index handle.
   * @return ModelIndex The original un-proxied coordinate mapping source
   * handle.
   */
  virtual ModelIndex mapToSource(const ModelIndex& child) const = 0;

  bool canFetchMore(const ModelIndex& proxyParent) const override {
    if (!sourceModel()) {
      return false;
    }

    ModelIndex sourceParent = mapToSource(proxyParent);

    // Forward the contract check straight down the pipeline
    return sourceModel()->canFetchMore(sourceParent);
  }

  void fetchMore(const ModelIndex& proxyParent) override {
    if (!sourceModel()) {
      return;
    }

    ModelIndex sourceParent = mapToSource(proxyParent);

    // Command the source engine to execute its remote API/DB pull string
    sourceModel()->fetchMore(sourceParent);
  }

 protected:
  /**
   * @brief Abstract method to clear out internal transformation memory tables,
   * lookup lists, or indices.
   */
  virtual void invalidate() = 0;

  /**
   * @brief Utility for safely configuring standard source indices manually
   * inside custom proxy implementations.
   */
  ModelIndex createSourceIndex(int row, int col, void* internalPtr) const {
    assert(m_source != nullptr &&
           "Cannot create source index when source model is null!");
    if (!m_source) {
      return {};  // Safe production fallback boundary
    }
    return {row, col, internalPtr, m_source.get()};
  }

  // =========================================================================
  // Protected Virtual Handlers (Overridden by Subclasses)
  // =========================================================================
  virtual void onSourceHeaderDataChanged(int section, int role) {
    this->headerDataChanged(section, role);
  }

  virtual void onSourceDataChanged(const ModelIndex& topLeft,
                                   const ModelIndex& bottomRight) {
    this->dataChanged(mapFromSource(topLeft), mapFromSource(bottomRight));
  }

  virtual void onSourceBeginInsertRows(const ModelIndex& parent,
                                       int start,
                                       int end) {
    this->beginInsertRows(mapFromSource(parent), start, end);
  }

  virtual void onSourceEndInsertRows() { this->endInsertRows(); }

  virtual void onSourceBeginRemoveRows(const ModelIndex& parent,
                                       int start,
                                       int end) {
    this->beginRemoveRows(mapFromSource(parent), start, end);
  }

  virtual void onSourceEndRemoveRows() { this->endRemoveRows(); }

  virtual void onSourceBeginInsertColumns(const ModelIndex& parent,
                                          int start,
                                          int end) {
    this->beginInsertColumns(mapFromSource(parent), start, end);
  }

  virtual void onSourceEndInsertColumns() { this->endInsertColumns(); }

  virtual void onSourceBeginRemoveColumns(const ModelIndex& parent,
                                          int start,
                                          int end) {
    this->beginRemoveColumns(mapFromSource(parent), start, end);
  }

  virtual void onSourceEndRemoveColumns() { this->endRemoveColumns(); }

  virtual void onSourceBeginResetModel() { this->beginResetModel(); }

  virtual void onSourceEndResetModel() { this->endResetModel(); }

 private:
  // =========================================================================
  // Private Non-Virtual Wrapper Slots (Vtable Trampolines + Invariant Guards)
  // =========================================================================
  void slotSourceHeaderDataChanged(int section, int role) {
    onSourceHeaderDataChanged(section, role);  // Safe virtual dispatch
  }

  void slotSourceDataChanged(const ModelIndex& topLeft,
                             const ModelIndex& bottomRight) {
    invalidate();
    onSourceDataChanged(topLeft, bottomRight);
  }

  void slotSourceBeginInsertRows(const ModelIndex& parent, int start, int end) {
    onSourceBeginInsertRows(parent, start, end);
  }

  void slotSourceEndInsertRows() {
    invalidate();
    onSourceEndInsertRows();
  }

  void slotSourceBeginRemoveRows(const ModelIndex& parent, int start, int end) {
    onSourceBeginRemoveRows(parent, start, end);
  }

  void slotSourceEndRemoveRows() {
    invalidate();
    onSourceEndRemoveRows();
  }

  void slotSourceBeginInsertColumns(const ModelIndex& parent,
                                    int start,
                                    int end) {
    onSourceBeginInsertColumns(parent, start, end);
  }

  void slotSourceEndInsertColumns() {
    invalidate();
    onSourceEndInsertColumns();
  }

  void slotSourceBeginRemoveColumns(const ModelIndex& parent,
                                    int start,
                                    int end) {
    onSourceBeginRemoveColumns(parent, start, end);
  }

  void slotSourceEndRemoveColumns() {
    invalidate();
    onSourceEndRemoveColumns();
  }

  void slotSourceBeginResetModel() { onSourceBeginResetModel(); }

  void slotSourceEndResetModel() {
    invalidate();
    onSourceEndResetModel();
  }
};

}  // namespace ftxmodel

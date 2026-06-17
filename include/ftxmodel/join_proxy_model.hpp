#pragma once
#include <memory>
#include <nlohmann/detail/meta/type_traits.hpp>

#include "abstract_item_model.hpp"

namespace ftxmodel {

/**
 * Join one or more models horizontally and vertically.
 * Only grid like models (table or list) are supported, otherwise
 * code complexity required would be enormous.
 */
class JoinProxyModel : public AbstractItemModel {
 public:
  JoinProxyModel();

  ~JoinProxyModel() override;

  Orientation joinOrientation() const noexcept;
  void setJoinOrientation(Orientation newOrientation) noexcept;

  ModelIndex index(int row,
                   int column,
                   const ModelIndex& parent = ModelIndex()) const override;
  ModelIndex parent(const ModelIndex& child) const override;
  int rowCount(const ModelIndex& parent = ModelIndex()) const override;
  int columnCount(const ModelIndex& parent = ModelIndex()) const override;
  bool hasChildren(const ModelIndex& parent = ModelIndex()) const override;
  std::any data(const ModelIndex& index,
                ItemRole role = ItemRole::DisplayRole) const override;
  bool setData(const ModelIndex& index,
               const std::any& value,
               ItemRole role = ItemRole::EditRole) override;
  std::any headerData(int section,
                      Orientation orientation,
                      ItemRole role) const override;
  bool setHeaderData(int section,
                     Orientation orientation,
                     const std::any& value,
                     ItemRole role) override;
  ItemFlags flags(const ModelIndex& index) const override;
  UniqueNodeId uniqueId(const ModelIndex& index) const override;
  ModelIndex findIndexById(
      const UniqueNodeId& targetId,
      const ModelIndex& parent = ModelIndex()) const override;
  bool canFetchMore(const ModelIndex& parent) const override;
  void fetchMore(const ModelIndex& parent) override;

  void addSourceModel(const std::shared_ptr<AbstractItemModel>& model);
  void clearModels();

 private:
  ModelIndex mapToSource(const ModelIndex& proxyIndex) const;
  ModelIndex mapFromSource(const ModelIndex& sourceIndex) const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

struct JoinProxyModel::Impl {
  std::vector<std::shared_ptr<AbstractItemModel>> m_models;
  Orientation m_orientation = Orientation::Horizontal;
  std::vector<sigslot::scoped_connection> m_connections;
  JoinProxyModel* self;

  explicit Impl(JoinProxyModel* q) : self(q) {}

  void invalidate();
  void rebuild();

  int fromSourceRow(const AbstractItemModel* sourceModel, int row) const;
  int fromSourceColumn(const AbstractItemModel* sourceModel, int column) const;
  std::pair<AbstractItemModel*, int> toSourceRow(int row) const;
  std::pair<AbstractItemModel*, int> toSourceColumn(int column) const;

  void slotSourceHeaderDataChanged(const AbstractItemModel* source,
                                   int section,
                                   int role) {
    if (m_orientation == Orientation::Vertical) {
      return;
    }
    const int proxy_section = fromSourceColumn(source, section);
    self->headerDataChanged(proxy_section, role);
  }

  void slotSourceDataChanged(const ModelIndex& topLeft,
                             const ModelIndex& bottomRight) {
    const auto proxyTL = self->mapToSource(topLeft);
    const auto proxyBR = self->mapToSource(bottomRight);
    self->dataChanged(proxyTL, proxyBR);
  }

  void slotSourceBeginInsertRows(const AbstractItemModel* source,
                                 const ModelIndex& parent,
                                 int start,
                                 int end) {
    if (parent.isValid()) {
      return;
    }
    if (m_orientation == Orientation::Horizontal) {
      // Calculate max and notify only if we'd change height
      const int height = self->rowCount(ModelIndex());
      if (end >= height) {
        self->beginInsertRows(ModelIndex(), std::max(height, start),
                              std::max(height, end));
      }
    } else {
      // We always notify
      const int proxyStart = fromSourceRow(source, start);
      const int proxyEnd = fromSourceRow(source, end);
      self->beginInsertRows(ModelIndex(), proxyStart, proxyEnd);
    }
  }

  void slotSourceEndInsertRows() { self->endInsertRows(); }

  void slotSourceBeginRemoveRows(const AbstractItemModel*,
                                 const ModelIndex&,
                                 int,
                                 int) {
    self->beginResetModel();
  }

  void slotSourceEndRemoveRows() { self->endResetModel(); }

  void slotSourceBeginInsertColumns(const AbstractItemModel* source,
                                    const ModelIndex& parent,
                                    int start,
                                    int end) {
    if (parent.isValid()) {
      return;
    }
    if (m_orientation == Orientation::Vertical) {
      // Calculate max and notify only if we'd change height
      const int width = self->columnCount(ModelIndex());
      if (end >= width) {
        self->beginInsertColumns(ModelIndex(), std::max(width, start),
                                 std::max(width, end));
      }
    } else {
      // We always notify
      const int proxyStart = fromSourceColumn(source, start);
      const int proxyEnd = fromSourceColumn(source, end);
      self->beginInsertColumns(ModelIndex(), proxyStart, proxyEnd);
    }
  }

  void slotSourceEndInsertColumns() { self->endInsertColumns(); }

  void slotSourceBeginRemoveColumns(const AbstractItemModel*,
                                    const ModelIndex&,
                                    int,
                                    int) {
    self->beginResetModel();
  }

  void slotSourceEndRemoveColumns() { self->endResetModel(); }

  void slotSourceBeginResetModel() { self->beginResetModel(); }

  void slotSourceEndResetModel() { self->endResetModel(); }
};

inline JoinProxyModel::JoinProxyModel() : impl_(std::make_unique<Impl>(this)) {}

inline JoinProxyModel::~JoinProxyModel() = default;

inline Orientation JoinProxyModel::joinOrientation() const noexcept {
  return impl_->m_orientation;
}

inline void JoinProxyModel::setJoinOrientation(
    const Orientation newOrientation) noexcept {
  if (impl_->m_orientation != newOrientation) {
    beginResetModel();
    impl_->invalidate();
    impl_->m_orientation = newOrientation;
    endResetModel();
  }
}

inline ModelIndex JoinProxyModel::index(int row,
                                        int column,
                                        const ModelIndex& parent) const {
  if (parent.isValid() || row < 0 || column < 0 || row >= rowCount() ||
      column >= columnCount()) {
    return {};
  }
  impl_->rebuild();
  const int orig_row = row;
  const int orig_col = column;

  if (impl_->m_orientation == Orientation::Horizontal) {
    // Find column
    for (const auto& m : impl_->m_models) {
      const int columns = m->columnCount();
      if (column < columns) {
        const auto sourceIndex = m->index(row, column);
        if (sourceIndex.isValid()) {
          return createIndex(orig_row, orig_col, sourceIndex.internalPointer());
        } else {
          return createIndex(orig_row, orig_col, nullptr);
        }
      }
      column -= columns;
    }
  } else {
    // Find row
    for (const auto& m : impl_->m_models) {
      const int rows = m->rowCount();
      if (row < rows) {
        const auto sourceIndex = m->index(row, column);
        if (sourceIndex.isValid()) {
          return createIndex(orig_row, orig_col, sourceIndex.internalPointer());
        } else {
          return createIndex(orig_row, orig_col, nullptr);
        }
      }
      row -= rows;
    }
  }
  return {};
}

inline ModelIndex JoinProxyModel::parent(const ModelIndex&) const {
  return {};
}

inline int JoinProxyModel::rowCount(const ModelIndex& parent) const {
  if (parent.isValid()) {
    return 0;
  }
  impl_->rebuild();
  int result = 0;
  if (impl_->m_orientation == Orientation::Horizontal) {
    // Take max of all row counts. Models are joined horizontally
    for (const auto& m : impl_->m_models) {
      result = std::max(result, m->rowCount(parent));
    }
  } else {
    // Take sum of all row counts. Models are joined vertically
    for (const auto& m : impl_->m_models) {
      result += m->rowCount(parent);
    }
  }
  return result;
}

inline int JoinProxyModel::columnCount(const ModelIndex& parent) const {
  if (parent.isValid()) {
    return 0;
  }
  impl_->rebuild();
  int result = 0;
  if (impl_->m_orientation == Orientation::Horizontal) {
    for (const auto& m : impl_->m_models) {
      result += m->columnCount(parent);
    }
  } else {
    for (const auto& m : impl_->m_models) {
      result = std::max(result, m->columnCount(parent));
    }
  }
  return result;
}

inline bool JoinProxyModel::hasChildren(const ModelIndex& parent) const {
  if (parent.isValid() || impl_->m_models.empty()) {
    return false;
  }
  for (const auto& m : impl_->m_models) {
    if (m->hasChildren(parent)) {
      return true;
    }
  }
  return false;
}

inline std::any JoinProxyModel::data(const ModelIndex& index,
                                     ItemRole role) const {
  if (!index.isValid()) {
    return {};
  }
  const auto sourceIndex = mapToSource(index);
  return sourceIndex.data(role);
}

inline bool JoinProxyModel::setData(const ModelIndex& index,
                                    const std::any& value,
                                    ItemRole role) {
  const auto sourceIndex = mapToSource(index);
  if (!sourceIndex.isValid()) {
    return false;
  }
  return const_cast<AbstractItemModel*>(sourceIndex.model())
      ->setData(sourceIndex, value);
}

inline std::any JoinProxyModel::headerData(int section,
                                           Orientation orientation,
                                           ItemRole role) const {
  impl_->rebuild();
  if (impl_->m_orientation == Orientation::Horizontal) {
    if (orientation == Orientation::Horizontal) {
      for (const auto& m : impl_->m_models) {
        const int columns = m->columnCount();
        if (section < columns) {
          return m->headerData(section, orientation, role);
        }
        section -= columns;
      }
    }
  } else {
    if (orientation == Orientation::Vertical) {
      for (const auto& m : impl_->m_models) {
        const int rows = m->rowCount();
        if (section < rows) {
          return m->headerData(section, orientation, role);
        }
        section -= rows;
      }
    }
  }
  return {};
}

inline bool JoinProxyModel::setHeaderData(int section,
                                          Orientation orientation,
                                          const std::any& value,
                                          ItemRole role) {
  impl_->rebuild();
  if (impl_->m_orientation == Orientation::Horizontal) {
    if (orientation == Orientation::Horizontal) {
      for (const auto& m : impl_->m_models) {
        const int columns = m->columnCount();
        if (section < columns) {
          return m->setHeaderData(section, orientation, value, role);
        }
        section -= columns;
      }
    }
  } else {
    if (orientation == Orientation::Vertical) {
      for (const auto& m : impl_->m_models) {
        const int rows = m->rowCount();
        if (section < rows) {
          return m->setHeaderData(section, orientation, value, role);
        }
        section -= rows;
      }
    }
  }
  return {};
}

inline ItemFlags JoinProxyModel::flags(const ModelIndex& index) const {
  return mapToSource(index).flags();
}

inline UniqueNodeId JoinProxyModel::uniqueId(const ModelIndex& index) const {
  return mapToSource(index).uniqueId();
}

inline ModelIndex JoinProxyModel::findIndexById(
    const UniqueNodeId& targetId,
    const ModelIndex& parent) const {
  if (parent.isValid()) {
    return {};
  }

  for (const auto& m : impl_->m_models) {
    if (const auto idx = m->findIndexById(targetId); idx.isValid()) {
      return mapFromSource(idx);
    }
  }
  return {};
}

inline bool JoinProxyModel::canFetchMore(const ModelIndex& parent) const {
  const auto sourceParent = mapToSource(parent);
  if (sourceParent.isValid()) {
    return false;
  }
  for (const auto& m : impl_->m_models) {
    if (m->canFetchMore(sourceParent)) {
      return true;
    }
  }
  return false;
}

inline void JoinProxyModel::fetchMore(const ModelIndex& parent) {
  if (parent.isValid()) {
    return;
  }
  for (const auto& m : impl_->m_models) {
    m->fetchMore(parent);
  }
}

inline void JoinProxyModel::addSourceModel(
    const std::shared_ptr<AbstractItemModel>& model) {
  beginResetModel();
  impl_->m_models.emplace_back(model);

  impl_->m_connections.emplace_back(model->beginResetModel.connect(
      &Impl::slotSourceBeginResetModel, impl_.get()));

  impl_->m_connections.emplace_back(model->endResetModel.connect(
      &Impl::slotSourceEndResetModel, impl_.get()));

  impl_->m_connections.emplace_back(
      model->dataChanged.connect(&Impl::slotSourceDataChanged, impl_.get()));

  impl_->m_connections.emplace_back(model->headerDataChanged.connect(
      [&, m_ref = model.get()](const int section, const int role) {
        impl_->slotSourceHeaderDataChanged(m_ref, section, role);
      }));

  impl_->m_connections.emplace_back(model->beginInsertRows.connect(
      [&, m_ref = model.get()](const ModelIndex& parent, int s, int e) {
        impl_->slotSourceBeginInsertRows(m_ref, parent, s, e);
      }));

  impl_->m_connections.emplace_back(model->endInsertRows.connect(
      &Impl::slotSourceEndInsertRows, impl_.get()));

  impl_->m_connections.emplace_back(model->beginRemoveRows.connect(
      [&, m_ref = model.get()](const ModelIndex& parent, int s, int e) {
        impl_->slotSourceBeginRemoveRows(m_ref, parent, s, e);
      }));

  impl_->m_connections.emplace_back(model->endRemoveRows.connect(
      &Impl::slotSourceEndRemoveRows, impl_.get()));

  impl_->m_connections.emplace_back(model->beginInsertColumns.connect(
      [&, m_ref = model.get()](const ModelIndex& parent, int s, int e) {
        impl_->slotSourceBeginInsertColumns(m_ref, parent, s, e);
      }));

  impl_->m_connections.emplace_back(model->endInsertColumns.connect(
      &Impl::slotSourceEndInsertColumns, impl_.get()));

  impl_->m_connections.emplace_back(model->beginRemoveColumns.connect(
      [&, m_ref = model.get()](const ModelIndex& parent, int s, int e) {
        impl_->slotSourceBeginRemoveColumns(m_ref, parent, s, e);
      }));

  impl_->m_connections.emplace_back(model->endRemoveColumns.connect(
      &Impl::slotSourceEndRemoveColumns, impl_.get()));

  endResetModel();
}

inline void JoinProxyModel::clearModels() {
  beginResetModel();
  impl_->m_connections.clear();
  impl_->m_models.clear();
  endResetModel();
}

inline ModelIndex JoinProxyModel::mapToSource(
    const ModelIndex& proxyIndex) const {
  if (!proxyIndex.isValid()) {
    return {};
  }
  int row = proxyIndex.row();
  int column = proxyIndex.column();

  impl_->rebuild();

  if (impl_->m_orientation == Orientation::Horizontal) {
    // Find column
    for (const auto& m : impl_->m_models) {
      const int columns = m->columnCount();
      if (column < columns) {
        return m->index(row, column);
      }
      column -= columns;
    }
  } else {
    // Find row
    for (const auto& m : impl_->m_models) {
      const int rows = m->rowCount();
      if (row < rows) {
        return m->index(row, column);
      }
      row -= rows;
    }
  }
  return {};
}

inline ModelIndex JoinProxyModel::mapFromSource(
    const ModelIndex& sourceIndex) const {
  if (!sourceIndex.isValid()) {
    return {};
  }

  int row = sourceIndex.row();
  int column = sourceIndex.column();
  void* internalPtr = sourceIndex.internalPointer();

  impl_->rebuild();

  if (impl_->m_orientation == Orientation::Horizontal) {
    // Adjust column
    for (const auto& m : impl_->m_models) {
      if (m.get() == sourceIndex.model()) {
        break;
      }
      column += m->columnCount();
    }
  } else {
    for (const auto& m : impl_->m_models) {
      if (m.get() == sourceIndex.model()) {
        break;
      }
      row += m->rowCount();
    }
  }
  return createIndex(row, column, internalPtr);
}

inline void JoinProxyModel::Impl::invalidate() {}

inline void JoinProxyModel::Impl::rebuild() {}

inline int JoinProxyModel::Impl::fromSourceRow(
    const AbstractItemModel* sourceModel,
    int row) const {
  if (m_orientation == Orientation::Horizontal) {
    // Row identity is preserved
    return row;
  }
  for (const auto& m : m_models) {
    if (m.get() == sourceModel) {
      return row;
    }
    row += m->rowCount();
  }
  return -1;
}

inline int JoinProxyModel::Impl::fromSourceColumn(
    const AbstractItemModel* sourceModel,
    int column) const {
  if (m_orientation == Orientation::Vertical) {
    // Column identity is preserved
    return column;
  }
  for (const auto& m : m_models) {
    if (m.get() == sourceModel) {
      return column;
    }
    column += m->columnCount();
  }
  return -1;
}

inline std::pair<AbstractItemModel*, int> JoinProxyModel::Impl::toSourceRow(
    int row) const {
  if (m_orientation == Orientation::Horizontal) {
    return {nullptr, -1};
  }
  for (const auto& m : m_models) {
    const int rows = m->rowCount();
    if (row < rows) {
      return {m.get(), row};
    }
    row -= rows;
  }
  return {nullptr, -1};
}

inline std::pair<AbstractItemModel*, int> JoinProxyModel::Impl::toSourceColumn(
    int column) const {
  if (m_orientation == Orientation::Vertical) {
    return {nullptr, -1};
  }
  for (const auto& m : m_models) {
    const int columns = m->columnCount();
    if (column < columns) {
      return {m.get(), column};
    }
    column -= columns;
  }
  return {nullptr, -1};
}

}  // namespace ftxmodel

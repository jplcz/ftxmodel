#pragma once
#include <ftxmodel/abstract_item_model.hpp>

namespace ftxmodel {

class ConcatProxyModel : public AbstractItemModel {
  std::vector<std::shared_ptr<AbstractItemModel>> m_models;
  Orientation m_orientation = Orientation::Horizontal;
  std::vector<sigslot::scoped_connection> m_connections;

 public:
  ConcatProxyModel() = default;
  ~ConcatProxyModel() override = default;

  void setSourceModels(
      std::vector<std::shared_ptr<AbstractItemModel>> models,
      const Orientation orientation = Orientation::Horizontal) {
    m_connections.clear();
    m_models = models;
    m_orientation = orientation;

    for (const auto& model : m_models) {
      connectModel(model.get());
    }

    this->beginResetModel();
    this->endResetModel();
  }

  ModelIndex index(const int local_row,
                   const int local_column,
                   const ModelIndex& parent = ModelIndex()) const override {
    if (parent.isValid() || local_row < 0 || local_column < 0 ||
        local_row >= rowCount(parent) || local_column >= columnCount(parent)) {
      return {};
    }
    if (m_orientation == Orientation::Horizontal) {
      int column = local_column;
      for (const auto& model : m_models) {
        const int columns = model->columnCount();
        if (column < columns) {
          const ModelIndex src_idx = model->index(local_row, column);
          if (!src_idx.isValid()) {
            return {};
          }
          return createIndex(local_row, local_column,
                             src_idx.internalPointer());
        }
        column -= columns;
      }
    } else {
      int row = local_row;
      for (const auto& model : m_models) {
        const int rows = model->rowCount();
        if (row < rows) {
          const ModelIndex src_idx = model->index(row, local_column);
          if (!src_idx.isValid()) {
            return {};
          }
          return createIndex(local_row, local_column,
                             src_idx.internalPointer());
        }
        row -= rows;
      }
    }
    return {};
  }

  ModelIndex parent(const ModelIndex&) const override {
    // This is flat structure
    return {};
  }

  int rowCount(const ModelIndex& parent = ModelIndex()) const override {
    if (parent.isValid()) {
      return 0;
    }
    // For horizontal stitch we take max rows of all models
    int value = 0;
    if (m_orientation == Orientation::Horizontal) {
      for (const auto& model : m_models) {
        value = std::max(value, model->rowCount());
      }
    } else {
      // For vertical stitch we take sum of rows
      for (const auto& model : m_models) {
        value += model->rowCount();
      }
    }
    return value;
  }

  int columnCount(const ModelIndex& parent = ModelIndex()) const override {
    if (parent.isValid()) {
      return 0;
    }
    int value = 0;
    if (m_orientation == Orientation::Horizontal) {
      // For horizontal stitch, we take sum of all columns
      for (const auto& model : m_models) {
        value += model->columnCount();
      }
    } else {
      // For vertical stitch, we take max of all columns
      for (const auto& model : m_models) {
        value = std::max(value, model->columnCount());
      }
    }
    return value;
  }

  bool hasChildren(const ModelIndex& parent = ModelIndex()) const override {
    if (parent.isValid()) {
      return false;
    }
    for (const auto& model : m_models) {
      if (model->hasChildren()) {
        return true;
      }
    }
    return false;
  }

  std::any data(const ModelIndex& index,
                ItemRole role = ItemRole::DisplayRole) const override {
    const auto child = mapFromProxy(index);
    return child.first.isValid() ? child.second->data(child.first, role)
                                 : std::any();
  }

  bool setData(const ModelIndex& index,
               const std::any& value,
               ItemRole role = ItemRole::EditRole) override {
    const auto child = mapFromProxy(index);
    return child.first.isValid()
               ? child.second->setData(child.first, value, role)
               : false;
  }

  ItemFlags flags(const ModelIndex& index) const override {
    const auto child = mapFromProxy(index);
    return child.first.isValid() ? child.second->flags(child.first)
                                 : ItemFlag::NoItemFlags;
  }

  UniqueNodeId uniqueId(const ModelIndex& index) const override {
    const auto child = mapFromProxy(index);
    if (!child.first.isValid()) {
      return {};
    }
    return child.second->uniqueId(child.first);
  }

  ModelIndex findIndexById(
      const UniqueNodeId& targetId,
      const ModelIndex& parent = ModelIndex()) const override {
    if (parent.isValid()) {
      return {};
    }
    for (const auto& model : m_models) {
      if (const auto index = model->findIndexById(targetId); index.isValid()) {
        return mapFromSource(index);
      }
    }
    return {};
  }

  std::pair<ModelIndex, AbstractItemModel*> mapFromProxy(
      const ModelIndex& proxy_index) const {
    if (!proxy_index.isValid()) {
      return {};
    }
    if (m_orientation == Orientation::Horizontal) {
      int column = proxy_index.column();
      for (const auto& model : m_models) {
        const int columns = model->columnCount();
        if (column < columns) {
          return {model->index(proxy_index.row(), column), model.get()};
        }
        column -= columns;
      }
    } else {
      int row = proxy_index.row();
      for (const auto& model : m_models) {
        const int rows = model->rowCount();
        if (row < rows) {
          return {model->index(row, proxy_index.column()), model.get()};
        }
        row -= rows;
      }
    }
    return {};
  }

  ModelIndex mapFromSource(const ModelIndex& proxy_index) const {
    if (!proxy_index.isValid()) {
      return {};
    }
    const auto* model_p = proxy_index.model();
    int row = proxy_index.row();
    int column = proxy_index.column();

    if (m_orientation == Orientation::Horizontal) {
      // Adjust column index
      for (const auto& model : m_models) {
        if (model.get() == model_p) {
          break;
        }
        column += model->columnCount();
      }
    } else {
      for (const auto& model : m_models) {
        if (model.get() == model_p) {
          break;
        }
        row += model->rowCount();
      }
    }

    return createIndex(row, column, proxy_index.internalPointer());
  }

  std::any headerData(int section,
                      Orientation orientation,
                      ItemRole role = ItemRole::DisplayRole) const override {
    if (section < 0) {
      return std::any();
    }

    if (m_orientation == Orientation::Horizontal) {
      if (orientation == Orientation::Horizontal) {
        // --- Axis Match: Stitch column headers together sequentially ---
        int column = section;
        for (const auto& model : m_models) {
          if (!model) {
            continue;
          }
          const int columns = model->columnCount();
          if (column < columns) {
            return model->headerData(column, orientation, role);
          }
          column -= columns;
        }
      } else {
        // --- Cross Axis: Generic vertical row index indicators ("1", "2", "3")
        // ---
        if (section < rowCount()) {
          return std::to_string(section + 1);
        }
      }
    } else {  // Orientation::Vertical
      if (orientation == Orientation::Vertical) {
        // --- Axis Match: Stitch row headers together sequentially ---
        int row = section;
        for (const auto& model : m_models) {
          if (!model) {
            continue;
          }
          const int rows = model->rowCount();
          if (row < rows) {
            return model->headerData(row, orientation, role);
          }
          row -= rows;
        }
      } else {
        // --- Cross Axis: Forward horizontal headers from the first valid
        // model,
        //                 or fallback to a generic column label string. ---
        for (const auto& model : m_models) {
          if (model && section < model->columnCount()) {
            return model->headerData(section, orientation, role);
          }
        }
        if (section < columnCount()) {
          return std::format("Column {}", section);
        }
      }
    }

    return std::any();
  }

 private:
  void connectModel(AbstractItemModel* model) {
    m_connections.emplace_back(model->dataChanged.connect(
        [this](const ModelIndex& tl, const ModelIndex& br) {
          this->dataChanged(mapFromSource(tl), mapFromSource(br));
        }));

    m_connections.emplace_back(model->beginInsertRows.connect(
        [this, model](const ModelIndex&, int start, int end) {
          const auto [row_offset, col_offset] = findModelOffset(model, 0, 0);

          this->beginInsertRows(ModelIndex(), start + row_offset,
                                end + row_offset);
        }));
    m_connections.emplace_back(
        model->endInsertRows.connect([this]() { this->endInsertRows(); }));

    m_connections.emplace_back(model->beginInsertColumns.connect(
        [this, model](const ModelIndex&, int start, int end) {
          const auto [row_offset, col_offset] = findModelOffset(model, 0, 0);

          this->beginInsertColumns(ModelIndex(), start + col_offset,
                                   end + col_offset);
        }));
    m_connections.emplace_back(model->endInsertColumns.connect(
        [this]() { this->endInsertColumns(); }));

    m_connections.emplace_back(model->beginRemoveRows.connect(
        [this, model](const ModelIndex&, int start, int end) {
          const auto [row_offset, col_offset] = findModelOffset(model, 0, 0);

          this->beginRemoveRows(ModelIndex(), start + row_offset,
                                end + row_offset);
        }));
    m_connections.emplace_back(
        model->endRemoveRows.connect([this]() { this->endRemoveRows(); }));

    m_connections.emplace_back(model->beginRemoveColumns.connect(
        [this, model](const ModelIndex&, int start, int end) {
          const auto [row_offset, col_offset] = findModelOffset(model, 0, 0);

          this->beginRemoveColumns(ModelIndex(), start + col_offset,
                                   end + col_offset);
        }));
    m_connections.emplace_back(model->endRemoveColumns.connect(
        [this]() { this->endRemoveColumns(); }));

    m_connections.emplace_back(
        model->beginResetModel.connect([this]() { this->beginResetModel(); }));
    m_connections.emplace_back(
        model->endResetModel.connect([this]() { this->endResetModel(); }));

    m_connections.emplace_back(
        model->headerDataChanged.connect([this, model](int section, int role) {
          if (m_orientation == Orientation::Horizontal) {
            // Horizontal alignment adds column offsets across split boundaries
            const auto [row_offset, col_offset] =
                this->findModelOffset(model, 0, section);
            headerDataChanged(section + col_offset, role);
          } else {
            // Vertical alignment stacks models; columns match 1:1 up to max
            // width
            headerDataChanged(section, role);
          }
        }));
  }

  std::pair<int, int> findModelOffset(const AbstractItemModel* model_p,
                                      int row,
                                      int column) const {
    if (row < 0 || column < 0) {
      return {-1, -1};
    }
    if (m_orientation == Orientation::Horizontal) {
      // Adjust column index
      for (const auto& model : m_models) {
        if (model.get() == model_p) {
          break;
        }
        column += model->columnCount();
      }
    } else {
      for (const auto& model : m_models) {
        if (model.get() == model_p) {
          break;
        }
        row += model->rowCount();
      }
    }

    return {row, column};
  }
};

}  // namespace ftxmodel

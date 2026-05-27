#pragma once
#include "abstract_item_model.hpp"

namespace ftxmodel {

// Assuming our previous ModelIndex definition is available
class AbstractListModel : public AbstractItemModel {
 public:
  virtual ~AbstractListModel() = default;

  // A list ALWAYS has exactly 1 column
  int columnCount(const ModelIndex& = ModelIndex()) const final { return 1; }

  // A flat list item NEVER has a parent
  ModelIndex parent(const ModelIndex&) const final { return ModelIndex(); }

  // Simplifies index generation so subclasses don't mess up columns or parents
  ModelIndex index(int row,
                   int column = 0,
                   const ModelIndex& parent = ModelIndex()) const final {
    if (parent.isValid() || row < 0 || row >= rowCount() || column != 0) {
      return ModelIndex();  // Return an invalid index if bounds are broken
    }
    // Call a protected hook that lets the concrete model supply its internal
    // node pointer
    return createIndex(row, column, internalPointerAt(row));
  }

 protected:
  // Subclasses can optionally override this to provide internal pointers for
  // optimization
  virtual void* internalPointerAt(int row) const {
    std::ignore = row;
    return nullptr;
  }
};

}  // namespace ftxmodel

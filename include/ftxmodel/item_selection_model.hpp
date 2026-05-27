#pragma once
#include "abstract_item_model.hpp"

namespace ftxmodel {

class ItemSelectionModel {
 public:
  explicit ItemSelectionModel(AbstractItemModel* model) : model_(model) {}

  void setCurrentIndex(const ModelIndex& index) {
    if (current_index_ == index) {
      return;
    }
    current_index_ = index;
    currentIndexChanged(current_index_);
  }

  ModelIndex currentIndex() const { return current_index_; }
  AbstractItemModel* model() const { return model_; }

  // SigSlot event to notify views when focus switches rows/columns
  sigslot::signal_st<const ModelIndex&> currentIndexChanged;

 private:
  AbstractItemModel* model_;
  ModelIndex current_index_;
};

}  // namespace ftxmodel

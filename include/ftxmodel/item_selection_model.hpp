#pragma once
#include "abstract_item_model.hpp"

namespace ftxmodel {

class ItemSelectionModel {
 public:
  explicit ItemSelectionModel(const std::shared_ptr<AbstractItemModel>& model)
      : model_(model) {}

  void setCurrentIndex(const ModelIndex& index) {
    if (!model_ || !index.isValid()) {
      return;
    }
    UniqueNodeId id = index.uniqueId();
    if (current_id_ == id) {
      return;
    }
    current_id_ = id;
    currentIndexChanged(current_id_);
  }

  UniqueNodeId currentId() const { return current_id_; }
  ModelIndex currentIndex() const {
    return model_ ? model_->findIndexById(currentId()) : ModelIndex();
  }
  AbstractItemModel* model() const { return model_.get(); }

  // SigSlot event to notify views when focus switches rows/columns
  sigslot::signal_st<const UniqueNodeId&> currentIndexChanged;

 private:
  std::shared_ptr<AbstractItemModel> model_;
  UniqueNodeId current_id_ = {nullptr};
};

}  // namespace ftxmodel

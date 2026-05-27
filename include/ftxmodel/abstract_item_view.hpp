#pragma once
#include "abstract_item_model.hpp"
#include "item_delegate.hpp"
#include "item_selection_model.hpp"

namespace ftxmodel {

class AbstractItemView : public sigslot::observer {
 public:
  AbstractItemView() = default;
  virtual ~AbstractItemView() = default;

  // Attaches a data backend to this view interface
  virtual void setModel(AbstractItemModel* model) {
    // Automatically disconnects any slots previously bound to an old model
    this->disconnect_all();

    model_ = model;
    selection_model_ = std::make_unique<ItemSelectionModel>(model_);

    // Setup internal signal handlers: when model changes, the view should
    // repaint
    model_->dataChanged.connect(&AbstractItemView::onDataChanged, this);
  }

  // Attaches a presentation layout customizer to this view interface
  virtual void setItemDelegate(std::shared_ptr<ItemDelegate> delegate) {
    delegate_ = delegate;
  }

  AbstractItemModel* model() const { return model_; }
  ItemSelectionModel* selectionModel() const { return selection_model_.get(); }
  ItemDelegate* itemDelegate() const { return delegate_.get(); }

  // Pure virtual method to draw the entire structural frame container
  virtual ftxui::Element render() = 0;

 protected:
  // This acts as our slot callback
  void onDataChanged(const ModelIndex& topLeft, const ModelIndex& bottomRight) {
    // Refresh your FTXUI screen or trigger a repaint
    update();
  }

  // Forces the underlying framework screen loop to invalidate and refresh
  virtual void update() = 0;

 private:
  AbstractItemModel* model_ = nullptr;
  std::shared_ptr<ItemDelegate> delegate_ = nullptr;
  std::unique_ptr<ItemSelectionModel> selection_model_ = nullptr;
};

}  // namespace ftxmodel

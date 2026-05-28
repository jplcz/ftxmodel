#pragma once
#include <utility>

#include "abstract_item_model.hpp"
#include "ftxui/component/component_base.hpp"
#include "header_delegate.hpp"
#include "item_delegate.hpp"
#include "item_selection_model.hpp"

namespace ftxmodel {

class AbstractItemView : public ftxui::ComponentBase, public sigslot::observer {
 public:
  AbstractItemView() = default;
  ~AbstractItemView() override = default;

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
    delegate_ = std::move(delegate);
  }

  [[nodiscard]] AbstractItemModel* model() const { return model_; }
  [[nodiscard]] ItemSelectionModel* selectionModel() const {
    return selection_model_.get();
  }
  [[nodiscard]] ItemDelegate* itemDelegate() const { return delegate_.get(); }

  [[nodiscard]] bool Focusable() const override { return true; }

 protected:
  // This acts as our slot callback
  void onDataChanged(const ModelIndex&, const ModelIndex&) {
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

class AbstractGridLikeItemView : public AbstractItemView {
 private:
  std::shared_ptr<HeaderDelegate> header_delegate_ =
      std::make_shared<AdvancedHeaderDelegate>();
  bool show_headers_ = true;

 public:
  AbstractGridLikeItemView() = default;
  ~AbstractGridLikeItemView() override = default;

  // --- Header Management ---
  void setHeaderDelegate(const std::shared_ptr<HeaderDelegate>& delegate) {
    if (delegate) {
      header_delegate_ = delegate;
    }
  }

  [[nodiscard]] HeaderDelegate* headerDelegate() const {
    return header_delegate_.get();
  }

  void setShowHeaders(bool show) { show_headers_ = show; }
  [[nodiscard]] bool showHeaders() const { return show_headers_; }
};

}  // namespace ftxmodel

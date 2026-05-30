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
  virtual void setModel(const std::shared_ptr<AbstractItemModel>& model) {
    // Automatically disconnects any slots previously bound to an old model
    this->disconnect_all();

    model_ = model;
    if (!model_) {
      selection_model_.reset();
      this->update();  // Request an immediate frame repaint to handle the blank
                       // view state
      return;
    }

    // Instantiate a fresh selection model synchronized with the new data
    // authority
    selection_model_ = std::make_unique<ItemSelectionModel>(model_);

    // Content Value Updates
    model_->dataChanged.connect(&AbstractItemView::onDataChanged, this);
    model_->headerDataChanged.connect(&AbstractItemView::onHeaderDataChanged,
                                      this);

    // Row Modification Lifecycle Channels
    model_->beginInsertRows.connect(&AbstractItemView::onBeginInsertRows, this);
    model_->endInsertRows.connect(&AbstractItemView::onEndInsertRows, this);

    model_->beginRemoveRows.connect(&AbstractItemView::onBeginRemoveRows, this);
    model_->endRemoveRows.connect(&AbstractItemView::onEndRemoveRows, this);

    // Column Modification Lifecycle Channels
    model_->beginInsertColumns.connect(&AbstractItemView::onBeginInsertColumns,
                                       this);
    model_->endInsertColumns.connect(&AbstractItemView::onEndInsertColumns,
                                     this);

    model_->beginRemoveColumns.connect(&AbstractItemView::onBeginRemoveColumns,
                                       this);
    model_->endRemoveColumns.connect(&AbstractItemView::onEndRemoveColumns,
                                     this);

    // Total Model Reset Channels
    model_->beginResetModel.connect(&AbstractItemView::onBeginResetModel, this);
    model_->endResetModel.connect(&AbstractItemView::onEndResetModel, this);

    // Post-initialization view update to parse structural data layout
    // parameters
    this->update();
  }

  // Attaches a presentation layout customizer to this view interface
  virtual void setItemDelegate(std::shared_ptr<ItemDelegate> delegate) {
    delegate_ = std::move(delegate);
  }

  [[nodiscard]] AbstractItemModel* model() const { return model_.get(); }
  [[nodiscard]] ItemSelectionModel* selectionModel() const {
    return selection_model_.get();
  }
  [[nodiscard]] ItemDelegate* itemDelegate() const { return delegate_.get(); }

  [[nodiscard]] bool Focusable() const override { return true; }

 protected:
  // Forces the underlying framework screen loop to invalidate and refresh
  virtual void update() = 0;

  // ==========================================================================
  // Protected Signal Receiver Slot Sinks
  // ==========================================================================

  /**
   * @brief Invoked when specific cell value payloads inside a bounded range
   * change.
   */
  virtual void onDataChanged(const ModelIndex& topLeft,
                             const ModelIndex& bottomRight) {
    std::ignore = topLeft;
    std::ignore = bottomRight;
    this->update();  // Default action: flag frame repaint
  }

  /**
   * @brief Invoked when border layout tracks or section labels are altered.
   */
  virtual void onHeaderDataChanged(int section, int role) {
    std::ignore = section;
    std::ignore = role;
    this->update();
  }

  /**
   * @brief Invoked right before fresh rows are appended into the dataset tree.
   */
  virtual void onBeginInsertRows(const ModelIndex& parent, int start, int end) {
    std::ignore = parent;
    std::ignore = start;
    std::ignore = end;
    // Proxies use this to safeguard or offset active tracking selection models
  }

  /**
   * @brief Invoked immediately after raw row entries are securely instantiated.
   */
  virtual void onEndInsertRows() { this->update(); }

  /**
   * @brief Invoked right before structural rows are cut out of memory.
   */
  virtual void onBeginRemoveRows(const ModelIndex& parent, int start, int end) {
    std::ignore = parent;
    std::ignore = start;
    std::ignore = end;
    // Overrides should move selections away from indexes targeted for deletion
  }

  /**
   * @brief Invoked immediately after raw row deletion procedures wrap up.
   */
  virtual void onEndRemoveRows() { this->update(); }

  /**
   * @brief Invoked right before horizontal column indices are expanded.
   */
  virtual void onBeginInsertColumns(const ModelIndex& parent,
                                    int start,
                                    int end) {
    std::ignore = parent;
    std::ignore = start;
    std::ignore = end;
  }

  /**
   * @brief Invoked immediately after new horizontal layout tracks are
   * configured.
   */
  virtual void onEndInsertColumns() { this->update(); }

  /**
   * @brief Invoked right before data columns are purged from memory systems.
   */
  virtual void onBeginRemoveColumns(const ModelIndex& parent,
                                    int start,
                                    int end) {
    std::ignore = parent;
    std::ignore = start;
    std::ignore = end;
  }

  /**
   * @brief Invoked immediately after horizontal structural data tracks are
   * removed.
   */
  virtual void onEndRemoveColumns() { this->update(); }

  /**
   * @brief Invoked right before the model completely drops its active index
   * layout tree maps.
   */
  virtual void onBeginResetModel() {
    // Clear out local display caches, selection markers, or flattened lookups
    // here
  }

  /**
   * @brief Invoked after a comprehensive model data purge and rebuild finishes.
   */
  virtual void onEndResetModel() { this->update(); }

 private:
  std::shared_ptr<AbstractItemModel> model_;
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

  void setShowHeaders(const bool show) { show_headers_ = show; }
  [[nodiscard]] bool showHeaders() const { return show_headers_; }
};

}  // namespace ftxmodel

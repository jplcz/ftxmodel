#pragma once
#include <utility>

#include "abstract_item_model.hpp"
#include "ftxui/component/component_base.hpp"
#include "header_delegate.hpp"
#include "item_delegate.hpp"
#include "item_selection_model.hpp"
#include "selection_highlight_style.hpp"

namespace ftxmodel {

class AbstractItemView : public ftxui::ComponentBase, public sigslot::observer {
  UniqueNodeId m_preserved_selection_id{nullptr};
  int m_fallback_row_index = -1;
  int m_target_column = 0;

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
    onBeginResetModel();
    onEndResetModel();
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
  void onDataChanged(const ModelIndex& topLeft, const ModelIndex& bottomRight) {
    std::ignore = topLeft;
    std::ignore = bottomRight;
    this->update();  // Default action: flag frame repaint
  }

  /**
   * @brief Invoked when border layout tracks or section labels are altered.
   */
  void onHeaderDataChanged(int section, int role) {
    std::ignore = section;
    std::ignore = role;
    this->update();
  }

  /**
   * @brief Invoked right before fresh rows are appended into the dataset tree.
   */
  void onBeginInsertRows(const ModelIndex& parent, int start, int end) {
    std::ignore = parent;
    std::ignore = start;
    std::ignore = end;
    // Proxies use this to safeguard or offset active tracking selection models
  }

  /**
   * @brief Invoked immediately after raw row entries are securely instantiated.
   */
  void onEndInsertRows() { this->update(); }

  /**
   * @brief Invoked right before structural rows are cut out of memory.
   */
  void onBeginRemoveRows(const ModelIndex& parent, int start, int end) {
    if (!selectionModel() || !model()) {
      return;
    }

    ModelIndex current = selectionModel()->currentIndex();
    if (!current.isValid()) {
      return;
    }

    // Is the currently selected row inside the block being deleted?
    if (current.parent() == parent && current.row() >= start &&
        current.row() <= end) {
      // Save its unique persistent ID token
      m_preserved_selection_id = model()->uniqueId(current);
      m_target_column = current.column();

      // Compute a smart fallback row index (the row right above the deletion
      // block)
      m_fallback_row_index = std::max(0, start - 1);
    } else if (current.parent() == parent && current.row() > end) {
      // If the deletion happens ABOVE our selection, our row index will shift
      // up. We store the unique ID to re-verify it post-delete.
      m_preserved_selection_id = model()->uniqueId(current);
      m_target_column = current.column();
      m_fallback_row_index = current.row() - (end - start + 1);
    }
  }

  /**
   * @brief Invoked immediately after raw row deletion procedures wrap up.
   */
  void onEndRemoveRows() {
    this->update();  // Flush the view layout

    if (m_preserved_selection_id == UniqueNodeId{nullptr} ||
        !selectionModel() || !model()) {
      return;  // No active selection was impacted
    }

    // Attempt a data-first reverse lookup to see if the item moved elsewhere
    ModelIndex remarpped_index =
        model()->findIndexById(m_preserved_selection_id);

    if (remarpped_index.isValid()) {
      // The item survived or moved; restore focus to its new location
      selectionModel()->setCurrentIndex(remarpped_index);
    } else {
      // The item is completely gone. Fall back to the nearest structural
      // neighbor row
      if (const int safe_row =
              std::min(m_fallback_row_index, model()->rowCount() - 1);
          safe_row >= 0) {
        selectionModel()->setCurrentIndex(
            model()->index(safe_row, m_target_column));
      } else {
        // The table is completely empty now
        selectionModel()->setCurrentIndex(ModelIndex());
      }
    }

    // Clear the temporary tracking state
    m_preserved_selection_id = {nullptr};
    m_fallback_row_index = -1;
  }

  /**
   * @brief Invoked right before horizontal column indices are expanded.
   */
  void onBeginInsertColumns(const ModelIndex& parent, int start, int end) {
    std::ignore = parent;
    std::ignore = start;
    std::ignore = end;
  }

  /**
   * @brief Invoked immediately after new horizontal layout tracks are
   * configured.
   */
  void onEndInsertColumns() { this->update(); }

  /**
   * @brief Invoked right before data columns are purged from memory systems.
   */
  void onBeginRemoveColumns(const ModelIndex& parent, int start, int end) {
    std::ignore = parent;
    std::ignore = start;
    std::ignore = end;
  }

  /**
   * @brief Invoked immediately after horizontal structural data tracks are
   * removed.
   */
  void onEndRemoveColumns() { this->update(); }

  /**
   * @brief Invoked right before the model completely drops its active index
   * layout tree maps.
   */
  void onBeginResetModel() {
    if (selectionModel() && model()) {
      if (const ModelIndex current = selectionModel()->currentIndex();
          current.isValid()) {
        m_preserved_selection_id = model()->uniqueId(current);
        m_target_column = current.column();
      }
    }
  }

  /**
   * @brief Invoked after a comprehensive model data purge and rebuild finishes.
   */
  void onEndResetModel() {
    this->update();  // Rebuild layout caches

    if (m_preserved_selection_id != UniqueNodeId{nullptr} && selectionModel() &&
        model()) {
      if (const ModelIndex restored =
              model()->findIndexById(m_preserved_selection_id);
          restored.isValid()) {
        selectionModel()->setCurrentIndex(restored);
      } else if (model()->rowCount() > 0) {
        // Fallback to top-left if the original item no longer exists post-reset
        selectionModel()->setCurrentIndex(model()->index(0, 0));
      } else {
        selectionModel()->setCurrentIndex(ModelIndex());
      }
    }
    m_preserved_selection_id = {nullptr};
  }

 private:
  std::shared_ptr<AbstractItemModel> model_;
  std::shared_ptr<ItemDelegate> delegate_ = nullptr;
  std::unique_ptr<ItemSelectionModel> selection_model_ = nullptr;
};

class AbstractGridLikeItemView : public AbstractItemView {
 private:
  // Separate delegate tracks for fine-grained axis styling
  std::shared_ptr<HeaderDelegate> horizontal_header_delegate_ =
      std::make_shared<AdvancedHeaderDelegate>();
  std::shared_ptr<HeaderDelegate> vertical_header_delegate_ =
      std::make_shared<AdvancedHeaderDelegate>();
  std::shared_ptr<SelectionHighlightStyle> highlight_style_ =
      std::make_shared<SelectionHighlightStyle>();  // Default fallbacks

  // Independent visibility control switches
  bool show_horizontal_headers_ = false;
  bool show_vertical_headers_ = false;

 public:
  AbstractGridLikeItemView() = default;
  ~AbstractGridLikeItemView() override = default;

  // =========================================================================
  // --- Horizontal Header Management (Columns) ---
  // =========================================================================
  void setHorizontalHeaderDelegate(
      const std::shared_ptr<HeaderDelegate>& delegate) {
    if (delegate) {
      horizontal_header_delegate_ = delegate;
    }
  }

  [[nodiscard]] HeaderDelegate* horizontalHeaderDelegate() const {
    return horizontal_header_delegate_.get();
  }

  void setShowHorizontalHeaders(const bool show) {
    show_horizontal_headers_ = show;
  }
  [[nodiscard]] bool showHorizontalHeaders() const {
    return show_horizontal_headers_;
  }

  // =========================================================================
  // --- Vertical Header Management (Rows) ---
  // =========================================================================
  void setVerticalHeaderDelegate(
      const std::shared_ptr<HeaderDelegate>& delegate) {
    if (delegate) {
      vertical_header_delegate_ = delegate;
    }
  }

  [[nodiscard]] HeaderDelegate* verticalHeaderDelegate() const {
    return vertical_header_delegate_.get();
  }

  void setShowVerticalHeaders(const bool show) {
    show_vertical_headers_ = show;
  }

  [[nodiscard]] bool showVerticalHeaders() const {
    return show_vertical_headers_;
  }

  // =========================================================================
  // --- Selection Highlight ---
  // =========================================================================

  void setHighlightStyle(std::shared_ptr<SelectionHighlightStyle> style) {
    if (style) {
      highlight_style_ = std::move(style);
    }
  }

  [[nodiscard]] SelectionHighlightStyle* highlightStyle() const noexcept {
    return highlight_style_.get();
  }

  void setModel(const std::shared_ptr<AbstractItemModel>& model) override {
    AbstractItemView::setModel(model);
    if (model && !selectionModel()->currentIndex().isValid()) {
      selectionModel()->setCurrentIndex(model->index(0, 0));
    }
  }
};

}  // namespace ftxmodel

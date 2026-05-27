#pragma once
#include <algorithm>
#include <functional>
#include <string>
#include <vector>
#include "abstract_item_view.hpp"
#include "ftxui/dom/elements.hpp"
#include "header_delegate.hpp"

namespace ftxmodel {

class TableView : public AbstractItemView {
 private:
  std::function<void()> trigger_ftxui_refresh_;
  bool show_headers_ = true;

 public:
  explicit TableView(std::function<void()> refreshCb)
      : trigger_ftxui_refresh_(refreshCb) {}

  void setShowHeaders(bool show) { show_headers_ = show; }

  void setModel(AbstractItemModel* model) override {
    // Essential: Invoke base class to hook up signals and instantiate
    // SelectionModel
    AbstractItemView::setModel(model);

    if (model && model->rowCount() > 0 && model->columnCount() > 0) {
      // Default focus to the top-left cell (0, 0)
      selectionModel()->setCurrentIndex(model->index(0, 0));
    }
  }

  // ========================================================================
  // 2D Navigation Controls
  // ========================================================================
  void moveUp() {
    ModelIndex current = selectionModel()->currentIndex();
    if (current.row() > 0) {
      selectionModel()->setCurrentIndex(
          model()->index(current.row() - 1, current.column()));
      update();
    }
  }

  void moveDown() {
    ModelIndex current = selectionModel()->currentIndex();
    if (current.row() < model()->rowCount() - 1) {
      selectionModel()->setCurrentIndex(
          model()->index(current.row() + 1, current.column()));
      update();
    }
  }

  void moveLeft() {
    ModelIndex current = selectionModel()->currentIndex();
    if (current.column() > 0) {
      selectionModel()->setCurrentIndex(
          model()->index(current.row(), current.column() - 1));
      update();
    }
  }

  void moveRight() {
    ModelIndex current = selectionModel()->currentIndex();
    if (current.column() < model()->columnCount() - 1) {
      selectionModel()->setCurrentIndex(
          model()->index(current.row(), current.column() + 1));
      update();
    }
  }

  // ========================================================================
  // Rendering Logic
  // ========================================================================
  ftxui::Element render() override {
    if (!model() || !itemDelegate()) {
      return ftxui::text("Missing model or delegate bindings.");
    }

    std::vector<std::vector<ftxui::Element>> gridMatrix;
    int totalRows = model()->rowCount();
    int totalCols = model()->columnCount();
    ModelIndex focusedIndex = selectionModel()->currentIndex();

    // Optional Horizontal Headers Pass
    if (show_headers_) {
      std::vector<ftxui::Element> headerRow;
      for (int c = 0; c < totalCols; ++c) {
        // COMPONENT INTERACTION: Ask header delegate to construct the visual
        // block
        headerRow.push_back(headerDelegate()->createHeaderWidget(
            c, Orientation::Horizontal, model()));
      }
      gridMatrix.push_back(std::move(headerRow));
    }

    // 2D Data Rows Pass
    for (int r = 0; r < totalRows; ++r) {
      std::vector<ftxui::Element> uiRow;
      for (int c = 0; c < totalCols; ++c) {
        ModelIndex idx = model()->index(r, c);

        // Invoke user delegate
        ftxui::Element cellWidget = itemDelegate()->createWidget(idx, model());

        // Decorate cell widget based on active selection states
        if (idx == focusedIndex) {
          // Deep focus on the precise selected cell coordinates
          cellWidget = cellWidget | ftxui::bgcolor(ftxui::Color::Blue) |
                       ftxui::color(ftxui::Color::White) | ftxui::bold;
        } else if (r == focusedIndex.row()) {
          // Light row tracking highlight to visually guide across data columns
          cellWidget = cellWidget | ftxui::bgcolor(ftxui::Color::GrayDark);
        }

        uiRow.push_back(cellWidget);
      }
      gridMatrix.push_back(std::move(uiRow));
    }

    // Returns perfectly aligned 2D terminal grid canvas boundary box
    return ftxui::gridbox(gridMatrix) | ftxui::border;
  }

 protected:
  void update() override {
    trigger_ftxui_refresh_();  // Post event down pipeline to redraw loop
  }
};

}  // namespace ftxmodel

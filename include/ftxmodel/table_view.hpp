#pragma once
#include <algorithm>
#include <functional>
#include <string>
#include <utility>
#include <vector>
#include "abstract_item_view.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "header_delegate.hpp"

namespace ftxmodel {

class TableView : public AbstractGridLikeItemView {
 private:
  std::function<void()> trigger_ftxui_refresh_;

  // Helper calculation loop to find the widest cell footprint constraint in a
  // specific column track
  [[nodiscard]] int calculateOptimalColumnWidth(int colIndex) const {
    int maxColumnWidth = 0;
    int totalRows = model()->rowCount();

    // Check the header's size requirements first if headers are turned on
    if (showHeaders()) {
      const std::any hData = model()->headerData(
          colIndex, Orientation::Horizontal, ItemRole::DisplayRole);
      std::string header_text = AnyToStringTranslator::Translate(hData);

      if (!header_text.empty()) {
        const ftxui::Dimensions header_bounds =
            UnicodeTextScaler::GetTextBounds(header_text);
        maxColumnWidth = std::max(maxColumnWidth, header_bounds.dimx + 2);
      }
    }

    // Query the item delegate for every active cell row in this column
    for (int r = 0; r < totalRows; ++r) {
      ModelIndex index = model()->index(r, colIndex);
      if (index.isValid()) {
        // Poll our newly integrated delegate sizeHint API component loop
        ftxui::Dimensions hint = itemDelegate()->sizeHint(index, model());
        maxColumnWidth = std::max(maxColumnWidth, hint.dimx);
      }
    }

    // Add a 1-character padding buffer to prevent text clipping against layout
    // borders
    return maxColumnWidth + 1;
  }

 public:
  explicit TableView(std::function<void()> refreshCb)
      : trigger_ftxui_refresh_(std::move(refreshCb)) {}

  void setModel(const std::shared_ptr<AbstractItemModel>&  model) override {
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
  bool moveUp() {
    ModelIndex current = selectionModel()->currentIndex();
    if (current.row() > 0) {
      selectionModel()->setCurrentIndex(
          model()->index(current.row() - 1, current.column()));
      update();
      return true;
    }
    return false;
  }

  bool moveDown() {
    ModelIndex current = selectionModel()->currentIndex();
    if (current.row() < model()->rowCount() - 1) {
      selectionModel()->setCurrentIndex(
          model()->index(current.row() + 1, current.column()));
      update();
      return true;
    }
    return false;
  }

  bool moveLeft() {
    ModelIndex current = selectionModel()->currentIndex();
    if (current.column() > 0) {
      selectionModel()->setCurrentIndex(
          model()->index(current.row(), current.column() - 1));
      update();
      return true;
    }
    return false;
  }

  bool moveRight() {
    ModelIndex current = selectionModel()->currentIndex();
    if (current.column() < model()->columnCount() - 1) {
      selectionModel()->setCurrentIndex(
          model()->index(current.row(), current.column() + 1));
      update();
      return true;
    }
    return false;
  }

  bool OnEvent(ftxui::Event event) override {
    if (event == ftxui::Event::ArrowUp) {
      if (moveUp()) {
        return true;
      }
    } else if (event == ftxui::Event::ArrowDown) {
      if (moveDown()) {
        return true;
      }
    } else if (event == ftxui::Event::ArrowLeft) {
      if (moveLeft()) {
        return true;
      }
    } else if (event == ftxui::Event::ArrowRight) {
      if (moveRight()) {
        return true;
      }
    }
    return AbstractGridLikeItemView::OnEvent(event);
  }

  // ========================================================================
  // Rendering Logic
  // ========================================================================
  ftxui::Element OnRender() override {
    if (!model() || !itemDelegate()) {
      return ftxui::text("Missing model or delegate bindings.");
    }

    std::vector<std::vector<ftxui::Element>> gridMatrix;
    int totalRows = model()->rowCount();
    int totalCols = model()->columnCount();
    ModelIndex focusedIndex = selectionModel()->currentIndex();

    // Precalculate optimal column layout widths
    std::vector<int> colWidths((size_t)totalCols, 0);
    for (int c = 0; c < totalCols; ++c) {
      colWidths[(size_t)c] = calculateOptimalColumnWidth(c);
    }

    // Optional Horizontal Headers Pass
    if (showHeaders()) {
      std::vector<ftxui::Element> headerRow;
      for (int c = 0; c < totalCols; ++c) {
        ftxui::Element hWidget = headerDelegate()->createHeaderWidget(
            c, Orientation::Horizontal, model());

        // Enforce calculated uniform column constraint limits!
        hWidget = hWidget |
                  ftxui::size(ftxui::WIDTH, ftxui::EQUAL, colWidths[(size_t)c]);
        headerRow.emplace_back(hWidget);

        // Inject a light separator between headers, but skip after the final
        // column
        if (c < totalCols - 1) {
          headerRow.emplace_back(ftxui::separatorLight());
        }
      }
      gridMatrix.emplace_back(std::move(headerRow));
    }

    // 2D Data Rows Pass
    for (int r = 0; r < totalRows; ++r) {
      std::vector<ftxui::Element> uiRow;
      for (int c = 0; c < totalCols; ++c) {
        ModelIndex idx = model()->index(r, c);

        // Invoke user delegate
        ftxui::Element cellWidget = itemDelegate()->createWidget(idx, model());

        // Enforce calculated uniform column size metrics down across cells!
        cellWidget = cellWidget | ftxui::size(ftxui::WIDTH, ftxui::EQUAL,
                                              colWidths[(size_t)c]);

        // Check selection state for the entire row to handle background
        // rendering
        bool isRowSelected = (r == focusedIndex.row());

        // Decorate cell widget based on active selection states
        if (idx == focusedIndex) {
          // Deep focus on the precise selected cell coordinates
          cellWidget = cellWidget | ftxui::bgcolor(ftxui::Color::Blue) |
                       ftxui::color(ftxui::Color::White) | ftxui::bold;
          if (Focused()) {
            cellWidget = cellWidget | ftxui::focus;
          }
        } else if (isRowSelected) {
          // Light row tracking highlight to visually guide across data columns
          cellWidget = cellWidget | ftxui::bgcolor(ftxui::Color::GrayDark);
        }

        uiRow.emplace_back(cellWidget);

        // Inject vertical separator between cells, skipping after the last
        // column
        if (c < totalCols - 1) {
          auto sep = ftxui::separatorLight();

          // Color the separator background to match the row's selection state
          if (idx == focusedIndex ||
              (r == focusedIndex.row() && c == focusedIndex.column() - 1)) {
            // Separator is adjacent to the uniquely focused cell
            sep = sep | ftxui::bgcolor(ftxui::Color::Blue);
          } else if (isRowSelected) {
            // Separator is part of the general row selection guide track
            sep = sep | ftxui::bgcolor(ftxui::Color::GrayDark);
          }

          uiRow.push_back(sep);
        }
      }
      gridMatrix.emplace_back(std::move(uiRow));
    }

    // Returns perfectly aligned 2D terminal grid canvas boundary box
    return ftxui::gridbox(gridMatrix) | ftxui::vscroll_indicator |
           ftxui::frame | ftxui::border;
  }

 protected:
  void update() override {
    trigger_ftxui_refresh_();  // Post event down pipeline to redraw loop
  }
};

}  // namespace ftxmodel

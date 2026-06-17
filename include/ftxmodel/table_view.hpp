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
  // Helper calculation loop to find the widest cell footprint constraint in a
  // specific column track
  [[nodiscard]] int calculateOptimalColumnWidth(int colIndex) const {
    int maxColumnWidth = 0;
    int totalRows = model()->rowCount();

    // Check the header's size requirements first if headers are turned on
    if (showHorizontalHeaders()) {
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
  TableView() = default;

  void setModel(const std::shared_ptr<AbstractItemModel>& model) override {
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

  bool moveHome() {
    ModelIndex current = selectionModel()->currentIndex();
    const int row = current.row();
    if (row > 0) {
      selectionModel()->setCurrentIndex(model()->index(0, current.column()));
      update();
      return true;
    }
    return false;
  }

  bool movePageUp() {
    ModelIndex current = selectionModel()->currentIndex();
    const int row = current.row();
    if (row > 0) {
      selectionModel()->setCurrentIndex(
          model()->index(std::max<int>(row - 5, 0), current.column()));
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

  bool movePageDown() {
    ModelIndex current = selectionModel()->currentIndex();
    if (current.row() < model()->rowCount() - 1) {
      selectionModel()->setCurrentIndex(
          model()->index(std::min(current.row() + 5, model()->rowCount() - 1),
                         current.column()));
      update();
      return true;
    }
    return false;
  }

  bool moveEnd() {
    ModelIndex current = selectionModel()->currentIndex();
    if (current.row() < model()->rowCount() - 1) {
      selectionModel()->setCurrentIndex(
          model()->index(model()->rowCount() - 1, current.column()));
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
    } else if (event == ftxui::Event::Home) {
      if (moveHome()) {
        return true;
      }
    } else if (event == ftxui::Event::PageUp) {
      if (movePageUp()) {
        return true;
      }
    } else if (event == ftxui::Event::PageDown) {
      if (movePageDown()) {
        return true;
      }
    } else if (event == ftxui::Event::End) {
      if (moveEnd()) {
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
    const auto delegate = highlightStyle();

    // Precalculate optimal column layout widths
    std::vector<int> colWidths((size_t)totalCols, 0);
    for (int c = 0; c < totalCols; ++c) {
      colWidths[(size_t)c] = calculateOptimalColumnWidth(c);
    }

    // Optional Horizontal Headers Pass
    if (showHorizontalHeaders()) {
      std::vector<ftxui::Element> headerRow;

      if (showVerticalHeaders()) {
        headerRow.emplace_back(ftxui::text(" "));
      }

      for (int c = 0; c < totalCols; ++c) {
        ftxui::Element hWidget = horizontalHeaderDelegate()->createHeaderWidget(
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

      if (showVerticalHeaders()) {
        uiRow.emplace_back(verticalHeaderDelegate()->createHeaderWidget(
            r, Orientation::Vertical, model()));
      }

      for (int c = 0; c < totalCols; ++c) {
        ModelIndex idx = model()->index(r, c);

        // Invoke user delegate
        ftxui::Element cellWidget = itemDelegate()->createWidget(idx, model());

        // Enforce calculated uniform column size metrics down across cells!
        cellWidget = cellWidget | ftxui::size(ftxui::WIDTH, ftxui::EQUAL,
                                              colWidths[(size_t)c]);

        // Check selection state for the entire row to handle background
        // rendering
        const bool isRowSelected = (r == focusedIndex.row());

        ViewStateFlags f = ViewNormal;

        if (Focused()) {
          f |= ViewFocused;
        }

        if (focusedIndex.isValid()) {
          f |= ViewSelected;
        }

        if (isRowSelected) {
          f |= ViewIsSameRow;
        }

        if (idx == focusedIndex) {
          f |= ViewIsExactCell;
        }

        cellWidget = delegate->applyHighlight(std::move(cellWidget), f);
        cellWidget = delegate->applyGlobalFocus(std::move(cellWidget), f, idx);

        uiRow.emplace_back(cellWidget);

        // Inject vertical separator between cells, skipping after the last
        // column
        if (c < totalCols - 1) {
          uiRow.push_back(
              delegate->applySeparatorHighlight(ftxui::separatorLight(), f));
        }
      }
      gridMatrix.emplace_back(std::move(uiRow));
    }

    // Returns perfectly aligned 2D terminal grid canvas boundary box
    return ftxui::gridbox(gridMatrix);
  }

 protected:
  void update() override {
    const auto selectedIndex = selectionModel()->currentIndex();
    if (!selectedIndex.isValid() && model()) {
      const auto rootIndex = model()->index(0, 0);
      if (rootIndex.isValid()) {
        selectionModel()->setCurrentIndex(rootIndex);
      }
    }
  }
};

}  // namespace ftxmodel

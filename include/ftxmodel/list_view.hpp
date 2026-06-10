#pragma once
#include <algorithm>
#include "abstract_item_view.hpp"
#include "abstract_list_model.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"

namespace ftxmodel {

class ListView : public AbstractGridLikeItemView {
 private:
  int selected_row_ = 0;

 public:
  ListView() = default;

  void setModel(const std::shared_ptr<AbstractItemModel>& model) override {
    AbstractItemView::setModel(model);
    selected_row_ = 0;
    if (model && model->rowCount() > 0) {
      selectionModel()->setCurrentIndex(model->index(0, 0));
    }
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
    } else if (event == ftxui::Event::Character(' ')) {
      toggleCurrentItem();
      return true;
    }
    return AbstractGridLikeItemView::OnEvent(event);
  }

  bool moveUp() {
    int currentRow = selectionModel()->currentIndex().row();
    if (currentRow > 0) {
      selectionModel()->setCurrentIndex(model()->index(currentRow - 1, 0));
      update();
      return true;
    }
    return false;
  }

  bool moveDown() {
    int currentRow = selectionModel()->currentIndex().row();
    if (currentRow < model()->rowCount() - 1) {
      selectionModel()->setCurrentIndex(model()->index(currentRow + 1, 0));
      update();
      return true;
    }
    return false;
  }

  void toggleCurrentItem() {
    ModelIndex idx = selectionModel()->currentIndex();
    if (!idx.isValid()) {
      return;
    }
    bool currentStatus =
        std::any_cast<bool>(model()->data(idx, ItemRole::CheckedRole));
    model()->setData(idx, !currentStatus, ItemRole::CheckedRole);
  }

  ftxui::Element OnRender() override {
    if (!model() || !itemDelegate()) {
      return ftxui::text("Missing model or delegate bindings.");
    }
    const int totalRows = model()->rowCount();
    const auto currentIndex = selectionModel()->currentIndex();
    const int activeRow = currentIndex.row();
    const bool viewFocused = this->Focused();
    const auto* barStyle = this->highlightStyle();

    // Compute List Panel Box Width Limit
    int optimalWidth = 0;
    for (int r = 0; r < totalRows; ++r) {
      optimalWidth = std::max(
          optimalWidth,
          itemDelegate()->sizeHint(model()->index(r, 0), model()).dimx);
    }
    optimalWidth = std::max(optimalWidth + 2,
                            15);  // Snug minimum constraint limit boundary

    std::vector<std::vector<ftxui::Element>> renderedRows;

    if (showHorizontalHeaders()) {
      std::vector<ftxui::Element> row;

      if (showVerticalHeaders()) {
        row.emplace_back(ftxui::text(" "));
      }
      // Pulls directly from the base class via headerDelegate()
      ftxui::Element headerWidget =
          horizontalHeaderDelegate()->createHeaderWidget(
              0, Orientation::Horizontal, model());
      row.emplace_back(headerWidget |
                       ftxui::size(ftxui::WIDTH, ftxui::EQUAL, optimalWidth));
      row.emplace_back(ftxui::separator());
      renderedRows.emplace_back(std::move(row));
    }

    for (int r = 0; r < totalRows; ++r) {
      std::vector<ftxui::Element> row;

      if (showVerticalHeaders()) {
        row.emplace_back(verticalHeaderDelegate()->createHeaderWidget(
            r, Orientation::Vertical, model()));
      }

      ModelIndex idx = model()->index(r, 0);
      ftxui::Element cellWidget =
          itemDelegate()->createWidget(idx, model()) |
          ftxui::size(ftxui::WIDTH, ftxui::EQUAL, optimalWidth);

      ViewStateFlags f = ViewNormal;

      if (currentIndex.isValid()) {
        f |= ViewSelected;
      }

      if (r == activeRow) {
        f |= ViewIsSameRow;
      }

      if (idx == currentIndex) {
        f |= ViewIsExactCell;
      }

      if (viewFocused) {
        f |= ViewFocused;
      }

      cellWidget = barStyle->applyHighlight(std::move(cellWidget), f);
      cellWidget = barStyle->applyGlobalFocus(std::move(cellWidget), f, idx);

      row.push_back(cellWidget);

      renderedRows.emplace_back(std::move(row));
    }
    return ftxui::gridbox(std::move(renderedRows)) | ftxui::vscroll_indicator |
           ftxui::frame | ftxui::border;
  }

 protected:
  void update() override {}
};

}  // namespace  ftxmodel

#pragma once
#include <algorithm>
#include "abstract_item_view.hpp"
#include "abstract_list_model.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"

namespace ftxmodel {

class ListView : public AbstractGridLikeItemView {
 private:
  int selected_row_ = 0;
  std::function<void()> trigger_ftxui_refresh_;

 public:
  explicit ListView(std::function<void()> refreshCb)
      : trigger_ftxui_refresh_(refreshCb) {}

  void setModel(AbstractItemModel* model) override {
    AbstractItemView::setModel(model);
    selected_row_ = 0;
    if (model && model->rowCount() > 0) {
      selectionModel()->setCurrentIndex(model->index(0, 0));
    }
  }

  void moveUp() override {
    int currentRow = selectionModel()->currentIndex().row();
    if (currentRow > 0) {
      selectionModel()->setCurrentIndex(model()->index(currentRow - 1, 0));
      update();
    }
  }

  void moveDown() override {
    int currentRow = selectionModel()->currentIndex().row();
    if (currentRow < model()->rowCount() - 1) {
      selectionModel()->setCurrentIndex(model()->index(currentRow + 1, 0));
      update();
    }
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

  ftxui::Element render() override {
    if (!model() || !itemDelegate()) {
      return ftxui::text("Missing model or delegate bindings.");
    }
    int totalRows = model()->rowCount();
    int activeRow = selectionModel()->currentIndex().row();

    // Compute List Panel Box Width Limit
    int optimalWidth = 0;
    for (int r = 0; r < totalRows; ++r) {
      optimalWidth = std::max(
          optimalWidth,
          itemDelegate()->sizeHint(model()->index(r, 0), model()).dimx);
    }
    optimalWidth = std::max(optimalWidth + 2,
                            15);  // Snug minimum constraint limit boundary

    std::vector<ftxui::Element> renderedRows;

    if (showHeaders()) {
      // Pulls directly from the base class via headerDelegate()
      ftxui::Element headerWidget = headerDelegate()->createHeaderWidget(
          0, Orientation::Horizontal, model());
      renderedRows.push_back(
          headerWidget | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, optimalWidth));
      renderedRows.push_back(ftxui::separator());
    }

    for (int r = 0; r < totalRows; ++r) {
      ModelIndex idx = model()->index(r, 0);
      ftxui::Element cellWidget =
          itemDelegate()->createWidget(idx, model()) |
          ftxui::size(ftxui::WIDTH, ftxui::EQUAL, optimalWidth);

      // Highlight selection
      if (r == activeRow) {
        cellWidget = cellWidget | ftxui::bgcolor(ftxui::Color::Blue) |
                     ftxui::color(ftxui::Color::White) | ftxui::bold;
      }
      renderedRows.push_back(cellWidget);
    }
    return ftxui::vbox(std::move(renderedRows)) | ftxui::border;
  }

  void update() override { trigger_ftxui_refresh_(); }
};

}  // namespace  ftxmodel

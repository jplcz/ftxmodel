#pragma once
#include <functional>
#include <set>
#include <string>
#include <vector>
#include "abstract_item_view.hpp"
#include "ftxui/dom/elements.hpp"
#include "header_delegate.hpp"

namespace ftxmodel {

class TreeView : public AbstractGridLikeItemView {
 private:
  std::function<void()> trigger_ftxui_refresh_;
  std::vector<ModelIndex> flattened_indices_;

  // Tracks which nodes are currently expanded using their internal pointer
  // address
  std::set<void*> expanded_nodes_;

  int selected_linear_row_ = 0;

 public:
  explicit TreeView(std::function<void()> refreshCb)
      : trigger_ftxui_refresh_(refreshCb) {}

  void setModel(AbstractItemModel* model) override {
    AbstractItemView::setModel(model);
    expanded_nodes_.clear();
    selected_linear_row_ = 0;
    rebuildFlattenedTree();
  }

  // ========================================================================
  // Keyboard Navigation Mapping
  // ========================================================================
  void moveUp() {
    if (selected_linear_row_ > 0) {
      selected_linear_row_--;
      selectionModel()->setCurrentIndex(
          flattened_indices_[(size_t)selected_linear_row_]);
      update();
    }
  }

  void moveDown() {
    if (selected_linear_row_ <
        static_cast<int>(flattened_indices_.size()) - 1) {
      selected_linear_row_++;
      selectionModel()->setCurrentIndex(
          flattened_indices_[(size_t)selected_linear_row_]);
      update();
    }
  }

  // Right Arrow: Expand current item if it has children
  void moveRight() {
    ModelIndex current = selectionModel()->currentIndex();
    if (current.isValid() && model()->hasChildren(current)) {
      void* ptr = current.internalPointer();
      if (expanded_nodes_.find(ptr) == expanded_nodes_.end()) {
        expanded_nodes_.insert(ptr);
        update();  // Force flattening recalculation
      }
    }
  }

  // Left Arrow: Collapse current item, or jump up to its parent if already
  // collapsed
  void moveLeft() {
    ModelIndex current = selectionModel()->currentIndex();
    if (!current.isValid()) {
      return;
    }

    void* ptr = current.internalPointer();
    if (expanded_nodes_.find(ptr) != expanded_nodes_.end()) {
      // Node is open, close it
      expanded_nodes_.erase(ptr);
      update();
    } else {
      // Node is already closed, jump focus smoothly up to its structural parent
      ModelIndex parentIdx = model()->parent(current);
      if (parentIdx.isValid()) {
        auto it = std::find(flattened_indices_.begin(),
                            flattened_indices_.end(), parentIdx);
        if (it != flattened_indices_.end()) {
          selected_linear_row_ =
              static_cast<int>(std::distance(flattened_indices_.begin(), it));
          selectionModel()->setCurrentIndex(parentIdx);
          update();
        }
      }
    }
  }

  // ========================================================================
  // Dynamic Tree Flattening (Respects Collapsed States)
  // ========================================================================
  void rebuildFlattenedTree() {
    flattened_indices_.clear();
    if (model()) {
      flattenBranch(ModelIndex());
    }

    if (!flattened_indices_.empty()) {
      selected_linear_row_ =
          std::clamp(selected_linear_row_, 0,
                     static_cast<int>(flattened_indices_.size()) - 1);
      selectionModel()->setCurrentIndex(
          flattened_indices_[(size_t)selected_linear_row_]);
    }
  }

  // ========================================================================
  // Layout Compilation Pass
  // ========================================================================
  ftxui::Element render() override {
    if (!model() || !itemDelegate()) {
      return ftxui::text(
          "Missing tree data model or layout delegate bindings.");
    }

    std::vector<std::vector<ftxui::Element>> gridMatrix;
    ModelIndex activeIndex = selectionModel()->currentIndex();
    int totalCols = model()->columnCount();

    // Optional Heterogeneous Header Generation
    if (showHeaders()) {
      std::vector<ftxui::Element> headerRow;
      for (int c = 0; c < totalCols; ++c) {
        headerRow.emplace_back(headerDelegate()->createHeaderWidget(
            c, Orientation::Horizontal, model()));
        if (c < totalCols - 1) {
          headerRow.push_back(ftxui::separatorLight());
        }
      }
      gridMatrix.emplace_back(std::move(headerRow));
    }

    // Loop Through Visible Rows (Flattens 2D multi-column chunks)
    for (int i = 0; i < static_cast<int>(flattened_indices_.size()); ++i) {
      std::vector<ftxui::Element> uiRow;

      // Column 0 establishes structural alignment anchor
      ModelIndex indexCol0 = flattened_indices_[(size_t)i];
      int depth = calculateDepth(indexCol0);
      bool hasChildren = model()->hasChildren(indexCol0);
      bool isExpanded = expanded_nodes_.find(indexCol0.internalPointer()) !=
                        expanded_nodes_.end();

      // Construct Indentation Guides
      std::string indentStr = "";
      for (int d = 0; d < depth; ++d) {
        indentStr += "    ";
      }

      // Append Expansion Handle Indicator Icons ([+] / [-])
      std::string nodeHandle =
          hasChildren ? (isExpanded ? "[-] " : "[+] ") : "  • ";

      ftxui::Element structuralPrefix =
          ftxui::text(indentStr + nodeHandle) | ftxui::dim;
      ftxui::Element itemContent =
          itemDelegate()->createWidget(indexCol0, model());
      ftxui::Element anchorCell = ftxui::hbox({structuralPrefix, itemContent});
      // Determine row selection state globally for highlight synchronization
      bool isRowSelected =
          (indexCol0.row() == activeIndex.row() &&
           model()->parent(indexCol0) == model()->parent(activeIndex));

      // Highlight cell 0 if row is selected
      if (isRowSelected) {
        anchorCell = anchorCell | ftxui::bgcolor(ftxui::Color::Blue) |
                     ftxui::color(ftxui::Color::White) | ftxui::bold;
      }
      uiRow.emplace_back(anchorCell);

      // INTERACTION: Insert column divider after column 0
      if (totalCols > 1) {
        auto sep = ftxui::separatorLight();
        if (isRowSelected) {
          // Optional: Keeps your highlight line unbroken across the grid
          // separator block
          sep = sep | ftxui::bgcolor(ftxui::Color::Blue) |
                ftxui::color(ftxui::Color::White);
        }
        uiRow.emplace_back(sep);
      }

      // Process remaining data columns dynamically for the current item
      for (int c = 1; c < totalCols; ++c) {
        // Construct index coordinate relative to its current layout parent
        // sibling array
        ModelIndex targetColIdx =
            model()->index(indexCol0.row(), c, model()->parent(indexCol0));
        ftxui::Element sideCell =
            itemDelegate()->createWidget(targetColIdx, model());

        // Sync highlight across all elements on this line
        if (isRowSelected) {
          sideCell = sideCell | ftxui::bgcolor(ftxui::Color::Blue) |
                     ftxui::color(ftxui::Color::White) | ftxui::bold;
        }
        uiRow.emplace_back(sideCell);
        // INTERACTION: Inject a separator up to the second-to-last column
        // boundary track
        if (c < totalCols - 1) {
          auto sep = ftxui::separatorLight();
          if (isRowSelected) {
            sep = sep | ftxui::bgcolor(ftxui::Color::Blue) |
                  ftxui::color(ftxui::Color::White);
          }
          uiRow.push_back(sep);
        }
      }

      gridMatrix.emplace_back(std::move(uiRow));
    }

    // Return safely bound inside an auto-aligning grid layout matrix
    return ftxui::gridbox(std::move(gridMatrix)) | ftxui::border;
  }

 protected:
  void update() override {
    rebuildFlattenedTree();
    trigger_ftxui_refresh_();
  }

 private:
  void flattenBranch(const ModelIndex& parent) {
    int count = model()->rowCount(parent);
    for (int r = 0; r < count; ++r) {
      ModelIndex childIndex = model()->index(r, 0, parent);
      if (childIndex.isValid()) {
        flattened_indices_.push_back(childIndex);

        // CRITICAL CRITERIA: Only traverse sub-branches if the current node is
        // explicitly open
        bool isExpanded = expanded_nodes_.find(childIndex.internalPointer()) !=
                          expanded_nodes_.end();
        if (isExpanded && model()->hasChildren(childIndex)) {
          flattenBranch(childIndex);
        }
      }
    }
  }

  int calculateDepth(const ModelIndex& index) const {
    int depth = 0;
    ModelIndex currentParent = model()->parent(index);
    while (currentParent.isValid()) {
      depth++;
      currentParent = model()->parent(currentParent);
    }
    return depth;
  }

  bool isLastChild(const ModelIndex& index) const {
    ModelIndex parentIndex = model()->parent(index);
    int siblingCount = model()->rowCount(parentIndex);
    return index.row() == (siblingCount - 1);
  }
};

}  // namespace ftxmodel

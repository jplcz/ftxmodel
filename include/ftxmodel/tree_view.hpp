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
          flattened_indices_[selected_linear_row_]);
      update();
    }
  }

  void moveDown() {
    if (selected_linear_row_ <
        static_cast<int>(flattened_indices_.size()) - 1) {
      selected_linear_row_++;
      selectionModel()->setCurrentIndex(
          flattened_indices_[selected_linear_row_]);
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
          flattened_indices_[selected_linear_row_]);
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

    std::vector<ftxui::Element> rows;
    ModelIndex activeIndex = selectionModel()->currentIndex();

    // Optional Horizontal Header Generation
    if (showHeaders()) {
      // TreeView pulls directly from the base class effortlessly
      ftxui::Element headerWidget = headerDelegate()->createHeaderWidget(
          0, Orientation::Horizontal, model());
      rows.push_back(headerWidget | ftxui::xflex_shrink);
      rows.push_back(ftxui::separator());
    }

    // Loop Through Visible Nodes
    for (int i = 0; i < static_cast<int>(flattened_indices_.size()); ++i) {
      ModelIndex index = flattened_indices_[i];
      int depth = calculateDepth(index);
      bool isLast = isLastChild(index);
      bool hasChildren = model()->hasChildren(index);
      bool isExpanded = expanded_nodes_.find(index.internalPointer()) !=
                        expanded_nodes_.end();

      // Construct Indentation Guides
      std::string indentStr = "";
      for (int d = 0; d < depth; ++d) {
        indentStr += "    ";
      }

      // Append Expansion Handle Indicator Icons ([+] / [-])
      std::string nodeHandle = "";
      if (hasChildren) {
        nodeHandle = isExpanded ? "[-] " : "[+] ";
      } else {
        nodeHandle = "• ";  // Leaf item
      }

      ftxui::Element structuralPrefix =
          ftxui::text(indentStr + nodeHandle) | ftxui::dim;
      ftxui::Element itemContent = itemDelegate()->createWidget(index, model());
      ftxui::Element wholeRow =
          ftxui::hbox({structuralPrefix, itemContent}) | ftxui::flex_shrink;

      // Apply Selection Accent Highlights
      if (index == activeIndex) {
        wholeRow = wholeRow | ftxui::bgcolor(ftxui::Color::Blue) |
                   ftxui::color(ftxui::Color::White) | ftxui::bold;
      }

      rows.push_back(wholeRow);
    }

    return ftxui::vbox(std::move(rows)) | ftxui::border;
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

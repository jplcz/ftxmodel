#pragma once
#include <functional>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "abstract_item_view.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "header_delegate.hpp"

namespace ftxmodel {

class TreeView : public AbstractGridLikeItemView {
 private:
  std::vector<ModelIndex> flattened_indices_;

  // Tracks which nodes are currently expanded using their internal pointer
  // address
  std::set<UniqueNodeId> expanded_nodes_;
  int selected_linear_row_ = 0;

  // ========================================================================
  // Dynamic Column Width Processing Pass
  // ========================================================================
  [[nodiscard]] int calculateOptimalColumnWidth(const int colIndex) const {
    int maxColumnWidth = 0;

    // Account for Header text constraints if enabled
    if (showHorizontalHeaders()) {
      const std::any hData = model()->headerData(
          colIndex, Orientation::Horizontal, ItemRole::DisplayRole);

      const std::string header_text = AnyToStringTranslator::Translate(hData);

      if (!header_text.empty()) {
        // Correctly track visual wide-character layouts instead of processing
        // raw bytes
        ftxui::Dimensions header_bounds =
            UnicodeTextScaler::GetTextBounds(header_text);
        maxColumnWidth = std::max(maxColumnWidth, header_bounds.dimx);
      }
    }

    // Scan visible flattened layout rows
    for (const auto& indexCol0 : flattened_indices_) {
      if (colIndex == 0) {
        // Column 0 is a special composite: Indentation + Handle Prefix +
        // Delegate Data
        const int depth = calculateDepth(indexCol0);
        const int prefixWidth = (depth * 4) + 4;

        const ftxui::Dimensions cellHint =
            itemDelegate()->sizeHint(indexCol0, model());
        maxColumnWidth = std::max(maxColumnWidth, prefixWidth + cellHint.dimx);
      } else {
        // Side data columns pull their precise index coordinates relative to
        // their parent
        ModelIndex targetColIdx = model()->index(indexCol0.row(), colIndex,
                                                 model()->parent(indexCol0));
        if (targetColIdx.isValid()) {
          const ftxui::Dimensions cellHint =
              itemDelegate()->sizeHint(targetColIdx, model());
          maxColumnWidth = std::max(maxColumnWidth, cellHint.dimx);
        }
      }
    }

    return maxColumnWidth + 1;  // Append a 1-character padding buffer
  }

 public:
  TreeView() {
    highlightStyle()->setSelectionBehavior(SelectionBehavior::SelectRows);
  }

  void setModel(const std::shared_ptr<AbstractItemModel>& model) override {
    AbstractItemView::setModel(model);
    expanded_nodes_.clear();
    selected_linear_row_ = 0;
    rebuildFlattenedTree();
  }

  // ========================================================================
  // Keyboard Navigation Mapping
  // ========================================================================
  bool moveUp() {
    if (selected_linear_row_ > 0) {
      selected_linear_row_--;
      selectionModel()->setCurrentIndex(
          flattened_indices_[(size_t)selected_linear_row_]);
      update();
      return true;
    }
    return false;
  }

  bool moveDown() {
    if (selected_linear_row_ <
        static_cast<int>(flattened_indices_.size()) - 1) {
      selected_linear_row_++;

      const auto newIndex =
          flattened_indices_[static_cast<size_t>(selected_linear_row_)];

      selectionModel()->setCurrentIndex(newIndex);

      if (model()->canFetchMore(newIndex)) {
        model()->fetchMore(newIndex);
      }

      update();
      return true;
    }
    return false;
  }

  // Right Arrow: Expand current item if it has children
  bool moveRight() {
    const ModelIndex current = selectionModel()->currentIndex();
    if (!current.isValid()) {
      return false;
    }
    if (model()->canFetchMore(current)) {
      model()->fetchMore(current);
    }
    if (model()->hasChildren(current)) {
      const auto nodeId = current.uniqueId();
      if (!expanded_nodes_.contains(nodeId)) {
        expanded_nodes_.insert(nodeId);
        update();  // Force flattening recalculation
      }
      return true;
    }
    return false;
  }

  // Left Arrow: Collapse current item, or jump up to its parent if already
  // collapsed
  bool moveLeft() {
    ModelIndex current = selectionModel()->currentIndex();
    if (!current.isValid()) {
      return false;
    }

    const auto nodeId = current.uniqueId();
    auto nodeId_iter = expanded_nodes_.find(nodeId);

    if (nodeId_iter != expanded_nodes_.end()) {
      // Node is open, close it
      expanded_nodes_.erase(nodeId_iter);
      update();
      return true;
    } else {
      // Node is already closed, jump focus smoothly up to its structural parent
      ModelIndex parentIdx = model()->parent(current);
      if (parentIdx.isValid()) {
        const auto it = std::find(flattened_indices_.begin(),
                                  flattened_indices_.end(), parentIdx);
        if (it != flattened_indices_.end()) {
          selected_linear_row_ =
              static_cast<int>(std::distance(flattened_indices_.begin(), it));
          selectionModel()->setCurrentIndex(parentIdx);
          update();
        }
        return true;
      }
      return false;
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
          flattened_indices_[static_cast<size_t>(selected_linear_row_)]);
    }
  }

  // ========================================================================
  // Layout Compilation Pass
  // ========================================================================
  ftxui::Element OnRender() override {
    if (!model() || !itemDelegate()) {
      return ftxui::text(
          "Missing tree data model or layout delegate bindings.");
    }

    std::vector<std::vector<ftxui::Element>> gridMatrix;
    const ModelIndex activeIndex = selectionModel()->currentIndex();
    const int totalCols = model()->columnCount();
    const auto delegate = highlightStyle();

    // Compute dynamic uniform column widths based on visible nodes
    std::vector<int> colWidths(static_cast<size_t>(totalCols), 0);
    for (int c = 0; c < totalCols; ++c) {
      colWidths[static_cast<size_t>(c)] = calculateOptimalColumnWidth(c);
    }

    // Optional Heterogeneous Header Generation
    if (showHorizontalHeaders()) {
      std::vector<ftxui::Element> headerRow;
      if (showVerticalHeaders()) {
        headerRow.emplace_back(ftxui::text(" "));
      }
      for (int c = 0; c < totalCols; ++c) {
        headerRow.emplace_back(
            horizontalHeaderDelegate()->createHeaderWidget(
                c, Orientation::Horizontal, model()) |
            ftxui::size(ftxui::WIDTH, ftxui::GREATER_THAN,
                        colWidths[static_cast<size_t>(c)] - 1));
        if (c < totalCols - 1) {
          headerRow.emplace_back(ftxui::separatorLight());
        }
      }
      gridMatrix.emplace_back(std::move(headerRow));
    }

    // Loop Through Visible Rows (Flattens 2D multi-column chunks)
    for (int i = 0; i < static_cast<int>(flattened_indices_.size()); ++i) {
      std::vector<ftxui::Element> uiRow;

      if (showVerticalHeaders()) {
        uiRow.emplace_back(verticalHeaderDelegate()->createHeaderWidget(
            i, Orientation::Vertical, model()));
      }

      // Column 0 establishes structural alignment anchor
      const ModelIndex indexCol0 = flattened_indices_[static_cast<size_t>(i)];
      const int depth = calculateDepth(indexCol0);
      const bool hasChildren = model()->hasChildren(indexCol0);
      const bool canFetchMore = model()->canFetchMore(indexCol0);
      const bool isExpanded = expanded_nodes_.contains(indexCol0.uniqueId());

      // Construct Indentation Guides
      std::string indentStr = "";
      for (int d = 0; d < depth; ++d) {
        indentStr += " ";
      }

      // Append Expansion Handle Indicator Icons ([+] / [-])
      std::string nodeHandle =
          hasChildren || canFetchMore ? (isExpanded ? "[-] " : "[+] ") : "• ";

      ftxui::Element structuralPrefix =
          ftxui::text(indentStr + nodeHandle) | ftxui::dim;
      ftxui::Element itemContent =
          itemDelegate()->createWidget(indexCol0, model());

      ftxui::Element anchorCell =
          ftxui::hbox({structuralPrefix, itemContent}) |
          ftxui::size(ftxui::WIDTH, ftxui::GREATER_THAN, colWidths[0] - 1);

      // Determine row selection state globally for highlight synchronization
      const bool isRowSelected =
          (indexCol0.row() == activeIndex.row() &&
           model()->parent(indexCol0) == model()->parent(activeIndex));

      ViewStateFlags f = ViewNormal;

      if (Focused()) {
        f |= ViewFocused;
      }

      if (activeIndex.isValid()) {
        f |= ViewSelected;
      }

      if (isRowSelected) {
        f |= ViewIsSameRow;
      }

      if (activeIndex == indexCol0) {
        f |= ViewIsExactCell;
      }

      anchorCell = delegate->applyHighlight(std::move(anchorCell), f);
      anchorCell =
          delegate->applyGlobalFocus(std::move(anchorCell), f, indexCol0);

      uiRow.emplace_back(anchorCell);

      // Process remaining data columns dynamically for the current item
      for (int c = 1; c < totalCols; ++c) {
        // Construct index coordinate relative to its current layout parent
        // sibling array
        ModelIndex targetColIdx =
            model()->index(indexCol0.row(), c, model()->parent(indexCol0));

        // Insert column divider
        uiRow.emplace_back(
            delegate->applySeparatorHighlight(ftxui::separatorLight(), f));

        ftxui::Element sideCell =
            itemDelegate()->createWidget(targetColIdx, model());

        // Enforce side column uniform width restrictions
        sideCell =
            sideCell | ftxui::size(ftxui::WIDTH, ftxui::GREATER_THAN,
                                   colWidths[static_cast<size_t>(c)] - 1);

        sideCell = delegate->applyHighlight(std::move(sideCell), f);

        uiRow.emplace_back(sideCell);
      }

      gridMatrix.emplace_back(std::move(uiRow));
    }

    // Return safely bound inside an auto-aligning grid layout matrix
    return ftxui::gridbox(std::move(gridMatrix)) | ftxui::vscroll_indicator |
           ftxui::frame | ftxui::border;
  }

 protected:
  void update() override {
    rebuildFlattenedTree();
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
        bool isExpanded = expanded_nodes_.find(childIndex.uniqueId()) !=
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

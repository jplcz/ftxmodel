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
  std::function<void()> trigger_ftxui_refresh_;
  std::vector<ModelIndex> flattened_indices_;

  // Tracks which nodes are currently expanded using their internal pointer
  // address
  std::set<UniqueNodeId> expanded_nodes_;
  int selected_linear_row_ = 0;

  // ========================================================================
  // Dynamic Column Width Processing Pass
  // ========================================================================
  int calculateOptimalColumnWidth(int colIndex) const {
    int maxColumnWidth = 0;

    // Account for Header text constraints if enabled
    if (showHeaders()) {
      std::any hData = model()->headerData(colIndex, Orientation::Horizontal,
                                           ItemRole::DisplayRole);
      if (hData.type() == typeid(std::string)) {
        maxColumnWidth = std::max(
            maxColumnWidth,
            static_cast<int>(std::any_cast<std::string>(hData).length()));
      }
    }

    // Scan visible flattened layout rows
    for (const auto& indexCol0 : flattened_indices_) {
      if (colIndex == 0) {
        // Column 0 is a special composite: Indentation + Handle Prefix +
        // Delegate Data
        int depth = calculateDepth(indexCol0);
        int prefixWidth =
            (depth * 4) + 4;  // 4 spaces per depth level + 4 chars for "[+] "

        ftxui::Dimensions cellHint =
            itemDelegate()->sizeHint(indexCol0, model());
        maxColumnWidth = std::max(maxColumnWidth, prefixWidth + cellHint.dimx);
      } else {
        // Side data columns pull their precise index coordinates relative to
        // their parent
        ModelIndex targetColIdx = model()->index(indexCol0.row(), colIndex,
                                                 model()->parent(indexCol0));
        if (targetColIdx.isValid()) {
          ftxui::Dimensions cellHint =
              itemDelegate()->sizeHint(targetColIdx, model());
          maxColumnWidth = std::max(maxColumnWidth, cellHint.dimx);
        }
      }
    }

    return maxColumnWidth + 1;  // Append a 1-character padding buffer
  }

 public:
  explicit TreeView(std::function<void()> refreshCb)
      : trigger_ftxui_refresh_(std::move(std::move(refreshCb))) {}

  void setModel(AbstractItemModel* model) override {
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
      selectionModel()->setCurrentIndex(
          flattened_indices_[static_cast<size_t>(selected_linear_row_)]);
      update();
      return true;
    }
    return false;
  }

  // Right Arrow: Expand current item if it has children
  bool moveRight() {
    ModelIndex current = selectionModel()->currentIndex();
    if (current.isValid() && model()->hasChildren(current)) {
      const auto nodeId = current.uniqueId();
      if (expanded_nodes_.find(nodeId) == expanded_nodes_.end()) {
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
          flattened_indices_[(size_t)selected_linear_row_]);
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
    ModelIndex activeIndex = selectionModel()->currentIndex();
    int totalCols = model()->columnCount();

    // Compute dynamic uniform column widths based on visible nodes
    std::vector<int> colWidths((size_t)totalCols, 0);
    for (int c = 0; c < totalCols; ++c) {
      colWidths[(size_t)c] = calculateOptimalColumnWidth(c);
    }

    // Optional Heterogeneous Header Generation
    if (showHeaders()) {
      std::vector<ftxui::Element> headerRow;
      for (int c = 0; c < totalCols; ++c) {
        headerRow.emplace_back(headerDelegate()->createHeaderWidget(
                                   c, Orientation::Horizontal, model()) |
                               ftxui::size(ftxui::WIDTH, ftxui::GREATER_THAN,
                                           colWidths[(size_t)c] - 1));
        if (c < totalCols - 1) {
          headerRow.emplace_back(ftxui::separatorLight());
        }
      }
      gridMatrix.emplace_back(std::move(headerRow));
    }

    // Loop Through Visible Rows (Flattens 2D multi-column chunks)
    for (int i = 0; i < static_cast<int>(flattened_indices_.size()); ++i) {
      std::vector<ftxui::Element> uiRow;

      // Column 0 establishes structural alignment anchor
      const ModelIndex indexCol0 = flattened_indices_[(size_t)i];
      const int depth = calculateDepth(indexCol0);
      const bool hasChildren = model()->hasChildren(indexCol0);
      const bool isExpanded =
          expanded_nodes_.find(indexCol0.uniqueId()) != expanded_nodes_.end();

      // Construct Indentation Guides
      std::string indentStr = "";
      for (int d = 0; d < depth; ++d) {
        indentStr += " ";
      }

      // Append Expansion Handle Indicator Icons ([+] / [-])
      std::string nodeHandle =
          hasChildren ? (isExpanded ? "[-] " : "[+] ") : "• ";

      ftxui::Element structuralPrefix =
          ftxui::text(indentStr + nodeHandle) | ftxui::dim;
      ftxui::Element itemContent =
          itemDelegate()->createWidget(indexCol0, model());

      ftxui::Element anchorCell =
          ftxui::hbox({structuralPrefix, itemContent}) |
          ftxui::size(ftxui::WIDTH, ftxui::GREATER_THAN, colWidths[0] - 1);

      // Determine row selection state globally for highlight synchronization
      bool isRowSelected =
          (indexCol0.row() == activeIndex.row() &&
           model()->parent(indexCol0) == model()->parent(activeIndex));

      // Highlight cell 0 if row is selected
      if (isRowSelected) {
        anchorCell = anchorCell | ftxui::bgcolor(ftxui::Color::Blue) |
                     ftxui::color(ftxui::Color::White) | ftxui::bold;

        if (Focused()) {
          anchorCell = anchorCell | ftxui::focus;
        }
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

        // Enforce side column uniform width restrictions
        sideCell = sideCell | ftxui::size(ftxui::WIDTH, ftxui::GREATER_THAN,
                                          colWidths[(size_t)c] - 1);

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
          uiRow.emplace_back(sep);
        }
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

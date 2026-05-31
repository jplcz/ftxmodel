#pragma once
#include <algorithm>
#include <ftxmodel/abstract_item_view.hpp>
#include <ftxmodel/view_coordinate_mapper.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

namespace ftxmodel {

class ScrollableTableView : public AbstractGridLikeItemView {
 private:
  // Structural model index tracking viewframes
  int m_scroll_row = 0;
  int m_scroll_col = 0;

  // Viewport capacity dimension allocations overridable by user configuration
  int m_user_max_rows = 15;
  int m_user_max_cols = 6;

  // Cache buffers for terminal reflection updates
  mutable ftxui::Box m_viewport_box;
  ViewCoordinateMapper m_coord_mapper;

 public:
  ScrollableTableView() = default;
  ~ScrollableTableView() override = default;

  // --- User Viewport Customization Overrides ---
  void setViewportDimensions(const int max_rows, const int max_cols) noexcept {
    m_user_max_rows = std::max(1, max_rows);
    m_user_max_cols = std::max(1, max_cols);
    this->update();
  }

  ftxui::Element OnRender() override {
    if (!model()) {
      return ftxui::text("No Model Attached") | ftxui::center | ftxui::border;
    }

    if (rowCount() == 0 || columnCount() == 0) {
      return ftxui::text("No Data Fields Available") | ftxui::center |
             ftxui::border;
    }

    const int total_rows = rowCount();
    const int total_cols = columnCount();
    const ModelIndex current_focus = selectionModel()->currentIndex();
    const bool is_currently_focused =
        this->Focused();  // True only if keyboard active on this widget

    // Sync viewport step boundaries before parsing elements
    ensureIndexVisible(current_focus.isValid() ? current_focus.row() : 0,
                       current_focus.isValid() ? current_focus.column() : 0);

    m_coord_mapper.reset();
    std::vector<std::vector<ftxui::Element>> grid_matrix;

    // Calculate structural viewport cutoff limits safely
    const int end_row = std::min(total_rows, m_scroll_row + m_user_max_rows);
    const int end_col = std::min(total_cols, m_scroll_col + m_user_max_cols);

    // --- A: HORIZONTAL COLUMN HEADERS ---
    if (showHorizontalHeaders() && horizontalHeaderDelegate()) {
      std::vector<ftxui::Element> header_row;

      // Top-Left Alignment Corner Anchor Spacer Block
      if (showVerticalHeaders() && verticalHeaderDelegate()) {
        header_row.push_back(ftxui::text(" "));
      }

      for (int c = m_scroll_col; c < end_col; ++c) {
        if (c > m_scroll_col) {
          header_row.emplace_back(ftxui::separator());
        }

        header_row.emplace_back(horizontalHeaderDelegate()->createHeaderWidget(
            c, Orientation::Horizontal, model()));
      }

      grid_matrix.emplace_back(std::move(header_row));
    }

    // --- B: CELL MATRIX GENERATOR LOOP ---
    for (int r = m_scroll_row; r < end_row; ++r) {
      std::vector<ftxui::Element> body_row;

      // Draw Left Margin Row Identifiers (IF ENABLED)
      if (showVerticalHeaders() && verticalHeaderDelegate()) {
        // Target the specialized vertical delegate explicitly
        ftxui::Element row_header =
            verticalHeaderDelegate()->createHeaderWidget(
                r, Orientation::Vertical, model());

        if (is_currently_focused && current_focus.isValid() &&
            current_focus.row() == r) {
          row_header =
              row_header | ftxui::bgcolor(ftxui::Color::DarkBlue) | ftxui::bold;
        }
        body_row.push_back(std::move(row_header));
      }

      // Draw Inner Workspace Data Elements
      for (int c = m_scroll_col; c < end_col; ++c) {
        if (c > m_scroll_col) {
          body_row.push_back(ftxui::separator());
        }

        ModelIndex cell_idx = model()->index(r, c);
        ftxui::Element cell_widget;

        if (cell_idx.isValid() && itemDelegate()) {
          // Generate the raw data presentation widget from your delegate
          cell_widget = itemDelegate()->createWidget(cell_idx, model());

          // Compute state flags dynamically
          ViewStateFlags state = ViewNormal;

          if (current_focus.isValid() && current_focus.row() == r &&
              current_focus.column() == c) {
            state |= ViewSelected;
          }

          if (is_currently_focused) {
            state |= ViewFocused;
          }

          if (current_focus.isValid() && current_focus.row() == r) {
            state |= ViewIsSameRow;
            if (current_focus.column() == c) {
              state |= ViewIsExactCell;
            }
          }

          // Apply selection style decorator if available
          if (highlightStyle()) {
            cell_widget =
                highlightStyle()->applyHighlight(std::move(cell_widget), state);
          }

          // Register stable coordinate mapper bindings
          cell_widget = std::move(cell_widget) |
                        ftxui::reflect(m_coord_mapper.registerCell(cell_idx));
        } else {
          cell_widget = ftxui::text("-");
        }

        // Native Pointer-Stable Structural Map Reflection Registration
        if (cell_idx.isValid()) {
          cell_widget = cell_widget |
                        ftxui::reflect(m_coord_mapper.registerCell(cell_idx));
        }

        body_row.push_back(cell_widget);
      }
      grid_matrix.push_back(std::move(body_row));
    }

    // Hand layout execution directly down to the 2D gridbox allocation engine
    return ftxui::gridbox(std::move(grid_matrix)) | ftxui::borderLight |
           ftxui::reflect(m_viewport_box);
  }

  bool OnEvent(ftxui::Event event) override {
    if (!model() || !selectionModel() || rowCount() == 0 ||
        columnCount() == 0) {
      return false;
    }

    // --- MOUSE PIPELINE ---
    if (event.is_mouse()) {
      const auto mouse = event.mouse();

      // Handle Scroll Wheel Increments Independently
      if (mouse.button == ftxui::Mouse::WheelUp) {
        m_scroll_row = std::max(0, m_scroll_row - 3);
        return true;
      }
      if (mouse.button == ftxui::Mouse::WheelDown) {
        m_scroll_row = std::min(rowCount() - 1, m_scroll_row + 3);
        return true;
      }

      // Precise Click-to-Index Translation via Pointer-Stable Mapper
      if (mouse.button == ftxui::Mouse::Left &&
          (mouse.motion == ftxui::Mouse::Pressed ||
           mouse.motion == ftxui::Mouse::Moved)) {
        if (const auto matched_index =
                m_coord_mapper.findIndexAt(mouse.x, mouse.y)) {
          if (matched_index->isValid() &&
              (matched_index->flags() & ItemFlag::ItemIsSelectable)) {
            selectionModel()->setCurrentIndex(*matched_index);
            this->TakeFocus();  // Grab operational focus state priority
            return true;
          }
        }
      }
      return false;
    }
    // --- KEYBOARD CONFIGURATION NAVIGATOR ---
    ModelIndex current = selectionModel()->currentIndex();
    int r = current.isValid() ? current.row() : 0;
    int c = current.isValid() ? current.column() : 0;

    if (event == ftxui::Event::ArrowUp) {
      r = std::max(0, r - 1);
    } else if (event == ftxui::Event::ArrowDown) {
      r = std::min(rowCount() - 1, r + 1);
    } else if (event == ftxui::Event::ArrowLeft) {
      c = std::max(0, c - 1);
    } else if (event == ftxui::Event::ArrowRight) {
      c = std::min(columnCount() - 1, c + 1);
    } else if (event == ftxui::Event::PageUp) {
      r = std::max(0, r - m_user_max_rows);
    } else if (event == ftxui::Event::PageDown) {
      r = std::min(rowCount() - 1, r + m_user_max_rows);
    } else if (event == ftxui::Event::Home) {
      r = 0;
      c = 0;
    } else if (event == ftxui::Event::End) {
      r = rowCount() - 1;
    } else {
      return false;
    }

    const ModelIndex target_idx = model()->index(r, c);
    if (target_idx.isValid()) {
      selectionModel()->setCurrentIndex(target_idx);
      ensureIndexVisible(r, c);
    }
    return true;
  }

 protected:
  void update() override {
    // Handled by parent event architecture to request repaint loops
  }

 private:
  // --- Pure Index-Counting Visibility Control Pass ---
  void ensureIndexVisible(const int target_row, const int target_col) {
    // Slide Vertical Tracking Frame Bounds
    if (target_row < m_scroll_row) {
      m_scroll_row = target_row;
    } else if (target_row >= m_scroll_row + m_user_max_rows) {
      m_scroll_row = target_row - m_user_max_rows + 1;
    }

    // Slide Horizontal Tracking Frame Bounds (Pure Structural Index-Shifting)
    if (target_col < m_scroll_col) {
      m_scroll_col = target_col;
    } else if (target_col >= m_scroll_col + m_user_max_cols) {
      m_scroll_col = target_col - m_user_max_cols + 1;
    }

    // Anchor outputs within valid structural bounds boundaries
    m_scroll_row = std::max(0, std::min(m_scroll_row, rowCount() - 1));
    m_scroll_col = std::max(0, std::min(m_scroll_col, columnCount() - 1));
  }

  [[nodiscard]] int rowCount() const {
    return model() ? model()->rowCount() : 0;
  }
  [[nodiscard]] int columnCount() const {
    return model() ? model()->columnCount() : 0;
  }
};

}  // namespace ftxmodel

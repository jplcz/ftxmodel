#pragma once
#include <ftxui/dom/elements.hpp>

namespace ftxmodel {

enum ViewStateFlag : uint8_t {
  ViewNormal = 0,
  ViewSelected = 1 << 0,  // General tracking selection state
  ViewFocused = 1 << 1,   // Component possesses active keyboard focus

  // Spatial relational descriptors for 2D Grid layouts
  ViewIsExactCell =
      1 << 2,  // The element is the precise cursor coordinate index
  ViewIsSameRow =
      1 << 3  // The element shares the row line with the cursor coordinate
};

using ViewStateFlags = uint8_t;

enum class SelectionBehavior : uint8_t {
  SelectRows,  // Clicking/focusing an item highlights the entire horizontal
               // record track
  SelectCells  // Highlight stays strictly pinned to the active intersection
               // cell coordinate
};

class SelectionHighlightStyle {
 private:
  SelectionBehavior m_behavior = SelectionBehavior::SelectCells;

 public:
  virtual ~SelectionHighlightStyle() = default;

  void setSelectionBehavior(const SelectionBehavior behavior) noexcept {
    m_behavior = behavior;
  }

  [[nodiscard]] SelectionBehavior selectionBehavior() const noexcept {
    return m_behavior;
  }

  /**
   * @brief Wraps a pre-rendered cell element with selection color filters.
   */
  virtual ftxui::Element applyHighlight(ftxui::Element content,
                                        const ViewStateFlags state) const {
    bool qualifies_for_selection = false;

    if (m_behavior == SelectionBehavior::SelectRows) {
      qualifies_for_selection = (state & ViewIsSameRow);
    } else {
      qualifies_for_selection = (state & ViewIsExactCell);
    }

    // Fast fail: If the cell doesn't qualify structurally, strip styling rules
    // immediately
    if (!qualifies_for_selection) {
      return content;
    }

    // Evaluate rendering themes based on active view window focus parameters
    if (state & ViewFocused) {
      // If we are selecting rows, provide visual context separation between the
      // exact cursor focus cell and the secondary column tracking fragments on
      // that line.
      if (m_behavior == SelectionBehavior::SelectRows &&
          !(state & ViewIsExactCell)) {
        return std::move(content) | ftxui::bgcolor(ftxui::Color::DarkBlue) |
               ftxui::color(ftxui::Color::White);
      }

      // Precision focus highlight style rule cross-over block
      return std::move(content) | ftxui::bgcolor(ftxui::Color::BlueLight) |
             ftxui::color(ftxui::Color::Black) | ftxui::bold;
    } else if (state & ViewSelected) {
      // Row selection fallback tracking theme when component focus is blurred
      // out
      return std::move(content) | ftxui::bgcolor(ftxui::Color::GrayDark);
    }

    return content;
  }
};

}  // namespace ftxmodel

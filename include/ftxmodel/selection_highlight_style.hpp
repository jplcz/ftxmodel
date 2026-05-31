#pragma once
#include <ftxui/dom/elements.hpp>

namespace ftxmodel {

enum ViewStateFlag : uint8_t {
  ViewNormal = 0,
  ViewSelected = 1 << 0,  // Row/Cell is selected
  ViewFocused = 1 << 1  // View component currently holds active keyboard focus
};
using ViewStateFlags = uint8_t;

class SelectionHighlightStyle {
 public:
  virtual ~SelectionHighlightStyle() = default;

  /**
   * @brief Wraps a pre-rendered cell element with selection color filters.
   */
  virtual ftxui::Element applyHighlight(ftxui::Element content,
                                        const ViewStateFlags state) const {
    if ((state & ViewSelected) && (state & ViewFocused)) {
      // Complete active focus highlight style across the grid lane
      return std::move(content) | ftxui::bgcolor(ftxui::Color::BlueLight) |
             ftxui::color(ftxui::Color::Black) | ftxui::bold;
    } else if (state & ViewSelected) {
      // Row selection highlight when the table itself is blurred/out-of-focus
      return std::move(content) | ftxui::bgcolor(ftxui::Color::GrayDark);
    }
    return content;  // Return raw content if unselected
  }
};

}  // namespace ftxmodel

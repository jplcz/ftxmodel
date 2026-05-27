#pragma once
#include <algorithm>
#include <string>
#include <string_view>
#include "ftxui/screen/terminal.hpp"

namespace ftxmodel {

class TextSizeConstraints {
 private:
  int min_width_ = 0;
  int max_width_ = -1;  // -1 indicates uncapped/unlimited width
  int preferred_width_ =
      -1;  // -1 indicates fallback to natural data string length

 public:
  TextSizeConstraints() = default;

  TextSizeConstraints(int minW, int maxW, int prefW = -1)
      : min_width_(minW), max_width_(maxW), preferred_width_(prefW) {}

  // --- Getters & Setters ---
  void setMinWidth(int minW) { min_width_ = minW; }
  int minWidth() const { return min_width_; }

  void setMaxWidth(int maxW) { max_width_ = maxW; }
  int maxWidth() const { return max_width_; }

  void setPreferredWidth(int prefW) { preferred_width_ = prefW; }
  int preferredWidth() const { return preferred_width_; }

  // ==========================================================================
  // Convenience Methods for Layout Delegates
  // ==========================================================================

  // Computes the definitive layout width footprint for sizeHint queries
  int calculateWidthHint(const std::string_view& rawText) const {
    int target = preferred_width_ >= 0 ? preferred_width_
                                       : static_cast<int>(rawText.length());

    // Apply maximum caps if explicitly set
    if (max_width_ >= 0) {
      target = std::min(target, max_width_);
    }

    // Enforce the layout floor
    return std::max(min_width_, target);
  }

  // Clips a text string and handles ellipsis formatting if it exceeds
  // constraints
  std::string applyBounds(const std::string_view& rawText) const {
    std::string text{rawText};

    // Handle Truncation (Max Width Boundary Check)
    if (max_width_ >= 0 && static_cast<int>(text.length()) > max_width_) {
      if (max_width_ > 3) {
        text = text.substr(0, static_cast<size_t>(max_width_ - 3)) + "...";
      } else {
        text = text.substr(0, static_cast<size_t>(max_width_));
      }
    }

    // Handle Padding (Minimum Width Boundary Check)
    if (static_cast<int>(text.length()) < min_width_) {
      text.append(static_cast<size_t>(min_width_ - (int)text.length()), ' ');
    }

    return text;
  }
};

}  // namespace ftxmodel

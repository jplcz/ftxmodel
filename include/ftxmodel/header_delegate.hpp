#pragma once
#include <any>
#include <string>
#include "abstract_item_model.hpp"
#include "ftxui/dom/elements.hpp"
#include "unicode_text_scaler.hpp"

namespace ftxmodel {

class HeaderDelegate {
 public:
  virtual ~HeaderDelegate() = default;

  // Generates a visual component for a specific column or row header section
  virtual ftxui::Element createHeaderWidget(
      int section,
      Orientation orientation,
      const AbstractItemModel* model) const = 0;
};

class AdvancedHeaderDelegate : public HeaderDelegate {
 private:
  ftxui::Color horizontal_bg_ = ftxui::Color::GrayDark;
  ftxui::Color horizontal_fg_ = ftxui::Color::White;
  // Formatting control for column headers
  FormattingOptions horizontal_options_;
  // Formatting control for row index headers
  FormattingOptions vertical_options_;

 public:
  AdvancedHeaderDelegate() {
    // Default Configuration for Column Headers: Centered, with safe
    // wrapping/clamping
    horizontal_options_.alignment = Alignment::Center;
    horizontal_options_.wrap_lines =
        false;  // Truncate long header words with ellipsis
    horizontal_options_.ellipsis = "…";

    // Default Configuration for Row Side Headers: Right-aligned, minimal stable
    // padding
    vertical_options_.alignment = Alignment::Right;
    vertical_options_.min_width =
        4;  // Ensures " 9 │" and "10 │" occupy identical spatial layouts
  }

  AdvancedHeaderDelegate(ftxui::Color hBg,
                         ftxui::Color hFg,
                         FormattingOptions hOpts = {},
                         FormattingOptions vOpts = {})
      : horizontal_bg_(hBg),
        horizontal_fg_(hFg),
        horizontal_options_(hOpts),
        vertical_options_(vOpts) {}

  // Expose configuration accessors for modifying header styles on the fly
  [[nodiscard]] FormattingOptions& horizontalOptions() noexcept {
    return horizontal_options_;
  }
  [[nodiscard]] FormattingOptions& verticalOptions() noexcept {
    return vertical_options_;
  }

  ftxui::Element createHeaderWidget(
      const int section,
      const Orientation orientation,
      const AbstractItemModel* model) const override {
    // Query the model's standard headerData implementation
    std::any rawHeader =
        model->headerData(section, orientation, ItemRole::DisplayRole);

    std::string headerText;
    if (rawHeader.type() == typeid(std::string)) {
      headerText = std::any_cast<std::string>(rawHeader);
    } else {
      headerText =
          std::to_string(section);  // Fallback to raw numeric index string
    }

    // 2. Process and Render Based on Layout Orientation
    if (orientation == Orientation::Horizontal) {
      // Pass text through your layout engine using column constraints
      std::string formatted_header =
          UnicodeTextScaler::FormatText(headerText, horizontal_options_);

      // Enforce the layout verbatim without conflicting FTXUI alignment
      // decorators
      return ftxui::text(std::move(formatted_header)) | ftxui::bold |
             ftxui::bgcolor(horizontal_bg_) | ftxui::color(horizontal_fg_);
    } else {
      // Pass row numbers through your layout engine using vertical constraints
      std::string formatted_row =
          UnicodeTextScaler::FormatText(headerText, vertical_options_);

      // Append the clean graphical grid separator glyph onto your padded string
      // block
      return ftxui::text(std::move(formatted_row) + " │") | ftxui::dim;
    }
  }
};

}  // namespace ftxmodel

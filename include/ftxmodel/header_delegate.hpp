#pragma once
#include <any>
#include <string>
#include <utility>
#include "abstract_item_model.hpp"
#include "any_to_string.hpp"
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
 public:
  /**
   * @brief Signature for header text decorators.
   * Modifies the header text in-place based on section, orientation, and model
   * context.
   */
  using HeaderDecorator = std::function<void(std::string& text,
                                             int section,
                                             Orientation orientation,
                                             const AbstractItemModel* model)>;

  using ElementDecorator =
      std::function<ftxui::Element(ftxui::Element,
                                   int,
                                   Orientation,
                                   const AbstractItemModel*)>;

 private:
  ftxui::Color horizontal_bg_ = ftxui::Color::GrayDark;
  ftxui::Color horizontal_fg_ = ftxui::Color::White;
  // Formatting control for column headers
  FormattingOptions horizontal_options_;
  // Formatting control for row index headers
  FormattingOptions vertical_options_;
  // Pipeline of functional extensions applied sequentially during rendering
  // passes
  std::vector<HeaderDecorator> decorators_;
  std::vector<ElementDecorator> element_decorators_;

 public:
  static ElementDecorator makeStandardElementDecorator(
      AdvancedHeaderDelegate* self) {
    return [self](const ftxui::Element& element, int,
                  const Orientation orientation, const AbstractItemModel*) {
      if (orientation == Orientation::Horizontal) {
        return element | ftxui::bold | ftxui::bgcolor(self->horizontal_bg_) |
               ftxui::color(self->horizontal_fg_);
      } else {
        return element | ftxui::dim;
      }
    };
  }

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

    addElementDecorator(makeStandardElementDecorator(this));
  }

  AdvancedHeaderDelegate(ftxui::Color hBg,
                         ftxui::Color hFg,
                         FormattingOptions hOpts = {},
                         FormattingOptions vOpts = {})
      : horizontal_bg_(hBg),
        horizontal_fg_(hFg),
        horizontal_options_(std::move(hOpts)),
        vertical_options_(std::move(vOpts)) {
    addElementDecorator(makeStandardElementDecorator(this));
  }

  // Expose configuration accessors for modifying header styles on the fly
  [[nodiscard]] FormattingOptions& horizontalOptions() noexcept {
    return horizontal_options_;
  }

  [[nodiscard]] FormattingOptions& verticalOptions() noexcept {
    return vertical_options_;
  }

  [[nodiscard]] const FormattingOptions& horizontalOptions() const noexcept {
    return horizontal_options_;
  }

  [[nodiscard]] const FormattingOptions& verticalOptions() const noexcept {
    return vertical_options_;
  }

  [[nodiscard]] ftxui::Color horizontalBg() const { return horizontal_bg_; }

  void setHorizontalBg(const ftxui::Color& horizontal_bg) {
    horizontal_bg_ = horizontal_bg;
  }

  [[nodiscard]] ftxui::Color horizontalFg() const { return horizontal_fg_; }

  void setHorizontalFg(const ftxui::Color& horizontal_fg) {
    horizontal_fg_ = horizontal_fg;
  }

  /**
   * @brief Appends a custom text decorator to the header rendering pipeline.
   */
  void addDecorator(HeaderDecorator decorator) {
    decorators_.emplace_back(std::move(decorator));
  }

  /**
   * @brief Clears all registered decorators from the pipeline.
   */
  void clearDecorators() noexcept { decorators_.clear(); }

  void addElementDecorator(ElementDecorator ed) {
    element_decorators_.emplace_back(std::move(ed));
  }

  ftxui::Element createHeaderWidget(
      const int section,
      const Orientation orientation,
      const AbstractItemModel* model) const override {
    // Query the model's standard headerData implementation
    const std::any rawHeader =
        model->headerData(section, orientation, ItemRole::DisplayRole);

    std::string headerText =
        AnyToStringTranslator::Translate(rawHeader, std::to_string(section));

    for (const auto& decorate : decorators_) {
      decorate(headerText, section, orientation, model);
    }

    ftxui::Element header_element;

    // Process and Render Based on Layout Orientation
    if (orientation == Orientation::Horizontal) {
      // Pass text through your layout engine using column constraints
      std::string formatted_header =
          UnicodeTextScaler::FormatText(headerText, horizontal_options_);

      // Enforce the layout verbatim without conflicting FTXUI alignment
      // decorators
      header_element = ftxui::text(std::move(formatted_header));
    } else {
      // Pass row numbers through your layout engine using vertical constraints
      std::string formatted_row =
          UnicodeTextScaler::FormatText(headerText, vertical_options_);

      // Append the clean graphical grid separator glyph onto your padded string
      // block
      header_element = ftxui::text(std::move(formatted_row) + " │");
    }

    for (const auto& ed : element_decorators_) {
      header_element =
          ed(std::move(header_element), section, orientation, model);
    }

    return header_element;
  }
};

}  // namespace ftxmodel

#pragma once
#include <any>
#include <string>
#include "abstract_item_model.hpp"
#include "ftxui/dom/elements.hpp"

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

 public:
  AdvancedHeaderDelegate() = default;

  AdvancedHeaderDelegate(ftxui::Color hBg, ftxui::Color hFg)
      : horizontal_bg_(hBg), horizontal_fg_(hFg) {}

  ftxui::Element createHeaderWidget(
      int section,
      Orientation orientation,
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

    if (orientation == Orientation::Horizontal) {
      // Horizontal table column header styling: Bold, centered, with
      // customizable background
      return ftxui::text(" " + headerText + " ") | ftxui::bold | ftxui::center |
             ftxui::bgcolor(horizontal_bg_) | ftxui::color(horizontal_fg_);
    } else {
      // Vertical table row numbers/side header styling: Dimmed, aligned right
      return ftxui::text(headerText + " │") | ftxui::dim | ftxui::align_right;
    }
  }
};

}  // namespace ftxmodel

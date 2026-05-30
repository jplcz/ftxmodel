#pragma once
#include <algorithm>
#include <any>
#include <ftxmodel/abstract_item_model.hpp>
#include <ftxui/dom/elements.hpp>
#include <memory>
#include <string>
#include <vector>
#include "text_size_constraints.hpp"
#include "unicode_text_scaler.hpp"

namespace ftxmodel {

class ItemDelegate {
 public:
  virtual ~ItemDelegate() = default;

  // Generates a visual component for a specific model cell
  virtual ftxui::Element createWidget(const ModelIndex& index,
                                      const AbstractItemModel* model) const = 0;

  // Returns the preferred width and height dimensions for a given cell.
  // We use standard ftxui::Dimensions (which wraps width and height ints).
  virtual ftxui::Dimensions sizeHint(const ModelIndex& index,
                                     const AbstractItemModel* model) const {
    std::ignore = index;
    std::ignore = model;

    // Default fallback: 0 means "no opinion / let the layout engine decide
    // auto-scaling"
    return ftxui::Dimensions{0, 1};
  }
};

class StyledTextDelegate : public ItemDelegate {
 public:
  enum class Alignment { Left, Center, Right };

  StyledTextDelegate(Alignment align = Alignment::Left,
                     ftxui::Color color = ftxui::Color::White,
                     TextSizeConstraints constraints = {})
      : alignment_(align), text_color_(color), constraints_(constraints) {}

  // Expose the helper profile directly to let developers modify bounds on the
  // fly
  TextSizeConstraints& constraints() { return constraints_; }
  const TextSizeConstraints& constraints() const { return constraints_; }

  void setConstraints(const TextSizeConstraints& constraints) {
    constraints_ = constraints;
  }

  ftxui::Element createWidget(const ModelIndex& index,
                              const AbstractItemModel* model) const override {
    // Convenience call to automatically apply truncation and padding bounds
    std::string processedText =
        constraints_.applyBounds(model->textData(index));

    auto element =
        ftxui::text(std::move(processedText)) | ftxui::color(text_color_);

    // Apply visual alignment properties
    switch (alignment_) {
      case Alignment::Center:
        return element | ftxui::center;
      case Alignment::Right:
        return element | ftxui::align_right;
      case Alignment::Left:
      default:
        return element;
    }
  }

  // Returns the width matching the string length, defaulting to 1 height block
  ftxui::Dimensions sizeHint(const ModelIndex& index,
                             const AbstractItemModel* model) const override {
    // Convenience call to resolve aggregate cell spatial demands
    int finalWidth = constraints_.calculateWidthHint(model->textData(index));
    return ftxui::Dimensions{std::max(1, finalWidth), 1};
  }

 private:
  Alignment alignment_;
  ftxui::Color text_color_;
  TextSizeConstraints constraints_;  // Dedicated layout configuration profile
};

class CheckBoxDelegate : public ItemDelegate {
 public:
  ftxui::Element createWidget(const ModelIndex& index,
                              const AbstractItemModel* model) const override {
    auto rawData = model->data(index, ItemRole::DisplayRole);

    bool checked = false;
    if (rawData.type() == typeid(bool)) {
      checked = std::any_cast<bool>(rawData);
    } else if (rawData.type() == typeid(int)) {
      checked = std::any_cast<int>(rawData) != 0;
    }

    // Generate clean structural bracket tags for terminal checkboxes
    if (checked) {
      return ftxui::hbox({ftxui::text("[") | ftxui::dim,
                          ftxui::text("X") | ftxui::color(ftxui::Color::Green) |
                              ftxui::bold,
                          ftxui::text("]") | ftxui::dim}) |
             ftxui::center;
    } else {
      return ftxui::text("[ ]") | ftxui::dim | ftxui::center;
    }
  }

  // Toggles are consistently 3 characters wide and 1 row tall
  ftxui::Dimensions sizeHint(const ModelIndex&,
                             const AbstractItemModel*) const override {
    return ftxui::Dimensions{3, 1};
  }
};

class ProgressBarDelegate : public ItemDelegate {
 private:
  float max_value_;
  ftxui::Color bar_color_;

  // New configurable layout parameters
  int text_width_;
  int gauge_width_;

 public:
  explicit ProgressBarDelegate(float maxValue = 100.0f,
                               ftxui::Color barColor = ftxui::Color::Cyan,
                               int textWidth = 5,
                               int gaugeWidth = 12)
      : max_value_(maxValue),
        bar_color_(barColor),
        text_width_(textWidth),
        gauge_width_(gaugeWidth) {}

  ftxui::Element createWidget(const ModelIndex& index,
                              const AbstractItemModel* model) const override {
    auto rawData = model->data(index, ItemRole::DisplayRole);

    float currentVal = 0.0f;
    if (rawData.type() == typeid(float)) {
      currentVal = std::any_cast<float>(rawData);
    } else if (rawData.type() == typeid(double)) {
      currentVal = static_cast<float>(std::any_cast<double>(rawData));
    } else if (rawData.type() == typeid(int)) {
      currentVal = static_cast<float>(std::any_cast<int>(rawData));
    }

    float percentage = (max_value_ > 0.0f) ? (currentVal / max_value_) : 0.0f;
    percentage = std::clamp(percentage, 0.0f, 1.0f);

    std::string percentageText =
        std::to_string(static_cast<int>(percentage * 100)) + "%";

    // Build the visual block using our configurable member variables
    return ftxui::hbox(
        {ftxui::text(percentageText) |
             ftxui::size(ftxui::WIDTH, ftxui::EQUAL, text_width_),
         ftxui::gauge(percentage) | ftxui::color(bar_color_) |
             ftxui::size(ftxui::WIDTH, ftxui::EQUAL, gauge_width_)});
  }

  // Calculates the aggregate cell demand on demand dynamically (Text + Gauge)
  ftxui::Dimensions sizeHint(const ModelIndex&,
                             const AbstractItemModel*) const override {
    return ftxui::Dimensions{text_width_ + gauge_width_, 1};
  }
};

}  // namespace ftxmodel

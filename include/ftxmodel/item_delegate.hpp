#pragma once
#include <any>
#include <ftxmodel/abstract_item_model.hpp>
#include <ftxui/dom/elements.hpp>
#include <memory>
#include <string>
#include <vector>

namespace ftxmodel {

class ItemDelegate {
 public:
  virtual ~ItemDelegate() = default;

  // Generates a visual component for a specific model cell
  virtual ftxui::Element createWidget(const ModelIndex& index,
                                      const AbstractItemModel* model) const = 0;
};

class StyledTextDelegate : public ItemDelegate {
 public:
  enum class Alignment { Left, Center, Right };

  StyledTextDelegate(Alignment align = Alignment::Left,
                     ftxui::Color color = ftxui::Color::White)
      : alignment_(align), text_color_(color) {}

  ftxui::Element createWidget(const ModelIndex& index,
                              const AbstractItemModel* model) const override {
    auto element =
        ftxui::text(model->textData(index)) | ftxui::color(text_color_);

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

 private:
  Alignment alignment_;
  ftxui::Color text_color_;
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
};

class ProgressBarDelegate : public ItemDelegate {
 private:
  float max_value_;
  ftxui::Color bar_color_;

 public:
  explicit ProgressBarDelegate(float maxValue = 100.0f,
                               ftxui::Color barColor = ftxui::Color::Cyan)
      : max_value_(maxValue), bar_color_(barColor) {}

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

    // Calculate filling percentage bound safely between 0.0 and 1.0
    float percentage = (max_value_ > 0.0f) ? (currentVal / max_value_) : 0.0f;
    percentage = std::clamp(percentage, 0.0f, 1.0f);

    // Build a split view: numeric readout text paired with a graphical
    // rendering engine
    std::string percentageText =
        std::to_string(static_cast<int>(percentage * 100)) + "%";

    return ftxui::hbox({ftxui::text(percentageText) |
                            ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 5),
                        ftxui::gauge(percentage) | ftxui::color(bar_color_) |
                            ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 12)});
  }
};

}  // namespace ftxmodel

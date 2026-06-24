#pragma once
#include <map>
#include <memory>
#include <typeindex>
#include "ftxui/dom/elements.hpp"
#include "item_delegate.hpp"

namespace ftxmodel {

class MultiColumnRouterDelegate : public ItemDelegate {
 private:
  std::map<int, std::shared_ptr<ItemDelegate>> column_delegates_;
  std::shared_ptr<ItemDelegate> default_fallback_delegate_ = nullptr;

 public:
  MultiColumnRouterDelegate() = default;

  // Fluent API to chain column registrations sequentially
  MultiColumnRouterDelegate& registerColumn(
      const int column,
      const std::shared_ptr<ItemDelegate>& delegate) {
    column_delegates_[column] = delegate;
    return *this;
  }

  // Set an optional fallback delegate for any column that isn't explicitly
  // registered
  void setDefaultFallback(std::shared_ptr<ItemDelegate> delegate) {
    default_fallback_delegate_ = std::move(delegate);
  }

  // ==========================================================================
  // Unified Visual Widget Routing Pass
  // ==========================================================================
  ftxui::Element createWidget(const ModelIndex& index,
                              const AbstractItemModel* model) const override {
    if (const auto it = column_delegates_.find(index.column());
        it != column_delegates_.end() && it->second) {
      return it->second->createWidget(index, model);
    }

    if (default_fallback_delegate_) {
      return default_fallback_delegate_->createWidget(index, model);
    }

    return ftxui::text("");
  }

  // ==========================================================================
  // Unified Spatial Sizing Dimension Routing Pass
  // ==========================================================================
  ftxui::Dimensions sizeHint(const ModelIndex& index,
                             const AbstractItemModel* model) const override {
    auto it = column_delegates_.find(index.column());
    if (it != column_delegates_.end() && it->second) {
      return it->second->sizeHint(index, model);
    }

    if (default_fallback_delegate_) {
      return default_fallback_delegate_->sizeHint(index, model);
    }

    return ftxui::Dimensions{0, 1};
  }
};

class MultiTypeRouterDelegate : public ItemDelegate {
 private:
  std::map<std::type_index, std::shared_ptr<ItemDelegate>> type_delegates_;

 public:
  MultiTypeRouterDelegate& registerType(
      const std::type_index type,
      const std::shared_ptr<ItemDelegate>& delegate) {
    type_delegates_[type] = delegate;
    return *this;
  }

  ftxui::Element createWidget(const ModelIndex& index,
                              const AbstractItemModel* model) const override {
    const std::any val = model->data(index, ItemRole::DisplayRole);
    if (!val.has_value()) {
      return ftxui::text("");
    }

    if (const auto it = type_delegates_.find(std::type_index(val.type()));
        it != type_delegates_.end() && it->second) {
      return it->second->createWidget(index, model);
    }
    return ftxui::text("");
  }

  ftxui::Dimensions sizeHint(const ModelIndex& index,
                             const AbstractItemModel* model) const override {
    const std::any val = model->data(index, ItemRole::DisplayRole);
    if (!val.has_value()) {
      return ftxui::Dimensions{0, 1};
    }

    if (const auto it = type_delegates_.find(std::type_index(val.type()));
        it != type_delegates_.end() && it->second) {
      return it->second->sizeHint(index, model);
    }
    return ftxui::Dimensions{0, 1};
  }
};

}  // namespace ftxmodel

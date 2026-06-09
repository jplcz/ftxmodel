#pragma once
#include <ftxmodel/header_delegate.hpp>
#include <ftxmodel/sort_filter_proxy_model.hpp>

#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"

namespace ftxmodel {

namespace header_decorators {

/**
 * @brief Creates a decorator that tracks and appends sorting arrows for a
 * SortFilterProxyModel.
 */
inline AdvancedHeaderDelegate::HeaderDecorator MakeSortArrowDecorator(
    const std::shared_ptr<const SortFilterProxyModel>& target_proxy) {
  std::weak_ptr weak_proxy = target_proxy;

  return [weak_proxy](std::string& text, const int section,
                      const Orientation orientation, const AbstractItemModel*) {
    // Only sort horizontal column headers
    if (orientation != Orientation::Horizontal) {
      return;
    }

    // Ensure the active model context matches your proxy framework layer
    if (auto proxy = weak_proxy.lock()) {
      if (const int active_col = proxy->sortColumn();
          active_col >= 0 && section == active_col) {
        text += proxy->ascending() ? " ▲" : " ▼";
      }
    }
  };
}

/**
 * @brief Appends a key icon to primary tracking data pathways.
 */
inline AdvancedHeaderDelegate::HeaderDecorator MakePrimaryKeyDecorator(
    const std::unordered_set<int>& key_columns) {
  return
      [key_columns](std::string& text, const int section,
                    const Orientation orientation, const AbstractItemModel*) {
        if (orientation == Orientation::Horizontal &&
            key_columns.contains(section)) {
          text = "🔑 " + text;  // Prepend key emblem to the label string
        }
      };
}

}  // namespace header_decorators

}  // namespace ftxmodel

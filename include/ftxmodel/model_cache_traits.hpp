#pragma once
#include <concepts>
#include "model_index.hpp"

namespace ftxmodel {

/**
 * @brief Ensures at compile-time that a Traits class contains the required
 * static functions to drive the UniqueIdCacheHelper.
 */
template <typename Traits, typename Model, typename InternalHandle>
concept ModelCachePolicy = requires {
  // The traits must provide a static route to manufacture a stable ID
  {
    Traits::getUniqueId(std::declval<const Model&>(),
                        std::declval<const InternalHandle&>())
  } -> std::same_as<UniqueNodeId>;

  // The traits must provide a static route to turn a handle into a view index
  // coordinate
  {
    Traits::createIndex(std::declval<const Model&>(),
                        std::declval<const InternalHandle&>(),
                        std::declval<int>())
  } -> std::same_as<ModelIndex>;
};

}  // namespace ftxmodel

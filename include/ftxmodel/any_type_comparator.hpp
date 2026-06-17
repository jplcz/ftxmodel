#pragma once
#include <any>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>

#include "any_to_string.hpp"
#include "model_index.hpp"

namespace ftxmodel {

/**
 * @brief A thread-safe, extensible utility for evaluating less-than
 * relationships between two `ModelIndex` objects based on their variant data
 * payloads.
 */
class AnyTypeComparator {
 public:
  // A generic comparator callback takes two std::any wrappers and evaluates
  // less-than
  using ComparatorCallback =
      std::function<bool(const std::any&, const std::any&)>;

  // ============================================================================
  // EXTENSIBLE TEMPLATE REGISTRATION TYPE
  // ============================================================================
  /**
   * @brief Registers a custom less-than comparison function for a given type
   * `T`.
   * @tparam T The custom type payload inside your model nodes.
   * @param comparator A lambda or function matching `bool(const T&, const T&)`
   */
  template <typename T>
  static void Register(std::function<bool(const T&, const T&)> comparator) {
    std::unique_lock lock(registry_mutex_);

    // Wrap the type-specific binary predicate into a generic std::any unpacker
    registry_[std::type_index(typeid(T))] =
        [comparator = std::move(comparator)](const std::any& lhs,
                                             const std::any& rhs) {
          return comparator(std::any_cast<const T&>(lhs),
                            std::any_cast<const T&>(rhs));
        };
  }

  /**
   * @brief Unregisters a previously registered comparator for a type `T`.
   */
  template <typename T>
  static void Unregister() {
    std::unique_lock lock(registry_mutex_);
    registry_.erase(std::type_index(typeid(T)));
  }

  /**
   * @brief Compares two `std::any` containers.
   * * If both contain the same type, it evaluates their relative order.
   * If the types mismatch, it falls back to comparing their stringified
   * versions.
   *
   * @return true if lhs is strictly less than rhs.
   */
  [[nodiscard]] static bool Compare(const std::any& lhs, const std::any& rhs) {
    // Ensure that values are not missing
    if (!lhs.has_value() || !rhs.has_value()) {
      return lhs.has_value() < rhs.has_value();
    }

    // Type Mismatch Fallback: Use string translation strategy
    if (lhs.type() != rhs.type()) {
      return AnyToStringTranslator::Translate(lhs) <
             AnyToStringTranslator::Translate(rhs);
    }

    const auto type_idx = std::type_index(lhs.type());

    // Native Primitives Optimization Paths
    if (type_idx == typeid(int)) {
      return std::any_cast<int>(lhs) < std::any_cast<int>(rhs);
    }
    if (type_idx == typeid(long)) {
      return std::any_cast<long>(lhs) < std::any_cast<long>(rhs);
    }
    if (type_idx == typeid(long long)) {
      return std::any_cast<long long>(lhs) < std::any_cast<long long>(rhs);
    }
    if (type_idx == typeid(unsigned int)) {
      return std::any_cast<unsigned int>(lhs) <
             std::any_cast<unsigned int>(rhs);
    }
    if (type_idx == typeid(unsigned long)) {
      return std::any_cast<unsigned long>(lhs) <
             std::any_cast<unsigned long>(rhs);
    }
    if (type_idx == typeid(unsigned long long)) {
      return std::any_cast<unsigned long long>(lhs) <
             std::any_cast<unsigned long long>(rhs);
    }
    if (type_idx == typeid(size_t)) {
      return std::any_cast<size_t>(lhs) < std::any_cast<size_t>(rhs);
    }
    if (type_idx == typeid(double)) {
      return std::any_cast<double>(lhs) < std::any_cast<double>(rhs);
    }
    if (type_idx == typeid(float)) {
      return std::any_cast<float>(lhs) < std::any_cast<float>(rhs);
    }
    if (type_idx == typeid(bool)) {
      return std::any_cast<bool>(lhs) < std::any_cast<bool>(rhs);
    }
    if (type_idx == typeid(char)) {
      return std::any_cast<char>(lhs) < std::any_cast<char>(rhs);
    }

    // String Types Fast Paths
    if (type_idx == typeid(std::string)) {
      return std::any_cast<const std::string&>(lhs) <
             std::any_cast<const std::string&>(rhs);
    }
    if (type_idx == typeid(std::string_view)) {
      return std::any_cast<std::string_view>(lhs) <
             std::any_cast<std::string_view>(rhs);
    }
    if (type_idx == typeid(const char*)) {
      return std::string_view(std::any_cast<const char*>(lhs)) <
             std::string_view(std::any_cast<const char*>(rhs));
    }

    // Dynamic Registry Lookup for registered user structures
    {
      std::shared_lock lock(registry_mutex_);
      if (const auto it = registry_.find(type_idx); it != registry_.end()) {
        return it->second(lhs, rhs);
      }
    }

    // Hard Fallback: If types match but no comparator is registered,
    // fallback to strings
    return AnyToStringTranslator::Translate(lhs) <
           AnyToStringTranslator::Translate(rhs);
  }

  /**
   * @brief Factory method that generates a `SortProxyModelLessThan` callback
   * closure.
   *
   * @param role The payload semantic target (e.g., DisplayRole, EditRole).
   * @param ascending True to sort matching structural logic upward, false to
   * flip output evaluation.
   * @return SortProxyModelLessThan Closure function compatible with
   * SortFilterProxyModel.
   */
  [[nodiscard]] static std::function<bool(ModelIndex, ModelIndex)>
  MakeSortCallback(ItemRole role = ItemRole::DisplayRole,
                   bool ascending = true) {
    return [role, ascending](ModelIndex lhs, ModelIndex rhs) -> bool {
      if (!lhs.isValid() || !rhs.isValid()) {
        return lhs.isValid() < rhs.isValid();
      }

      if (ascending) {
        return Compare(lhs.data(role), rhs.data(role));
      } else {
        return Compare(rhs.data(role), lhs.data(role));
      }
    };
  }

 private:
  inline static std::unordered_map<std::type_index, ComparatorCallback>
      registry_;
  inline static std::shared_mutex registry_mutex_;
};

}  // namespace ftxmodel

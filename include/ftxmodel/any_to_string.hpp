#pragma once
#include <any>
#include <format>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>

namespace ftxmodel {

/**
 * @brief A thread-safe, extensible utility for converting `std::any` to
 * `std::string`.
 *
 * This class provides a centralized mechanism for converting values stored in
 * `std::any` to their string representations. It supports common primitive
 * types out-of-the-box and allows for custom formatters to be registered for
 * user-defined types. This is particularly useful for logging, debugging, or
 * any scenario where a generic type needs to be represented as a string.
 *
 * The registry is thread-safe, allowing for concurrent registration and
 * translation.
 */
class AnyToStringTranslator {
 public:
  // A formatter function takes a std::any reference holding the type and
  // returns a string
  using FormatterCallback = std::function<std::string(const std::any&)>;

  // ============================================================================
  // EXTENSIBLE TEMPLATE REGISTRATION TYPE
  // ============================================================================
  /**
   * @brief Registers a custom formatter for a given type `T`.
   *
   * This function allows you to define how a specific type `T` should be
   * converted to a string. The provided formatter is a lambda or function
   * that takes a `const T&` and returns a `std::string`.
   *
   * @tparam T The type for which to register the formatter.
   * @param formatter The function that will perform the conversion.
   *
   * Example:
   * @code
   * struct MyStruct { std::string name; };
   * AnyToStringTranslator::Register<MyStruct>(
   *     [](const MyStruct& s) { return s.name; }
   * );
   * @endcode
   */
  template <typename T>
  static void Register(std::function<std::string(const T&)> formatter) {
    std::unique_lock lock(registry_mutex_);

    // Wrap the type-specific lambda into a generic std::any unpacker
    registry_[std::type_index(typeid(T))] =
        [formatter = std::move(formatter)](const std::any& operand) {
          return formatter(std::any_cast<const T&>(operand));
        };
  }

  /**
   * @brief Unregisters a previously registered formatter for a type `T`.
   *
   * If a formatter for type `T` is no longer needed, this function can be used
   * to remove it from the registry.
   *
   * @tparam T The type whose formatter should be unregistered.
   */
  template <typename T>
  static void Unregister() {
    std::unique_lock lock(registry_mutex_);
    registry_.erase(std::type_index(typeid(T)));
  }

  /**
   * @brief Converts the value held by a `std::any` to a `std::string`.
   *
   * This is the core function of the translator. It attempts to convert the
   * given `std::any` to a string in the following order:
   * 1. If the `std::any` is empty, it returns the `fallback` string.
   * 2. It checks for direct string types (`std::string`, `std::string_view`,
   * `const char*`).
   * 3. It checks for common primitive types and formats them.
   * 4. It looks for a custom formatter in the registry.
   * 5. If no other conversion is possible, it returns the `fallback` string.
   *
   * @param operand The `std::any` containing the value to translate.
   * @param fallback A string to return if the conversion is not possible.
   * @return The string representation of the value, or the fallback string.
   */
  [[nodiscard]] static std::string Translate(
      const std::any& operand,
      const std::string_view fallback = "") {
    if (!operand.has_value()) {
      return std::string(fallback);
    }

    const auto type_idx = std::type_index(operand.type());

    // Direct String Matches (Zero conversion work)
    if (type_idx == typeid(std::string)) {
      return std::any_cast<const std::string&>(operand);
    }
    if (type_idx == typeid(std::string_view)) {
      return std::string(std::any_cast<std::string_view>(operand));
    }
    if (type_idx == typeid(const char*)) {
      return std::string(std::any_cast<const char*>(operand));
    }
    if (type_idx == typeid(char*)) {
      return std::string(std::any_cast<char*>(operand));
    }

    // PRIMITIVE PATHS (Maintaining low overhead formatting)
    if (type_idx == typeid(int)) {
      return Format(std::any_cast<int>(operand));
    }
    if (type_idx == typeid(long)) {
      return Format(std::any_cast<long>(operand));
    }
    if (type_idx == typeid(long long)) {
      return Format(std::any_cast<long long>(operand));
    }
    if (type_idx == typeid(unsigned int)) {
      return Format(std::any_cast<unsigned int>(operand));
    }
    if (type_idx == typeid(unsigned long)) {
      return Format(std::any_cast<unsigned long>(operand));
    }
    if (type_idx == typeid(unsigned long long)) {
      return Format(std::any_cast<unsigned long long>(operand));
    }
    if (type_idx == typeid(size_t)) {
      return Format(std::any_cast<size_t>(operand));
    }
    if (type_idx == typeid(double)) {
      return Format(std::any_cast<double>(operand));
    }
    if (type_idx == typeid(float)) {
      return Format(std::any_cast<float>(operand));
    }
    if (type_idx == typeid(bool)) {
      return std::any_cast<bool>(operand) ? "true" : "false";
    }
    if (type_idx == typeid(char)) {
      return std::string(1, std::any_cast<char>(operand));
    }

    {
      std::shared_lock lock(registry_mutex_);
      if (const auto it = registry_.find(type_idx); it != registry_.end()) {
        // Execute the registered formatter callback
        return it->second(operand);
      }
    }

    return std::string(fallback);
  }

 private:
  template <typename T>
  [[nodiscard]] static std::string Format(T value) {
    return std::format("{}", value);
  }

  // Static registry infrastructure
  inline static std::unordered_map<std::type_index, FormatterCallback>
      registry_;
  inline static std::shared_mutex registry_mutex_;  // Thread safety protection
};

}  // namespace ftxmodel

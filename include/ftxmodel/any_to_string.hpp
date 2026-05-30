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

class AnyToStringTranslator {
 public:
  // A formatter function takes a std::any reference holding the type and
  // returns a string
  using FormatterCallback = std::function<std::string(const std::any&)>;

  // ============================================================================
  // EXTENSIBLE TEMPLATE REGISTRATION TYPE
  // ============================================================================
  // Registers a custom formatter for type T.
  // Example: Register<MyStruct>([](const MyStruct& s) { return
  // std::format("{}", s.name); });
  template <typename T>
  static void Register(std::function<std::string(const T&)> formatter) {
    std::unique_lock lock(registry_mutex_);

    // Wrap the type-specific lambda into a generic std::any unpacker
    registry_[std::type_index(typeid(T))] =
        [formatter = std::move(formatter)](const std::any& operand) {
          return formatter(std::any_cast<const T&>(operand));
        };
  }

  // Unregisters a type formatting specification if needed
  template <typename T>
  static void Unregister() {
    std::unique_lock lock(registry_mutex_);
    registry_.erase(std::type_index(typeid(T)));
  }

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

    // INTEGER PATHS (Maintaining ultra-low overhead std::to_chars stack
    // buffers)
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
      return std::format("{:s}", std::any_cast<bool>(operand));
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

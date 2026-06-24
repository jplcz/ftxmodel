#pragma once
#include <any>
#include <typeindex>
#include <unordered_map>
#include "tree_table_model.hpp"

namespace ftxmodel {

/**
 * @class PolymorphicTreeTableModel
 * @brief An open, runtime polymorphic tree model that accepts any custom struct
 * type wrapped inside a std::any wrapper container, evaluating columns via
 * typeid matching rules.
 */
class PolymorphicTreeTableModel : public TreeTableModel<std::any> {
 public:
  using PolymorphicExtractor =
      std::function<std::any(const std::any&, int, ItemRole)>;
  using PolymorphicMutator =
      std::function<bool(std::any&, int, const std::any&, ItemRole)>;

  explicit PolymorphicTreeTableModel(std::vector<std::string> headers)
      : TreeTableModel(std::move(headers)) {
    for (int col = 0; col < static_cast<int>(m_headers.size()); ++col) {
      // Setup internal delegator maps to handle columns cleanly
      TreeTableModel::setColumnLogic(
          col,
          [this](const RowData& var, int c_col, ItemRole c_role) {
            return routeExtractor(var, c_col, c_role);
          },
          [this](RowData& var, int c_col, const std::any& val,
                 ItemRole c_role) {
            return routeMutator(var, c_col, val, c_role);
          });
    }
  }

  ~PolymorphicTreeTableModel() override = default;

  void setColumnLogic(int column,
                      DataExtractor extractor,
                      DataMutator mutator) = delete;

  /**
   * @brief Registers an extraction layout formatter rule bound to a specific
   * runtime type context.
   * @tparam T Concrete struct type being intercepted.
   */
  template <typename T>
  void registerTypeHandler(PolymorphicExtractor extractor,
                           PolymorphicMutator mutator = nullptr) {
    std::type_index typeIdx(typeid(T));
    m_extractors[typeIdx] = std::move(extractor);
    if (mutator) {
      m_mutators[typeIdx] = std::move(mutator);
    }
  }

 private:
  std::any routeExtractor(const RowData& variantRow,
                          int column,
                          ItemRole role) const {
    // Unwrap the std::any out of our model's core variant
    const std::any& rawAny = std::get<std::any>(variantRow);
    if (!rawAny.has_value()) {
      return {};
    }

    // Extract type_index and locate runtime registered logic handler
    const std::type_index typeIdx(rawAny.type());
    if (const auto it = m_extractors.find(typeIdx); it != m_extractors.end()) {
      return it->second(rawAny, column, role);
    }
    return {};
  }

  bool routeMutator(RowData& variantRow,
                    int column,
                    const std::any& incomingVal,
                    ItemRole role) {
    std::any& rawAny = std::get<std::any>(variantRow);
    if (!rawAny.has_value()) {
      return false;
    }

    std::type_index typeIdx(rawAny.type());
    if (const auto it = m_mutators.find(typeIdx); it != m_mutators.end()) {
      return it->second(rawAny, column, incomingVal, role);
    }
    return false;
  }

  std::unordered_map<std::type_index, PolymorphicExtractor> m_extractors;
  std::unordered_map<std::type_index, PolymorphicMutator> m_mutators;
};

}  // namespace ftxmodel

#pragma once
#include <any>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>
#include "vector_table_model.hpp"

namespace ftxmodel {

/**
 * @class PolymorphicVectorTableModel
 * @brief An open, 2D flat table model that accepts rows of any type wrapped
 * inside std::any, routing column extraction and editing rules via model-wide
 * runtime typeid matching.
 */
class PolymorphicVectorTableModel : public VectorTableModel<std::any> {
 public:
  // Lambdas now accept the active column parameter explicitly to decide field
  // routing internally
  using TypeExtractor =
      std::function<std::any(const std::any&, int column, ItemRole)>;
  using TypeMutator =
      std::function<bool(std::any&, int column, const std::any&, ItemRole)>;

  explicit PolymorphicVectorTableModel() = default;
  ~PolymorphicVectorTableModel() override = default;

  // Hide the base class column addition method to control proxy routing
  // internally
  void addColumn(const std::string&,
                 DataExtractor,
                 DataMutator = nullptr) = delete;

  /**
   * @brief Declares a new horizontal column header track layout configuration.
   */
  void declareColumn(const std::string& title) {
    const int colIdx = columnCount();

    // Bind the base VectorTableModel logic globally to our uniform type routers
    VectorTableModel::addColumn(
        title,
        [this, colIdx](const std::any& rowAny, ItemRole role) {
          return routeExtractor(rowAny, colIdx, role);
        },
        [this, colIdx](std::any& rowAny, const std::any& val, ItemRole role) {
          return routeMutator(rowAny, colIdx, val, role);
        });
  }

  /**
   * @brief Registers an extraction and optional mutation rule for a specific
   * type globally across the model.
   * @tparam T The concrete type being intercepted.
   */
  template <typename T>
  void registerTypeHandler(TypeExtractor extractor,
                           TypeMutator mutator = nullptr) {
    const std::type_index typeIdx(typeid(T));
    m_extractors[typeIdx] = std::move(extractor);

    if (mutator) {
      m_mutators[typeIdx] = std::move(mutator);
    }
  }

 private:
  std::any routeExtractor(const std::any& rowAny,
                          int column,
                          ItemRole role) const {
    if (!rowAny.has_value()) {
      return {};
    }

    std::type_index typeIdx(rowAny.type());
    auto it = m_extractors.find(typeIdx);
    if (it != m_extractors.end()) {
      return it->second(rowAny, column, role);
    }
    return {};
  }

  bool routeMutator(std::any& rowAny,
                    int column,
                    const std::any& incomingVal,
                    ItemRole role) {
    if (!rowAny.has_value()) {
      return false;
    }

    std::type_index typeIdx(rowAny.type());
    auto it = m_mutators.find(typeIdx);
    if (it != m_mutators.end()) {
      return it->second(rowAny, column, incomingVal, role);
    }
    return false;
  }

  // Model-wide maps: Single map entry holds the entire routing logic for a Type
  // index
  std::unordered_map<std::type_index, TypeExtractor> m_extractors;
  std::unordered_map<std::type_index, TypeMutator> m_mutators;
};

}  // namespace ftxmodel

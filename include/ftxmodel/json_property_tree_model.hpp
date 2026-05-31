#pragma once
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "abstract_item_model.hpp"

namespace ftxmodel {

enum class JsonNodeType : uint8_t {
  Object,
  Array,
  String,
  Number,
  Boolean,
  Null
};

struct JsonPropertyNode {
  JsonPropertyNode* parent = nullptr;
  std::vector<std::unique_ptr<JsonPropertyNode>> children;

  // Object property name or array index bracket string e.g., "[0]"
  std::string key;
  // Sibling index position
  int row_in_parent = 0;
  JsonNodeType type = JsonNodeType::Null;

  // Raw pointer to the actual value slice in the master document
  nlohmann::json* json_ptr = nullptr;

  // Recursive construction engine to map the entire JSON layout tree
  static std::unique_ptr<JsonPropertyNode> buildTree(
      nlohmann::json* element,
      const std::string& node_key = "Root",
      JsonPropertyNode* parent_node = nullptr) {
    auto node = std::make_unique<JsonPropertyNode>();
    node->parent = parent_node;
    node->json_ptr = element;
    node->key = node_key;

    if (!element) {
      node->type = JsonNodeType::Null;
      return node;
    }

    // Assign explicit semantic types
    if (element->is_object()) {
      node->type = JsonNodeType::Object;
    } else if (element->is_array()) {
      node->type = JsonNodeType::Array;
    } else if (element->is_string()) {
      node->type = JsonNodeType::String;
    } else if (element->is_number()) {
      node->type = JsonNodeType::Number;
    } else if (element->is_boolean()) {
      node->type = JsonNodeType::Boolean;
    } else {
      node->type = JsonNodeType::Null;
    }

    // Recursively append children if the element is a container node
    int current_row = 0;
    if (element->is_object()) {
      for (auto it = element->begin(); it != element->end(); ++it) {
        auto child = buildTree(&it.value(), it.key(), node.get());
        child->row_in_parent = current_row++;
        node->children.push_back(std::move(child));
      }
    } else if (element->is_array()) {
      for (auto it = element->begin(); it != element->end(); ++it) {
        auto child = buildTree(
            &it.value(), "[" + std::to_string(current_row) + "]", node.get());
        child->row_in_parent = current_row++;
        node->children.push_back(std::move(child));
      }
    }

    return node;
  }
};

class JsonPropertyTreeModel : public AbstractItemModel {
 private:
  nlohmann::json m_raw_document;
  std::unique_ptr<JsonPropertyNode> m_root_node;

 public:
  explicit JsonPropertyTreeModel(nlohmann::json json_doc)
      : m_raw_document(std::move(json_doc)) {
    m_root_node = JsonPropertyNode::buildTree(&m_raw_document, "Root");
  }

  ~JsonPropertyTreeModel() override = default;

  ModelIndex index(const int row,
                   const int column,
                   const ModelIndex& parent = ModelIndex()) const override {
    if (row < 0 || column < 0 || column >= columnCount()) {
      return {};
    }

    JsonPropertyNode* parent_node =
        parent.isValid()
            ? static_cast<JsonPropertyNode*>(parent.internalPointer())
            : m_root_node.get();

    if (parent_node && row < static_cast<int>(parent_node->children.size())) {
      return createIndex(row, column,
                         parent_node->children[static_cast<size_t>(row)].get());
    }
    return {};
  }

  ModelIndex parent(const ModelIndex& child) const override {
    if (!child.isValid()) {
      return {};
    }

    const auto* child_node =
        static_cast<JsonPropertyNode*>(child.internalPointer());
    if (!child_node || child_node == m_root_node.get()) {
      return {};
    }

    JsonPropertyNode* parent_node = child_node->parent;
    if (!parent_node || parent_node == m_root_node.get()) {
      return {};
    }

    return createIndex(parent_node->row_in_parent, 0, parent_node);
  }

  int rowCount(const ModelIndex& parent = ModelIndex()) const override {
    JsonPropertyNode* parent_node =
        parent.isValid()
            ? static_cast<JsonPropertyNode*>(parent.internalPointer())
            : m_root_node.get();

    return parent_node ? static_cast<int>(parent_node->children.size()) : 0;
  }

  int columnCount(const ModelIndex& = ModelIndex()) const override {
    return 3;  // Col 0: Property, Col 1: Type, Col 2: Value/Summary
  }

  std::any data(const ModelIndex& index, ItemRole role) const override {
    if (!index.isValid() || role != ItemRole::DisplayRole) {
      return {};
    }

    auto* node = static_cast<JsonPropertyNode*>(index.internalPointer());
    if (!node || !node->json_ptr) {
      return {};
    }

    // --- COLUMN 0: THE STRUCTURAL PROPERTY KEY ---
    if (index.column() == 0) {
      return node->key;
    }

    // --- COLUMN 1: TYPE SYSTEM FIELD METADATA ---
    if (index.column() == 1) {
      switch (node->type) {
        case JsonNodeType::Object:
          return std::string("Object");
        case JsonNodeType::Array:
          return std::string("Array");
        case JsonNodeType::String:
          return std::string("String");
        case JsonNodeType::Number:
          return std::string("Number");
        case JsonNodeType::Boolean:
          return std::string("Boolean");
        case JsonNodeType::Null:
          return std::string("Null");
      }
    }

    // --- COLUMN 2: CONTEXTUAL VALUE SUMMARY FACTORY ---
    if (index.column() == 2) {
      auto* j = node->json_ptr;
      switch (node->type) {
        case JsonNodeType::Object:
          return "{" + std::to_string(j->size()) + " fields}";
        case JsonNodeType::Array:
          return "[" + std::to_string(j->size()) + " items]";
        case JsonNodeType::String:
          return j->get<std::string>();
        case JsonNodeType::Number:
          return j->dump();  // Dumps pristine numeric string representations
                             // natively
        case JsonNodeType::Boolean:
          return j->get<bool>() ? std::string("true") : std::string("false");
        case JsonNodeType::Null:
          return std::string("null");
      }
    }

    return {};
  }

  std::any headerData(int section,
                      Orientation orientation,
                      ItemRole role) const override {
    if (role != ItemRole::DisplayRole ||
        orientation != Orientation::Horizontal) {
      return {};
    }
    if (section == 0) {
      return std::string("Property Key");
    }
    if (section == 1) {
      return std::string("Type");
    }
    if (section == 2) {
      return std::string("Value / Context Summary");
    }
    return {};
  }

  UniqueNodeId uniqueId(const ModelIndex& index) const override {
    if (!index.isValid()) {
      return {nullptr};
    }
    return {index.internalPointer()};
  }
};

}  // namespace ftxmodel

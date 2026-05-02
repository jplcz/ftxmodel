#pragma once
#include <ftxmodel/abstract_item_model.hpp>
#include <string>
#include <vector>

namespace ftxmodel {

class MockItemModel : public AbstractItemModel {
 public:
  struct Node {
    std::string name;
    std::vector<Node> children;
  };

 private:
  Node m_root;

 public:
  MockItemModel() {
    // Setup a 2-level hierarchical mock dataset
    m_root = {"Root", {{"Node_0", {{"Child_0_0", {}}}}, {"Node_1", {}}}};
  }

  ModelIndex index(int row,
                   int column,
                   const ModelIndex& parent = ModelIndex()) const override {
    if (row < 0 || column < 0) {
      return {};
    }

    const Node* parentNode =
        parent.isValid() ? static_cast<const Node*>(parent.internalPointer())
                         : &m_root;

    if (row >= static_cast<int>(parentNode->children.size())) {
      return {};
    }

    // Pass the address of the actual node item as the internal pointer track
    return createIndex(
        row, column,
        const_cast<Node*>(&parentNode->children[static_cast<size_t>(row)]));
  }

  ModelIndex parent(const ModelIndex& child) const override {
    if (!child.isValid()) {
      return {};
    }

    const auto childNode = static_cast<const Node*>(child.internalPointer());

    // Handle level 2 to level 1 upward tracking search
    for (size_t i = 0; i < m_root.children.size(); ++i) {
      for (size_t j = 0; j < m_root.children[i].children.size(); ++j) {
        if (&m_root.children[i].children[j] == childNode) {
          return createIndex(static_cast<int>(i), 0,
                             (void*)&m_root.children[i]);
        }
      }
    }
    return {};  // Level 1 parent is the invalid root context handle
  }

  int rowCount(const ModelIndex& parent = ModelIndex()) const override {
    const Node* parentNode =
        parent.isValid() ? static_cast<const Node*>(parent.internalPointer())
                         : &m_root;
    return static_cast<int>(parentNode->children.size());
  }

  int columnCount(const ModelIndex& = ModelIndex()) const override {
    return 2;  // Flat mock layout width metric constraint
  }

  std::any data(const ModelIndex& index,
                ItemRole role = ItemRole::DisplayRole) const override {
    if (!index.isValid()) {
      return {};
    }
    const Node* node = static_cast<const Node*>(index.internalPointer());
    assert(node != nullptr);

    if (role == ItemRole::DisplayRole) {
      return node->name;
    }
    if (role == ItemRole::UniqueIdentifierRole) {
      return "id_" + node->name;
    }
    return {};
  }

  // Exposed helper tools for testing reactive update triggers
  void triggerItemChanged(const ModelIndex& topLeft,
                          const ModelIndex& bottomRight) {
    dataChanged(topLeft, bottomRight);
  }
};

}  // namespace ftxmodel

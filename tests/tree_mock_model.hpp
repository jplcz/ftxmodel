#pragma once
#include <ftxmodel/abstract_item_model.hpp>
#include <memory>
#include <string>
#include <vector>

namespace ftxmodel {

class TreeMockModel : public AbstractItemModel {
 public:
  struct Node {
    std::string name;
    int weight;
    std::vector<std::shared_ptr<Node>> children;
  };

 private:
  std::shared_ptr<Node> m_root;

 public:
  TreeMockModel() {
    // Setup a 2-level multi-column hierarchy
    // Column 0: Name (string), Column 1: Weight (int)
    m_root = std::make_shared<Node>(
        "Root", 0,
        std::vector<std::shared_ptr<Node>>{
            std::make_shared<Node>(
                "Fruit", 10,
                std::vector<std::shared_ptr<Node>>{
                    std::make_shared<Node>(
                        "Apple", 5, std::vector<std::shared_ptr<Node>>{}),
                    std::make_shared<Node>(
                        "Banana", 2, std::vector<std::shared_ptr<Node>>{})}),
            std::make_shared<Node>(
                "Animal", 50,
                std::vector<std::shared_ptr<Node>>{std::make_shared<Node>(
                    "Zebra", 100, std::vector<std::shared_ptr<Node>>{})})});
  }

  ModelIndex index(int row,
                   int column,
                   const ModelIndex& parent = ModelIndex()) const override {
    if (row < 0 || column < 0) {
      return {};
    }
    const Node* parentNode =
        parent.isValid() ? static_cast<const Node*>(parent.internalPointer())
                         : m_root.get();
    if (row >= static_cast<int>(parentNode->children.size())) {
      return {};
    }
    return createIndex(row, column,
                       parentNode->children[static_cast<size_t>(row)].get());
  }

  ModelIndex parent(const ModelIndex& child) const override {
    if (!child.isValid()) {
      return {};
    }
    const Node* childNode = static_cast<const Node*>(child.internalPointer());

    // Check level 1 root children
    for (size_t i = 0; i < m_root->children.size(); ++i) {
      if (m_root->children[i].get() == childNode) {
        return {};
      }
      // Check level 2 nested items
      for (size_t j = 0; j < m_root->children[i]->children.size(); ++j) {
        if (m_root->children[i]->children[j].get() == childNode) {
          return createIndex(static_cast<int>(i), 0, m_root->children[i].get());
        }
      }
    }
    return {};
  }

  int rowCount(const ModelIndex& parent = ModelIndex()) const override {
    const Node* parentNode =
        parent.isValid() ? static_cast<const Node*>(parent.internalPointer())
                         : m_root.get();
    return static_cast<int>(parentNode->children.size());
  }

  int columnCount(const ModelIndex& = ModelIndex()) const override { return 2; }

  std::any data(const ModelIndex& index,
                ItemRole role = ItemRole::DisplayRole) const override {
    if (!index.isValid()) {
      return {};
    }
    const Node* node = static_cast<const Node*>(index.internalPointer());

    if (role == ItemRole::DisplayRole) {
      if (index.column() == 0) {
        return node->name;
      }
      if (index.column() == 1) {
        return node->weight;
      }
    }
    return {};
  }
};

}  // namespace ftxmodel

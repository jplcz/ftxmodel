#pragma once
#include <any>
#include <ftxmodel/abstract_item_model.hpp>  // Assumed baseline architecture match
#include <memory>
#include <string>
#include <vector>

namespace ftxmodel {

// Basic structural data node
struct Node {
  std::string name;
  std::string type;  // "Folder" or "File"
  Node* parent = nullptr;
  std::vector<std::unique_ptr<Node>> children;
};

class TreeSourceModel : public AbstractItemModel {
 public:
  TreeSourceModel() {
    // Build a mock file explorer tree hierarchy
    auto root = std::make_unique<Node>("Root", "Folder");

    auto src = std::make_unique<Node>("src", "Folder", root.get());
    src->children.emplace_back(
        std::make_unique<Node>("main.cpp", "File", src.get()));
    src->children.emplace_back(
        std::make_unique<Node>("utils.hpp", "File", src.get()));

    auto assets = std::make_unique<Node>("assets", "Folder", root.get());
    auto icons = std::make_unique<Node>("icons", "Folder", assets.get());
    icons->children.emplace_back(
        std::make_unique<Node>("logo.png", "File", icons.get()));
    assets->children.emplace_back(std::move(icons));

    root->children.emplace_back(std::move(src));
    root->children.emplace_back(std::move(assets));
    root_node_ = std::move(root);
  }

  int rowCount(const ModelIndex& parent) const override {
    if (!parent.isValid()) {
      return static_cast<int>(root_node_->children.size());
    }
    auto* parent_node = static_cast<Node*>(parent.internalPointer());
    return static_cast<int>(parent_node->children.size());
  }

  int columnCount(const ModelIndex&) const override { return 1; }

  ModelIndex index(int row,
                   int column,
                   const ModelIndex& parent) const override {
    if (row < 0 || column < 0) {
      return {};
    }
    Node* parent_node = parent.isValid()
                            ? static_cast<Node*>(parent.internalPointer())
                            : root_node_.get();
    if (row >= static_cast<int>(parent_node->children.size())) {
      return {};
    }
    return createIndex(row, column,
                       parent_node->children[static_cast<size_t>(row)].get());
  }

  ModelIndex parent(const ModelIndex& child) const override {
    if (!child.isValid()) {
      return {};
    }
    auto* child_node = static_cast<Node*>(child.internalPointer());
    Node* parent_node = child_node->parent;
    if (!parent_node || parent_node == root_node_.get()) {
      return {};
    }

    // Find parent's row relative to its grandparent
    Node* grand_node =
        parent_node->parent ? parent_node->parent : root_node_.get();
    for (size_t i = 0; i < grand_node->children.size(); ++i) {
      if (grand_node->children[i].get() == parent_node) {
        return createIndex(static_cast<int>(i), 0, parent_node);
      }
    }
    return {};
  }

  bool hasChildren(const ModelIndex& index) const override {
    if (!index.isValid()) {
      return !root_node_->children.empty();
    }
    auto* node = static_cast<Node*>(index.internalPointer());
    return !node->children.empty();
  }

  std::any data(const ModelIndex& index, ItemRole role) const override {
    if (!index.isValid() || role != ItemRole::DisplayRole) {
      return {};
    }
    auto* node = static_cast<Node*>(index.internalPointer());
    return node->name;
  }

  UniqueNodeId uniqueId(const ModelIndex& index) const override {
    if (!index.isValid()) {
      return {nullptr};
    }
    // Use the raw memory address of our stable data nodes as the unique
    // structural identifier
    return {index.internalPointer()};
  }

  // Helper math loop to trace indentation depth for our visual view layout
  // components
  int calculateNodeDepth(const ModelIndex& index) const {
    int depth = 0;
    ModelIndex current = index;
    while (current.isValid()) {
      depth++;
      current = parent(current);
    }
    return depth - 1;
  }

 private:
  std::unique_ptr<Node> root_node_;
};

}  // namespace ftxmodel

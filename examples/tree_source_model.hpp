#pragma once
#include <any>
#include <ftxmodel/abstract_item_model.hpp>
#include <memory>
#include <string>
#include <vector>

// Basic structural data node
struct Node {
  std::string name;
  std::string type;  // "Folder" or "File"
  Node* parent = nullptr;
  std::vector<std::unique_ptr<Node>> children;
};

class TreeSourceModel : public ftxmodel::AbstractItemModel {
 public:
  TreeSourceModel() {
    // 1. Root Workspace Node
    auto root = std::make_unique<Node>("Root", "Folder");

    // =========================================================================
    // LAYER 1 BRANCH: .github (Hidden configurations tracking)
    // =========================================================================
    auto github = std::make_unique<Node>(".github", "Folder", root.get());
    auto workflows =
        std::make_unique<Node>("workflows", "Folder", github.get());
    workflows->children.emplace_back(
        std::make_unique<Node>("ci.yml", "File", workflows.get()));
    workflows->children.emplace_back(
        std::make_unique<Node>("cd.yml", "File", workflows.get()));
    github->children.emplace_back(std::move(workflows));
    root->children.emplace_back(std::move(github));

    // =========================================================================
    // LAYER 1 BRANCH: src (Core implementation layers)
    // =========================================================================
    auto src = std::make_unique<Node>("src", "Folder", root.get());
    src->children.emplace_back(
        std::make_unique<Node>("main.cpp", "File", src.get()));
    src->children.emplace_back(
        std::make_unique<Node>("utils.hpp", "File", src.get()));

    // Deeply nesting a component layer inside src: src -> components -> views
    auto components = std::make_unique<Node>("components", "Folder", src.get());
    auto views = std::make_unique<Node>("views", "Folder", components.get());
    views->children.emplace_back(
        std::make_unique<Node>("table_view.cpp", "File", views.get()));
    views->children.emplace_back(
        std::make_unique<Node>("tree_view.cpp", "File", views.get()));
    components->children.emplace_back(std::move(views));

    auto models = std::make_unique<Node>("models", "Folder", components.get());
    models->children.emplace_back(
        std::make_unique<Node>("flatten_model.cpp", "File", models.get()));
    components->children.emplace_back(std::move(models));

    src->children.emplace_back(std::move(components));
    root->children.emplace_back(std::move(src));

    // =========================================================================
    // LAYER 1 BRANCH: assets (Static media and properties mappings)
    // =========================================================================
    auto assets = std::make_unique<Node>("assets", "Folder", root.get());

    // assets -> icons -> raster / vector subfolders
    auto icons = std::make_unique<Node>("icons", "Folder", assets.get());

    auto raster = std::make_unique<Node>("raster", "Folder", icons.get());
    raster->children.emplace_back(
        std::make_unique<Node>("logo_32x32.png", "File", raster.get()));
    raster->children.emplace_back(
        std::make_unique<Node>("logo_64x64.png", "File", raster.get()));
    icons->children.emplace_back(std::move(raster));

    auto vector = std::make_unique<Node>("vector", "Folder", icons.get());
    vector->children.emplace_back(
        std::make_unique<Node>("banner.svg", "File", vector.get()));
    icons->children.emplace_back(std::move(vector));

    assets->children.emplace_back(std::move(icons));

    auto themes = std::make_unique<Node>("themes", "Folder", assets.get());
    themes->children.emplace_back(
        std::make_unique<Node>("dark_mode.json", "File", themes.get()));
    themes->children.emplace_back(
        std::make_unique<Node>("light_mode.json", "File", themes.get()));
    assets->children.emplace_back(std::move(themes));

    root->children.emplace_back(std::move(assets));

    // =========================================================================
    // LAYER 1 BRANCH: build (Simulated build artifacts directory tree)
    // =========================================================================
    auto build = std::make_unique<Node>("build", "Folder", root.get());
    auto cmake_files =
        std::make_unique<Node>("CMakeFiles", "Folder", build.get());
    auto ftxmodel_dir =
        std::make_unique<Node>("ftxmodel.dir", "Folder", cmake_files.get());
    ftxmodel_dir->children.emplace_back(
        std::make_unique<Node>("main.cpp.o", "File", ftxmodel_dir.get()));
    ftxmodel_dir->children.emplace_back(
        std::make_unique<Node>("utils.cpp.o", "File", ftxmodel_dir.get()));
    cmake_files->children.emplace_back(std::move(ftxmodel_dir));
    build->children.emplace_back(std::move(cmake_files));

    build->children.emplace_back(
        std::make_unique<Node>("Makefile", "File", build.get()));
    build->children.emplace_back(
        std::make_unique<Node>("CMakeCache.txt", "File", build.get()));
    root->children.emplace_back(std::move(build));

    // =========================================================================
    // LAYER 1 SIBLINGS: Root files
    // =========================================================================
    root->children.emplace_back(
        std::make_unique<Node>("CMakeLists.txt", "File", root.get()));
    root->children.emplace_back(
        std::make_unique<Node>("README.md", "File", root.get()));
    root->children.emplace_back(
        std::make_unique<Node>(".gitignore", "File", root.get()));

    root_node_ = std::move(root);
  }

  int rowCount(const ftxmodel::ModelIndex& parent) const override {
    if (!parent.isValid()) {
      return static_cast<int>(root_node_->children.size());
    }
    auto* parent_node = static_cast<Node*>(parent.internalPointer());
    return static_cast<int>(parent_node->children.size());
  }

  int columnCount(const ftxmodel::ModelIndex&) const override { return 1; }

  ftxmodel::ModelIndex index(
      int row,
      int column,
      const ftxmodel::ModelIndex& parent) const override {
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

  ftxmodel::ModelIndex parent(
      const ftxmodel::ModelIndex& child) const override {
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

  bool hasChildren(const ftxmodel::ModelIndex& index) const override {
    if (!index.isValid()) {
      return !root_node_->children.empty();
    }
    auto* node = static_cast<Node*>(index.internalPointer());
    return !node->children.empty();
  }

  std::any data(const ftxmodel::ModelIndex& index,
                ftxmodel::ItemRole role) const override {
    if (!index.isValid() || role != ftxmodel::ItemRole::DisplayRole) {
      return {};
    }
    auto* node = static_cast<Node*>(index.internalPointer());
    return node->name;
  }

  ftxmodel::UniqueNodeId uniqueId(
      const ftxmodel::ModelIndex& index) const override {
    if (!index.isValid()) {
      return {nullptr};
    }
    return {index.internalPointer()};
  }

  int calculateNodeDepth(const ftxmodel::ModelIndex& index) const {
    int depth = 0;
    ftxmodel::ModelIndex current = index;
    while (current.isValid()) {
      depth++;
      current = parent(current);
    }
    return depth - 1;
  }

 private:
  std::unique_ptr<Node> root_node_;
};

#pragma once
#include <algorithm>
#include <any>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include "abstract_item_model.hpp"
#include "item_delegate.hpp"

namespace ftxmodel {

namespace fs = std::filesystem;

struct FsNode {
  fs::directory_entry entry;
  FsNode* parent = nullptr;
  std::vector<std::unique_ptr<FsNode>> children;
  bool children_populated = false;

  FsNode(fs::directory_entry e, FsNode* p = nullptr)
      : entry(std::move(e)), parent(p) {}

  // Find the row offset of this node within its parent's child array
  int row() const {
    if (!parent) {
      return 0;
    }
    auto it =
        std::find_if(parent->children.begin(), parent->children.end(),
                     [this](const auto& child) { return child.get() == this; });
    return (it != parent->children.end())
               ? static_cast<int>(std::distance(parent->children.begin(), it))
               : 0;
  }
};

class FileSystemModel : public AbstractItemModel {
 private:
  std::unique_ptr<FsNode> root_;

  // Helper to format file sizes cleanly
  std::string formatSize(uint64_t bytes) const {
    double size = static_cast<double>(bytes);
    std::vector<std::string> units = {"B", "KB", "MB", "GB", "TB"};
    size_t unit_idx = 0;
    while (size >= 1024.0 && unit_idx < units.size() - 1) {
      size /= 1024.0;
      unit_idx++;
    }
    std::stringstream ss;
    ss << std::fixed << std::setprecision(unit_idx == 0 ? 0 : 1) << size << " "
       << units[unit_idx];
    return ss.str();
  }

  // Helper to translate filesystem time to a display string
  std::string formatTime(fs::file_time_type time) const {
    auto sct =
        std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            time - fs::file_time_type::clock::now() +
            std::chrono::system_clock::now());
    std::time_t tt = std::chrono::system_clock::to_time_t(sct);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&tt), "%Y-%m-%d %H:%M:%S");
    return ss.str();
  }

  // Lazily populates directory paths on demand when requested by the View layer
  void populateChildren(FsNode* node) const {
    if (node->children_populated || !node->entry.is_directory()) {
      return;
    }

    try {
      // Read directories using native std::filesystem iterators
      for (const auto& entry : fs::directory_iterator(
               node->entry.path(),
               fs::directory_options::skip_permission_denied)) {
        node->children.emplace_back(std::make_unique<FsNode>(entry, node));
      }
      // Optional: Sort directories above files, then sort alphabetically
      std::sort(node->children.begin(), node->children.end(),
                [](const auto& a, const auto& b) {
                  if (a->entry.is_directory() != b->entry.is_directory()) {
                    return a->entry.is_directory() > b->entry.is_directory();
                  }
                  return a->entry.path().filename() <
                         b->entry.path().filename();
                });
    } catch (...) {
      // Gracefully ignore directories with locked OS permissions
    }
    node->children_populated = true;
  }

 public:
  explicit FileSystemModel(const std::string& rootPath) {
    root_ = std::make_unique<FsNode>(fs::directory_entry(fs::path(rootPath)));
  }

  // --- Geometry & Structure Resolution ---
  ModelIndex index(int row, int col, const ModelIndex& parent) const override {
    if (row < 0 || col < 0 || col >= 4) {
      return {};
    }
    FsNode* pNode = parent.isValid()
                        ? static_cast<FsNode*>(parent.internalPointer())
                        : root_.get();

    populateChildren(pNode);
    if (row >= static_cast<int>(pNode->children.size())) {
      return {};
    }
    return createIndex(row, col, pNode->children[(size_t)row].get());
  }

  ModelIndex parent(const ModelIndex& child) const override {
    if (!child.isValid()) {
      return {};
    }
    FsNode* cNode = static_cast<FsNode*>(child.internalPointer());
    FsNode* pNode = cNode->parent;
    if (!pNode || pNode == root_.get()) {
      return {};
    }
    return createIndex(pNode->row(), 0, pNode);
  }

  int rowCount(const ModelIndex& parent) const override {
    FsNode* pNode = parent.isValid()
                        ? static_cast<FsNode*>(parent.internalPointer())
                        : root_.get();
    if (!pNode->entry.is_directory()) {
      return 0;
    }
    populateChildren(pNode);
    return static_cast<int>(pNode->children.size());
  }

  int columnCount(const ModelIndex&) const override { return 4; }

  bool hasChildren(const ModelIndex& parent) const override {
    FsNode* pNode = parent.isValid()
                        ? static_cast<FsNode*>(parent.internalPointer())
                        : root_.get();
    return pNode->entry.is_directory();
  }

  // --- Data Extraction Pass ---
  std::any data(const ModelIndex& idx, ItemRole role) const override {
    if (!idx.isValid() || role != ItemRole::DisplayRole) {
      return {};
    }
    FsNode* node = static_cast<FsNode*>(idx.internalPointer());
    const auto& path = node->entry.path();

    switch (idx.column()) {
      case 0:
        return path.filename().string();  // Column 0: File/Folder Name
      case 1:                             // Column 1: Size String
        if (node->entry.is_directory()) {
          return std::string("<DIR>");
        }
        try {
          return formatSize(node->entry.file_size());
        } catch (...) {
          return std::string("0 B");
        }
      case 2:  // Column 2: Explicit Format Extension Type
        if (node->entry.is_directory()) {
          return std::string("Folder");
        }
        return path.has_extension() ? path.extension().string()
                                    : std::string("File");
      case 3:  // Column 3: Date Modified
        try {
          return formatTime(node->entry.last_write_time());
        } catch (...) {
          return std::string("---");
        }
      default:
        break;
    }
    return {};
  }

  std::any headerData(int section,
                      Orientation orient,
                      ItemRole role) const override {
    if (orient == Orientation::Horizontal && role == ItemRole::DisplayRole) {
      switch (section) {
        case 0:
          return std::string("File System Structure");
        case 1:
          return std::string("Size");
        case 2:
          return std::string("Format");
        case 3:
          return std::string("Date Modified");
        default:
          break;
      }
    }
    return AbstractItemModel::headerData(section, orient, role);
  }
};

class FileSystemRouterDelegate : public ItemDelegate {
 private:
  StyledTextDelegate name_dir_{StyledTextDelegate::Alignment::Left,
                               ftxui::Color::Yellow};
  StyledTextDelegate name_file_{StyledTextDelegate::Alignment::Left,
                                ftxui::Color::White};
  StyledTextDelegate meta_right_{StyledTextDelegate::Alignment::Right,
                                 ftxui::Color::CyanLight};
  StyledTextDelegate type_center_{StyledTextDelegate::Alignment::Center,
                                  ftxui::Color::GrayLight};

 public:
  ftxui::Element createWidget(const ModelIndex& index,
                              const AbstractItemModel* model) const override {
    FsNode* node = static_cast<FsNode*>(index.internalPointer());
    bool isDir = node->entry.is_directory();

    switch (index.column()) {
      case 0:  // Name
        return isDir ? name_dir_.createWidget(index, model)
                     : name_file_.createWidget(index, model);
      case 1:  // Size
        return meta_right_.createWidget(index, model);
      case 2:  // Type
        return type_center_.createWidget(index, model);
      case 3:  // Date
        return meta_right_.createWidget(index, model);
      default:
        return ftxui::text("");
    }
  }
};

}  // namespace ftxmodel

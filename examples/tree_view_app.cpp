#include <ftxmodel/tree_view.hpp>

#include "ftxmodel/proxy_item_delegate.hpp"
#include "ftxmodel/sort_filter_proxy_model.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"

using namespace ftxmodel;

struct FileNode {
  std::string name;
  bool is_directory;
  FileNode* parent = nullptr;
  std::vector<std::unique_ptr<FileNode>> children;

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

class FileTreeModel : public AbstractItemModel {
 private:
  std::unique_ptr<FileNode> root_;

 public:
  explicit FileTreeModel() {
    // Construct mock directories
    root_ = std::make_unique<FileNode>("root", true);

    auto src = std::make_unique<FileNode>("src", true, root_.get());
    src->children.push_back(
        std::make_unique<FileNode>("main.cpp", false, src.get()));
    src->children.push_back(
        std::make_unique<FileNode>("utils.hpp", false, src.get()));
    src->children.push_back(
        std::make_unique<FileNode>("aaa.hpp", false, src.get()));
    src->children.push_back(
        std::make_unique<FileNode>("v1.hpp", false, src.get()));

    auto docs = std::make_unique<FileNode>("docs", true, root_.get());
    docs->children.push_back(
        std::make_unique<FileNode>("README.md", false, docs.get()));

    root_->children.push_back(std::move(src));
    root_->children.push_back(std::move(docs));
    root_->children.push_back(
        std::make_unique<FileNode>("CMakeLists.txt", false, root_.get()));
  }

  ModelIndex index(int row, int col, const ModelIndex& parent) const override {
    if (row < 0 || col != 0) {
      return {};
    }
    FileNode* parentNode =
        parent.isValid() ? static_cast<FileNode*>(parent.internalPointer())
                         : root_.get();
    if (row >= static_cast<int>(parentNode->children.size())) {
      return {};
    }
    return createIndex(row, col, parentNode->children[(size_t)row].get());
  }

  ModelIndex parent(const ModelIndex& child) const override {
    if (!child.isValid()) {
      return {};
    }
    FileNode* childNode = static_cast<FileNode*>(child.internalPointer());
    FileNode* parentNode = childNode->parent;
    if (!parentNode || parentNode == root_.get()) {
      return {};
    }
    return createIndex(parentNode->row(), 0, parentNode);
  }

  int rowCount(const ModelIndex& parent) const override {
    FileNode* parentNode =
        parent.isValid() ? static_cast<FileNode*>(parent.internalPointer())
                         : root_.get();
    return parentNode->is_directory
               ? static_cast<int>(parentNode->children.size())
               : 0;
  }

  int columnCount(const ModelIndex&) const override { return 1; }

  std::any data(const ModelIndex& index, ItemRole role) const override {
    if (!index.isValid() || role != ItemRole::DisplayRole) {
      return {};
    }
    FileNode* node = static_cast<FileNode*>(index.internalPointer());
    return node->name;
  }

  bool setData(const ModelIndex&, const std::any&, ItemRole) override {
    return false;
  }
};

int main() {
  auto screen = ftxui::ScreenInteractive::TerminalOutput();

  auto model = std::make_shared<FileTreeModel>();

  auto proxy = std::make_shared<SortFilterProxyModel>();
  proxy->setSourceModel(model);
  proxy->sort(0, false);

  auto delegate = std::make_shared<StyledTextDelegate>(Alignment::Left,
                                                       ftxui::Color::Yellow);

  TreeView treeView;
  treeView.setItemDelegate(std::make_shared<ProxyItemDelegate>(delegate));
  treeView.setModel(proxy);

  auto baseComp = ftxui::Make<ftxui::ComponentBase>();
  auto appController = ftxui::CatchEvent(baseComp, [&](ftxui::Event event) {
    if (event == ftxui::Event::ArrowUp) {
      treeView.moveUp();
      return true;
    }
    if (event == ftxui::Event::ArrowDown) {
      treeView.moveDown();
      return true;
    }
    if (event == ftxui::Event::ArrowRight) {
      treeView.moveRight();
      return true;
    }  // Expands Branch
    if (event == ftxui::Event::ArrowLeft) {
      treeView.moveLeft();
      return true;
    }  // Collapses Branch
    if (event == ftxui::Event::Escape) {
      screen.Exit();
      return true;
    }
    return false;
  });
  auto appLayout = ftxui::Renderer(appController, [&]() {
    return ftxui::vbox({ftxui::text(" Interactive Collapsible TreeView ") |
                            ftxui::bold | ftxui::center,
                        ftxui::separator(),
                        treeView.Render() | ftxui::xflex_grow,
                        ftxui::separator(),
                        ftxui::text(" Controls: [→] Expand | [←] Collapse/Jump "
                                    "Parent | [↑/↓] Navigate") |
                            ftxui::dim});
  });
  screen.Loop(appLayout);
  return 0;
}

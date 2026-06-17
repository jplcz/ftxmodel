#include <algorithm>
#include <any>
#include <memory>
#include <string>
#include <vector>
#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"

#include <ftxmodel/tree_view.hpp>

using namespace ftxmodel;

// ============================================================================
// DYNAMIC NODE STRUCTS (TASK AND THREAD POLYS)
// ============================================================================
enum class NodeType { Task, Thread };

struct BaseNode {
  std::string name;
  NodeType type;
  BaseNode* parent = nullptr;
  std::vector<std::unique_ptr<BaseNode>> children;

  virtual ~BaseNode() = default;
  BaseNode(std::string n, NodeType t, BaseNode* p = nullptr)
      : name(std::move(n)), type(t), parent(p) {}

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

// Branch Node: Tasks have Memory attributes
struct TaskNode : public BaseNode {
  std::string memoryUsage;

  TaskNode(std::string name, std::string mem, BaseNode* p = nullptr)
      : BaseNode(std::move(name), NodeType::Task, p),
        memoryUsage(std::move(mem)) {}
};

// Leaf Node: Threads have specific IDs, performance metrics, and states
struct ThreadNode : public BaseNode {
  int threadId;
  float cpuLoad;
  bool isActive;

  ThreadNode(std::string name,
             int id,
             float cpu,
             bool active,
             BaseNode* p = nullptr)
      : BaseNode(std::move(name), NodeType::Thread, p),
        threadId(id),
        cpuLoad(cpu),
        isActive(active) {}
};

// ============================================================================
// CONCRETE DATA MODEL IMPLEMENTATION
// ============================================================================
class TaskThreadModel : public AbstractItemModel {
 private:
  std::unique_ptr<BaseNode> root_;

 public:
  TaskThreadModel() {
    root_ = std::make_unique<BaseNode>("Root", NodeType::Task);

    // Task 1 Configuration
    auto task1 = std::make_unique<TaskNode>("Web Browser Engine", "512.4 MB",
                                            root_.get());
    task1->children.push_back(std::make_unique<ThreadNode>(
        "Main Render Window", 2041, 6.2f, true, task1.get()));
    task1->children.push_back(std::make_unique<ThreadNode>(
        "V8 Engine Worker", 2042, 18.7f, true, task1.get()));
    task1->children.push_back(std::make_unique<ThreadNode>(
        "IPC Channel Pipe", 2043, 0.0f, false, task1.get()));

    // Task 2 Configuration
    auto task2 = std::make_unique<TaskNode>("Database Core Daemon", "1.8 GB",
                                            root_.get());
    task2->children.push_back(std::make_unique<ThreadNode>(
        "SQL Parser Execution", 7109, 2.1f, true, task2.get()));
    task2->children.push_back(std::make_unique<ThreadNode>(
        "Async Disk Flusher", 7110, 0.4f, true, task2.get()));

    root_->children.push_back(std::move(task1));
    root_->children.push_back(std::move(task2));
  }

  // --- Navigation Layout Hooks ---
  ModelIndex index(int row, int col, const ModelIndex& parent) const override {
    if (row < 0 || col < 0 || col >= 4) {
      return {};
    }
    BaseNode* pNode = parent.isValid()
                          ? static_cast<BaseNode*>(parent.internalPointer())
                          : root_.get();
    if (row >= static_cast<int>(pNode->children.size())) {
      return {};
    }
    return createIndex(row, col, pNode->children[(size_t)row].get());
  }

  ModelIndex parent(const ModelIndex& child) const override {
    if (!child.isValid()) {
      return {};
    }
    BaseNode* cNode = static_cast<BaseNode*>(child.internalPointer());
    BaseNode* pNode = cNode->parent;
    if (!pNode || pNode == root_.get()) {
      return {};
    }
    return createIndex(pNode->row(), 0, pNode);
  }

  int rowCount(const ModelIndex& parent) const override {
    BaseNode* pNode = parent.isValid()
                          ? static_cast<BaseNode*>(parent.internalPointer())
                          : root_.get();
    return static_cast<int>(pNode->children.size());
  }

  int columnCount(const ModelIndex&) const override { return 4; }

  // --- Dynamic Column Matrix Resolution ---
  std::any data(const ModelIndex& idx, ItemRole role) const override {
    if (!idx.isValid() || role != ItemRole::DisplayRole) {
      return {};
    }
    BaseNode* node = static_cast<BaseNode*>(idx.internalPointer());

    if (node->type == NodeType::Task) {
      auto* task = static_cast<TaskNode*>(node);
      switch (idx.column()) {
        case 0:
          return task->name;  // Column 0: String Title
        case 1:
          return task->memoryUsage;  // Column 1: Memory Footprint
        default:
          return std::string(
              "");  // Leave columns 2 & 3 completely empty for Tasks
      }
    } else if (node->type == NodeType::Thread) {
      auto* thread = static_cast<ThreadNode*>(node);
      switch (idx.column()) {
        case 0:
          return thread->name;  // Column 0: String Title
        case 1:
          return std::string("TID: ") +
                 std::to_string(
                     thread->threadId);  // Column 1: Formatted String
        case 2:
          return static_cast<double>(
              thread->cpuLoad);  // Column 2: Performance Metrics (double)
        case 3:
          return thread->isActive;  // Column 3: Active State Flag (bool)
        default:
          return {};
      }
    }
    return {};
  }

  std::any headerData(int section,
                      Orientation orient,
                      ItemRole role) const override {
    if (orient == Orientation::Horizontal && role == ItemRole::DisplayRole) {
      switch (section) {
        case 0:
          return std::string("Subsystem Allocation Context");
        case 1:
          return std::string("Resource Metrics");
        case 2:
          return std::string("CPU Workload");
        case 3:
          return std::string("State");
        default:
          break;
      }
    }
    return AbstractItemModel::headerData(section, orient, role);
  }

  bool setData(const ModelIndex&, const std::any&, ItemRole) override {
    return false;
  }
};

// ============================================================================
// MUX ROUTER DELEGATE IMPLEMENTATION
// ============================================================================
class TaskThreadRouterDelegate : public ItemDelegate {
 private:
  StyledTextDelegate left_aligned_text_{Alignment::Left, ftxui::Color::White};
  StyledTextDelegate right_aligned_meta_{Alignment::Right,
                                         ftxui::Color::CyanLight};
  ProgressBarDelegate cpu_gauge_bar_{
      30.0f, ftxui::Color::Yellow1};  // Normalized scaling capped at 30%
  CheckBoxDelegate active_state_checkbox_;

 public:
  ftxui::Element createWidget(const ModelIndex& index,
                              const AbstractItemModel* model) const override {
    std::any val = model->data(index, ItemRole::DisplayRole);
    if (!val.has_value()) {
      return ftxui::text("");
    }

    // Evaluate the Variant Type signature to map to the correct specialized
    // delegate
    if (val.type() == typeid(std::string)) {
      if (index.column() == 1) {
        return right_aligned_meta_.createWidget(index, model);
      }
      return left_aligned_text_.createWidget(index, model);
    }
    if (val.type() == typeid(double)) {
      return cpu_gauge_bar_.createWidget(index, model);
    }
    if (val.type() == typeid(bool)) {
      return active_state_checkbox_.createWidget(index, model);
    }

    return ftxui::text("");
  }

  ftxui::Dimensions sizeHint(const ModelIndex& index,
                             const AbstractItemModel* model) const override {
    std::any val = model->data(index, ItemRole::DisplayRole);
    if (!val.has_value()) {
      return ftxui::Dimensions{0, 1};
    }

    // Mirror the type evaluations exactly to route geometric hints forward
    if (val.type() == typeid(std::string)) {
      if (index.column() == 1) {
        return right_aligned_meta_.sizeHint(index, model);
      }
      return left_aligned_text_.sizeHint(index, model);
    }
    if (val.type() == typeid(double)) {
      return cpu_gauge_bar_.sizeHint(index, model);
    }
    if (val.type() == typeid(bool)) {
      return active_state_checkbox_.sizeHint(index, model);
    }

    return ftxui::Dimensions{0, 1};
  }
};

// ============================================================================
// MAIN APP LOOP INITIALIZATION RUNNER
// ============================================================================

int main() {
  auto screen = ftxui::ScreenInteractive::TerminalOutput();

  // Instantiate the explicit application-specific structures
  auto model = std::make_shared<TaskThreadModel>();
  auto delegate = std::make_shared<TaskThreadRouterDelegate>();

  // Configure the multi-column TreeView
  auto treeView = std::make_shared<TreeView>();
  treeView->setItemDelegate(delegate);
  treeView->setModel(model);
  treeView->setShowHorizontalHeaders(true);

  // Bind Keyboard Events to full spatial navigation loops
  auto loopController = ftxui::CatchEvent(treeView, [&](ftxui::Event event) {
    if (event == ftxui::Event::Escape) {
      screen.Exit();
      return true;
    }
    return false;
  });

  // Draw out the final app viewport panel canvas
  auto appLayout = ftxui::Renderer(loopController, [&]() {
    return ftxui::vbox(
        {ftxui::text(" Task and Thread Subsystem Dashboard ") | ftxui::bold |
             ftxui::center | ftxui::bgcolor(ftxui::Color::GrayDark),
         ftxui::separator(), treeView->Render(), ftxui::separator(),
         ftxui::text(" Controls: [→] Expand | [←] Collapse/Jump Parent | [↑/↓] "
                     "Navigate Rows") |
             ftxui::dim});
  });

  screen.Loop(appLayout);
  return 0;
}

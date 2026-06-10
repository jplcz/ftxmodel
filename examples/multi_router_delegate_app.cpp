#include <algorithm>
#include <any>
#include <ftxmodel/multi_router_delegate.hpp>
#include <ftxmodel/table_view.hpp>
#include <ftxmodel/tree_view.hpp>
#include <memory>
#include <string>
#include <vector>
#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"

using namespace ftxmodel;

// ============================================================================
// DATA STRUCTURES & MODEL FOR PERFORMANCE MONITOR (Flat 2D Grid)
// ============================================================================
struct ServerMetric {
  std::string serviceName;
  bool isHealthy;
  double cpuUtilization;
};

class ServerMetricsModel : public AbstractItemModel {
 private:
  std::vector<ServerMetric> metrics_;

 public:
  ServerMetricsModel() {
    metrics_ = {{"Nginx Web Edge", true, 12.5},
                {"PostgreSQL Cluster", true, 45.8},
                {"Redis Cache Layer", true, 4.2},
                {"Auth Microservice", false, 0.0}};
  }

  ModelIndex index(int row, int col, const ModelIndex& parent) const override {
    if (parent.isValid() || row < 0 ||
        row >= static_cast<int>(metrics_.size()) || col < 0 || col >= 3) {
      return {};
    }
    return createIndex(row, col, nullptr);
  }
  ModelIndex parent(const ModelIndex&) const override { return {}; }
  int rowCount(const ModelIndex& parent) const override {
    return parent.isValid() ? 0 : static_cast<int>(metrics_.size());
  }
  int columnCount(const ModelIndex&) const override { return 3; }

  std::any data(const ModelIndex& idx, ItemRole role) const override {
    if (!idx.isValid() || role != ItemRole::DisplayRole) {
      return {};
    }
    const auto& metric = metrics_[(size_t)idx.row()];
    switch (idx.column()) {
      case 0:
        return metric.serviceName;
      case 1:
        return metric.isHealthy;
      case 2:
        return metric.cpuUtilization;
      default:
        return {};
    }
  }

  std::any headerData(int section,
                      Orientation orient,
                      ItemRole role) const override {
    if (orient == Orientation::Horizontal && role == ItemRole::DisplayRole) {
      switch (section) {
        case 0:
          return std::string("Service Name");
        case 1:
          return std::string("Online");
        case 2:
          return std::string("Core CPU Load");
      }
    }
    return {};
  }
  bool setData(const ModelIndex&, const std::any&, ItemRole) override {
    return false;
  }
};

// ============================================================================
// DATA STRUCTURES & MODEL FOR CONFIG TREE EXPLORER (Hierarchical Multi-Type
// Tree)
// ============================================================================
struct ConfigNode {
  std::string name;
  std::any dynamicValue;  // Can contain string (path/desc), double (size KB),
                          // or bool (writeable)
  ConfigNode* parent = nullptr;
  std::vector<std::unique_ptr<ConfigNode>> children;

  ConfigNode(std::string n, std::any val, ConfigNode* p = nullptr)
      : name(std::move(n)), dynamicValue(std::move(val)), parent(p) {}

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

class ConfigTreeModel : public AbstractItemModel {
 private:
  std::unique_ptr<ConfigNode> root_;

 public:
  ConfigTreeModel() {
    root_ = std::make_unique<ConfigNode>("Root", std::string("Root Context"));

    // Folder 1: Network Configuration
    auto netFolder = std::make_unique<ConfigNode>(
        "etc/network/", std::string("Network Configuration"), root_.get());
    netFolder->children.push_back(std::make_unique<ConfigNode>(
        "interfaces.conf", std::string("/etc/network/interfaces"),
        netFolder.get()));
    netFolder->children.push_back(std::make_unique<ConfigNode>(
        "Allocated Size", 42.5, netFolder.get()));  // double type
    netFolder->children.push_back(std::make_unique<ConfigNode>(
        "Write Permissions", true, netFolder.get()));  // bool type

    // Folder 2: Security Credentials
    auto secureFolder = std::make_unique<ConfigNode>(
        "etc/security/", std::string("Access Rules"), root_.get());
    secureFolder->children.push_back(std::make_unique<ConfigNode>(
        "ssl_certs.pem", std::string("/etc/security/certs"),
        secureFolder.get()));
    secureFolder->children.push_back(std::make_unique<ConfigNode>(
        "Allocated Size", 128.0, secureFolder.get()));  // double type
    secureFolder->children.push_back(std::make_unique<ConfigNode>(
        "Write Permissions", false, secureFolder.get()));  // bool type

    root_->children.push_back(std::move(netFolder));
    root_->children.push_back(std::move(secureFolder));
  }

  ModelIndex index(int row, int col, const ModelIndex& parent) const override {
    if (row < 0 || col < 0 || col >= 2) {
      return {};
    }
    ConfigNode* pNode = parent.isValid()
                            ? static_cast<ConfigNode*>(parent.internalPointer())
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
    ConfigNode* cNode = static_cast<ConfigNode*>(child.internalPointer());
    ConfigNode* pNode = cNode->parent;
    if (!pNode || pNode == root_.get()) {
      return {};
    }
    return createIndex(pNode->row(), 0, pNode);
  }

  int rowCount(const ModelIndex& parent) const override {
    ConfigNode* pNode = parent.isValid()
                            ? static_cast<ConfigNode*>(parent.internalPointer())
                            : root_.get();
    return static_cast<int>(pNode->children.size());
  }

  int columnCount(const ModelIndex&) const override { return 2; }

  std::any data(const ModelIndex& idx, ItemRole role) const override {
    if (!idx.isValid() || role != ItemRole::DisplayRole) {
      return {};
    }
    ConfigNode* node = static_cast<ConfigNode*>(idx.internalPointer());

    // Column 0 always renders the key structural string identifier name
    if (idx.column() == 0) {
      return node->name;
    }

    // Column 1 yields a heterogeneous variant payload (String, Double, or Bool)
    return node->dynamicValue;
  }

  std::any headerData(int section,
                      Orientation orient,
                      ItemRole role) const override {
    if (orient == Orientation::Horizontal && role == ItemRole::DisplayRole) {
      return section == 0 ? std::string("Configuration Resource")
                          : std::string("Property Metadata Value");
    }
    return {};
  }
  bool setData(const ModelIndex&, const std::any&, ItemRole) override {
    return false;
  }
};

int main() {
  auto screen = ftxui::ScreenInteractive::TerminalOutput();

  // 1. Instantiate Data Models
  auto metricsModel = std::make_shared<ServerMetricsModel>();
  auto configModel = std::make_shared<ConfigTreeModel>();

  // 2. Instantiate Base Presentation Delegates
  auto textLeft = std::make_shared<StyledTextDelegate>(Alignment::Left,
                                                       ftxui::Color::White);
  auto textRight = std::make_shared<StyledTextDelegate>(
      Alignment::Right, ftxui::Color::CyanLight);
  auto checkDel = std::make_shared<CheckBoxDelegate>();
  auto progressDel =
      std::make_shared<ProgressBarDelegate>(100.0f, ftxui::Color::Yellow1);

  // ========================================================================
  // USE CASE 1: Build the Table Layout using MultiColumnRouterDelegate
  // ========================================================================
  auto tableRouter = std::make_shared<MultiColumnRouterDelegate>();
  tableRouter
      ->registerColumn(0, textLeft)     // Service Name Column
      .registerColumn(1, checkDel)      // Health Checkbox Column
      .registerColumn(2, progressDel);  // CPU Load Gauge Column

  TableView tableView;
  tableView.setItemDelegate(tableRouter);
  tableView.setModel(metricsModel);
  tableView.setShowHorizontalHeaders(true);

  // ========================================================================
  // USE CASE 2: Build the Tree Layout using MultiTypeRouterDelegate
  // ========================================================================
  auto treeRouter = std::make_shared<MultiTypeRouterDelegate>();
  treeRouter
      ->registerType(typeid(std::string),
                     textLeft)  // Strings map to normal text
      .registerType(
          typeid(double),
          progressDel)  // Doubles automatically render as progress gauges
      .registerType(
          typeid(bool),
          checkDel);  // Booleans automatically render as interactive checkboxes

  TreeView treeView;
  treeView.setItemDelegate(treeRouter);
  treeView.setModel(configModel);
  treeView.setShowHorizontalHeaders(true);
  treeView.highlightStyle()->setSelectionBehavior(
      SelectionBehavior::SelectRows);

  // ========================================================================
  // INTERACTION LAYER (Focus Navigation Controller Logic)
  // ========================================================================
  int activePanelIndex = 0;  // 0 = Performance Table, 1 = Config Tree

  auto baseComp = ftxui::Make<ftxui::ComponentBase>();
  auto appController = ftxui::CatchEvent(baseComp, [&](ftxui::Event event) {
    // Toggle active panel focus using the Tab key
    if (event == ftxui::Event::Tab) {
      activePanelIndex = (activePanelIndex + 1) % 2;
      if (activePanelIndex == 0) {
        tableView.TakeFocus();
      } else {
        treeView.TakeFocus();
      }
      return true;
    }

    // Direct inputs to the currently active panel
    if (activePanelIndex == 0) {
      if (event == ftxui::Event::ArrowUp) {
        tableView.moveUp();
        return true;
      }
      if (event == ftxui::Event::ArrowDown) {
        tableView.moveDown();
        return true;
      }
    } else {
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
      }  // Opens folder node
      if (event == ftxui::Event::ArrowLeft) {
        treeView.moveLeft();
        return true;
      }  // Closes folder node
    }

    if (event == ftxui::Event::Escape) {
      screen.Exit();
      return true;
    }
    return false;
  });

  // Decorate views with borders based on focus state
  auto appLayout = ftxui::Renderer(appController, [&]() {
    ftxui::Element topTable = tableView.Render();
    ftxui::Element bottomTree = treeView.Render();

    if (activePanelIndex == 0) {
      topTable = topTable | ftxui::color(ftxui::Color::Cyan) | ftxui::bold;
    } else {
      bottomTree = bottomTree | ftxui::color(ftxui::Color::Cyan) | ftxui::bold;
    }

    return ftxui::vbox(
        {ftxui::text(" Multi-Delegate System Administration Core Console ") |
             ftxui::bold | ftxui::center | ftxui::bgcolor(ftxui::Color::Blue),
         ftxui::text(" Active Subsystem Cluster Metrics ") | ftxui::dim,
         topTable,
         ftxui::text(" Configuration Node Attributes File Hierarchy ") |
             ftxui::dim,
         bottomTree, ftxui::separator(),
         ftxui::text(" Navigation: [Tab] Switch Panels | [↑/↓] Browse Rows | "
                     "[←/→] Expand/Collapse Folders ") |
             ftxui::dim});
  });

  screen.Loop(appLayout);
  return 0;
}

#include <ftxmodel/tree_view.hpp>
#include <memory>
#include <string>

#include "ftxmodel/proxy_item_delegate.hpp"
#include "ftxmodel/sort_filter_proxy_model.hpp"
#include "ftxmodel/tree_table_model.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"

using namespace ftxmodel;

struct Department {
  std::string deptName;
  std::string costCenter;
};

struct Employee {
  std::string name;
  std::string jobTitle;
};

int main() {
  auto screen = ftxui::ScreenInteractive::TerminalOutput();

  auto model = std::make_shared<TreeTableModel<Department, Employee>>(
      std::vector<std::string>{"Organization Structure", "Metadata / Context"});

  model->setColumnLogic(0, [](const auto& row, int, ItemRole role) -> std::any {
    if (role != ItemRole::DisplayRole) {
      return {};
    }

    return std::visit(
        [](const auto& actualNode) -> std::any {
          using T = std::decay_t<decltype(actualNode)>;
          if constexpr (std::is_same_v<T, Department>) {
            return actualNode.deptName;
          }
          if constexpr (std::is_same_v<T, Employee>) {
            return actualNode.name;
          }
          return std::string("");
        },
        row);
  });

  // Configure an optional second layout column for multi-column dashboards
  model->setColumnLogic(1, [](const auto& row, int, ItemRole role) -> std::any {
    if (role != ItemRole::DisplayRole) {
      return {};
    }

    return std::visit(
        [](const auto& actualNode) -> std::any {
          using T = std::decay_t<decltype(actualNode)>;
          if constexpr (std::is_same_v<T, Department>) {
            return "CC: " + actualNode.costCenter;
          }
          if constexpr (std::is_same_v<T, Employee>) {
            return actualNode.jobTitle;
          }
          return std::string("");
        },
        row);
  });

  model->setKeyExtractor([](const auto& row) -> UniqueNodeId {
    return std::visit(
        [](const auto& actualNode) -> UniqueNodeId {
          using T = std::decay_t<decltype(actualNode)>;
          if constexpr (std::is_same_v<T, Department>) {
            return actualNode.deptName;
          }
          if constexpr (std::is_same_v<T, Employee>) {
            return actualNode.name;
          }
          return {nullptr};
        },
        row);
  });

  // ========================================================================
  // Populate Topology Hierarchy
  // ========================================================================
  auto* rootNode = model->rootNode();

  // Add Department A (Engineering)
  auto* engDept = model->appendChildItem(
      rootNode, Department{"Engineering Branch", "ENG-802"});
  model->appendChildItem(engDept,
                         Employee{"Alice Smith", "Principal Architect"});
  model->appendChildItem(engDept,
                         Employee{"Charlie Brown", "Senior QA Engineer"});
  model->appendChildItem(engDept, Employee{"Bob Johnson", "UI Developer"});

  // Add Department B (Operations)
  auto* opsDept = model->appendChildItem(
      rootNode, Department{"Operations Division", "OPS-411"});
  model->appendChildItem(opsDept,
                         Employee{"Diana Prince", "Site Reliability Lead"});

  // ========================================================================
  // View Pipeline Instantiation
  // ========================================================================

  // Wrap our model in the SortFilterProxyModel for sorted view caches
  auto proxy = std::make_shared<SortFilterProxyModel>();
  proxy->setSourceModel(model);
  proxy->sort(0, false);  // Sort items alphabetically by column 0

  auto delegate = std::make_shared<StyledTextDelegate>(Alignment::Left,
                                                       ftxui::Color::Yellow);

  auto treeView = std::make_shared<TreeView>();
  treeView->setItemDelegate(std::make_shared<ProxyItemDelegate>(delegate));
  treeView->setModel(proxy);

  // Layout Rendering Setup
  auto baseComp = ftxui::Container::Vertical({treeView});

  auto appController = ftxui::CatchEvent(baseComp, [&](ftxui::Event event) {
    if (event == ftxui::Event::Escape) {
      screen.Exit();
      return true;
    }
    return false;
  });

  auto appLayout = ftxui::Renderer(appController, [&]() {
    return ftxui::vbox(
        {ftxui::text(" Interactive Collapsible TreeView ") | ftxui::bold |
             ftxui::center,
         ftxui::separator(), treeView->Render() | ftxui::xflex_grow,
         ftxui::separator(),
         ftxui::text(" Controls: [→] Expand Branch | [←] Collapse Branch / "
                     "Jump Up | [↑/↓] Navigate") |
             ftxui::dim});
  });

  screen.Loop(appLayout);
  return 0;
}

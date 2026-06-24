#include <any>
#include <ftxmodel/tree_view.hpp>
#include <memory>
#include <string>

#include "ftxmodel/polymorphic_tree_table_model.hpp"
#include "ftxmodel/proxy_item_delegate.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"

using namespace ftxmodel;

struct CloudRegion {
  std::string zoneName;
  std::string provider;
};

struct ComputeCluster {
  std::string clusterId;
  int hostCount;
};

struct VirtualMachine {
  std::string hostname;
  std::string ipAddress;
  bool isActive;
};

int main() {
  auto screen = ftxui::ScreenInteractive::TerminalOutput();

  // Instantiate the open polymorphic model for a 2-column view layout
  auto model =
      std::make_shared<PolymorphicTreeTableModel>(std::vector<std::string>{
          "Infrastructure Asset Nodes", "Details / Address Trace"});

  // ========================================================================
  // Register Type Handlers and Decouple Logic Rules Runtime
  // ========================================================================

  // Logic Rules for CloudRegion
  model->registerTypeHandler<CloudRegion>(
      [](const std::any& rawAny, int column, ItemRole role) -> std::any {
        if (role != ItemRole::DisplayRole) {
          return {};
        }
        const auto& region = std::any_cast<const CloudRegion&>(rawAny);

        if (column == 0) {
          return "🌐 Region: " + region.zoneName;
        }
        if (column == 1) {
          return region.provider;
        }
        return {};
      });

  // Logic Rules for ComputeCluster
  model->registerTypeHandler<ComputeCluster>(
      [](const std::any& rawAny, int column, ItemRole role) -> std::any {
        if (role != ItemRole::DisplayRole) {
          return {};
        }
        const auto& cluster = std::any_cast<const ComputeCluster&>(rawAny);

        if (column == 0) {
          return "  └── 🖥️ Cluster: " + cluster.clusterId;
        }
        if (column == 1) {
          return std::to_string(cluster.hostCount) + " Active Hypervisors";
        }
        return {};
      });

  // Logic Rules for VirtualMachine
  model->registerTypeHandler<VirtualMachine>(
      [](const std::any& rawAny, int column, ItemRole role) -> std::any {
        if (role != ItemRole::DisplayRole) {
          return {};
        }
        const auto& vm = std::any_cast<const VirtualMachine&>(rawAny);

        if (column == 0) {
          return "        └── 📦 " + vm.hostname;
        }
        if (column == 1) {
          return vm.ipAddress + (vm.isActive ? " [RUNNING]" : " [STOPPED]");
        }
        return {};
      });

  // ========================================================================
  // Configure Universal Key Extractor mapping typeid branches safely
  // ========================================================================
  model->setKeyExtractor(
      [](const TreeTableModel<std::any>::RowData& var) -> UniqueNodeId {
        const std::any& rawAny = std::get<std::any>(var);

        if (rawAny.type() == typeid(CloudRegion)) {
          return std::any_cast<const CloudRegion&>(rawAny).zoneName;
        }
        if (rawAny.type() == typeid(ComputeCluster)) {
          return std::any_cast<const ComputeCluster&>(rawAny).clusterId;
        }
        if (rawAny.type() == typeid(VirtualMachine)) {
          return std::any_cast<const VirtualMachine&>(rawAny).hostname;
        }
        return {nullptr};
      });

  // ========================================================================
  // Hydrate Open Heterogeneous Topology Matrix
  // ========================================================================
  auto* root = model->rootNode();

  // Box native structures into the model implicitly via std::any parameters
  auto* regUS =
      model->appendChildItem(root, CloudRegion{"us-east-1", "AWS Backend"});
  auto* clusterA =
      model->appendChildItem(regUS, ComputeCluster{"k8s-prod-alpha", 12});
  model->appendChildItem(clusterA,
                         VirtualMachine{"api-gateway-01", "10.0.1.4", true});
  model->appendChildItem(clusterA,
                         VirtualMachine{"worker-node-99", "10.0.1.85", false});

  auto* regEU = model->appendChildItem(
      root, CloudRegion{"eu-central-1", "GCP Core Engine"});
  auto* clusterB =
      model->appendChildItem(regEU, ComputeCluster{"legacy-monolith", 2});
  model->appendChildItem(
      clusterB, VirtualMachine{"db-primary-srv", "192.168.4.10", true});

  // ========================================================================
  // Connect Layout View Architecture
  // ========================================================================
  auto delegate =
      std::make_shared<StyledTextDelegate>(Alignment::Left, ftxui::Color::Cyan);

  auto treeView = std::make_shared<TreeView>();
  treeView->setItemDelegate(std::make_shared<ProxyItemDelegate>(delegate));
  treeView->setModel(model);

  // Assembly Frame Layout Panel
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
        {ftxui::text(" Open Polymorphic Model-View Tree Inspector ") |
             ftxui::bold | ftxui::center,
         ftxui::separator(), treeView->Render() | ftxui::xflex_grow,
         ftxui::separator(),
         ftxui::text(" Controls: [→] Expand Sub-Cluster | [←] Collapse Node | "
                     "[↑/↓] Navigate | [ESC] Exit") |
             ftxui::dim});
  });

  screen.Loop(appLayout);
  return 0;
}

#include <ftxmodel/json_property_tree_model.hpp>
#include <ftxmodel/scrollable_tree_view.hpp>

#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"

using namespace ftxmodel;

constexpr std::string_view example_json = R"JSON(

{
  "a_cluster_identity": {
    "cluster_name": "prod-eu-central-01",
    "datacenter_zone": "frankfurt-am-main",
    "deployment_tier": "production"
  },
  "b_network_topology": {
    "dns_primary_resolver": "10.10.0.2",
    "dns_secondary_resolver": "10.10.0.3",
    "domain_suffix": "internal.lan",
    "firewall_configuration": {
      "enabled_state": true,
      "policy_default": "DROP"
    }
  },
  "c_microservices_pool": [
    {
      "service_name": "auth-service",
      "target_replicas": 3
    },
    {
      "service_name": "gateway-service",
      "target_replicas": 2
    }
  ],
  "d_telemetry_metrics": {
    "active_alerts_count": 0,
    "health_status_tag": "HEALTHY",
    "uptime_seconds": 1572800
  },
  "e_system_version": "v2.4.1-a8f3c12"
}

)JSON";

int main() {
  auto screen = ftxui::ScreenInteractive::TerminalOutput();

  auto model = std::make_shared<JsonPropertyTreeModel>(
      nlohmann::json::parse(example_json));
  auto delegate = std::make_shared<StyledTextDelegate>(Alignment::Left,
                                                       ftxui::Color::Yellow);

  auto treeView = std::make_shared<ScrollableTreeView>();
  // treeView->setItemDelegate(delegate);
  treeView->setModel(model);

  auto baseComp = ftxui::Container::Vertical({treeView});
  auto appController = ftxui::CatchEvent(baseComp, [&](ftxui::Event event) {
    if (event == ftxui::Event::Escape) {
      screen.Exit();
      return true;
    }
    return false;
  });
  auto appLayout = ftxui::Renderer(appController, [&]() {
    return ftxui::vbox({ftxui::text(" Test JSON Property Tree Model ") |
                            ftxui::bold | ftxui::center,
                        ftxui::separator(),
                        treeView->Render() | ftxui::xflex_shrink,
                        ftxui::separator(),
                        ftxui::text(" Controls: [→] Expand | [←] Collapse/Jump "
                                    "Parent | [↑/↓] Navigate") |
                            ftxui::dim});
  });
  screen.Loop(appLayout);
  return 0;
}

#include <ftxmodel/json_property_tree_model.hpp>
#include <ftxmodel/tree_view.hpp>

#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"

using namespace ftxmodel;

constexpr std::string_view example_json = R"JSON(

{
  "cluster_id": "us-east-prod-01",
  "operational_state": true,
  "refresh_interval_seconds": 30.5,
  "maintenance_window": null,
  "metrics_summary": {
    "total_cpu_utilization": 0.74,
    "active_alerts_count": 2,
    "status_tag": "WARNING"
  },
  "nodes": [
    {
      "hostname": "node-edge-alpha",
      "ip_address": "10.0.1.15",
      "load_factor": 0.64,
      "services": ["auth-gateway", "logging-aggregator"]
    },
    {
      "hostname": "node-edge-beta",
      "ip_address": "10.0.1.16",
      "load_factor": 0.88,
      "services": ["payment-processor", "search-indexing"]
    },
    {
      "hostname": "node-core-storage",
      "ip_address": "10.0.2.100",
      "load_factor": 0.42,
      "services": ["user-profile-db"]
    }
  ],
  "load_balancer": {
    "strategy": "round-robin",
    "connections": 1420,
    "sticky_sessions": false
  }
}

)JSON";

int main() {
  auto screen = ftxui::ScreenInteractive::TerminalOutput();

  auto model = std::make_shared<JsonPropertyTreeModel>(
      nlohmann::json::parse(example_json));
  auto delegate = std::make_shared<StyledTextDelegate>(Alignment::Left,
                                                       ftxui::Color::Yellow);

  TreeView treeView([&]() { screen.PostEvent(ftxui::Event::Custom); });
  treeView.setItemDelegate(delegate);
  treeView.setModel(model);

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
    return ftxui::vbox({ftxui::text(" Test JSON Property Tree Model ") |
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

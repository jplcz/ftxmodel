#include <ftxmodel/flatten_tree_proxy_model.hpp>
#include <ftxmodel/table_view.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include "tree_source_model.hpp"

using namespace ftxui;
using namespace ftxmodel;

int main() {
  auto screen = ftxui::ScreenInteractive::TerminalOutput();

  // 1. Initialize our nested underlying source model
  auto source_model = std::make_shared<TreeSourceModel>();

  // 2. Wrap it with your 1D linear FlattenTreeProxyModel
  auto flat_proxy = std::make_shared<FlattenTreeProxyModel>();
  flat_proxy->setSourceModel(source_model);

  // Initial state: start with some nodes visible and expanded
  flat_proxy->expand(0);  // Expands "src"

  TableView tableView([&]() { screen.PostEvent(ftxui::Event::Custom); });
  tableView.setItemDelegate(std::make_shared<ftxmodel::StyledTextDelegate>());
  tableView.setModel(flat_proxy.get());

  auto baseComponent = ftxui::Make<ftxui::ComponentBase>();
  auto appController =
      ftxui::CatchEvent(baseComponent, [&](ftxui::Event event) {
        if (event == ftxui::Event::ArrowUp) {
          tableView.moveUp();
          return true;
        }
        if (event == ftxui::Event::ArrowDown) {
          tableView.moveDown();
          return true;
        }
        if (event == ftxui::Event::ArrowLeft) {
          tableView.moveLeft();
          return true;
        }
        if (event == ftxui::Event::ArrowRight) {
          tableView.moveRight();
          return true;
        }
        if (event == ftxui::Event::Escape) {
          screen.Exit();
          return true;
        }
        if (event == ftxui::Event::Character('+')) {
          flat_proxy->expandAll();
          return true;
        }
        if (event == ftxui::Event::Character('-')) {
          flat_proxy->collapseAll();
          return true;
        }
        return false;
      });

  auto appLayout = ftxui::Renderer(appController, [&]() {
    return ftxui::vbox({ftxui::text(" Test for FlattenTreeProxyModel ") |
                            ftxui::bold | ftxui::center,
                        tableView.Render()});
  });

  screen.Loop(appLayout);
  return 0;
}

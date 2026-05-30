#include <ftxmodel/flatten_tree_proxy_model.hpp>
#include <ftxmodel/sort_filter_proxy_model.hpp>
#include <ftxmodel/table_view.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include "tree_source_model.hpp"

using namespace ftxui;
using namespace ftxmodel;

int main() {
  auto screen = ftxui::ScreenInteractive::TerminalOutput();

  auto source_model = std::make_shared<TreeSourceModel>();

  auto flat_proxy = std::make_shared<FlattenTreeProxyModel>();
  flat_proxy->setSourceModel(source_model);

  auto sorted_proxy = std::make_shared<SortFilterProxyModel>();
  sorted_proxy->setSourceModel(flat_proxy);
  sorted_proxy->sort(0);

  std::locale system_locale("");

  sorted_proxy->setSortCallback(
      [&](const ModelIndex& lhs, const ModelIndex& rhs) -> bool {
        const auto lstring = AnyToStringTranslator::Translate(lhs.data());
        const auto rstring = AnyToStringTranslator::Translate(rhs.data());
        return system_locale(lstring, rstring);
      });

  TableView tableView([&]() { screen.PostEvent(ftxui::Event::Custom); });
  tableView.setItemDelegate(std::make_shared<ftxmodel::StyledTextDelegate>());
  tableView.setModel(sorted_proxy);

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
        if (event == ftxui::Event::Return) {
          flat_proxy->expandAll();
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

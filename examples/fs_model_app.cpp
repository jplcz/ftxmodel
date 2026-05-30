#include <ftxmodel/file_system_model.hpp>
#include <ftxmodel/tree_view.hpp>

#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"

using namespace ftxmodel;

int main() {
  auto screen = ftxui::ScreenInteractive::TerminalOutput();

  // Initialize model pointing to the current active directory path "."
  auto fsModel = std::make_shared<FileSystemModel>(".");
  auto fsDelegate = std::make_shared<FileSystemRouterDelegate>();

  // Pass component down into the multi-column separator TreeView engine
  TreeView treeView([&]() { screen.PostEvent(ftxui::Event::Custom); });
  treeView.setItemDelegate(fsDelegate);
  treeView.setModel(fsModel);
  treeView.setShowHeaders(true);

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
    }  // Expands folder
    if (event == ftxui::Event::ArrowLeft) {
      treeView.moveLeft();
      return true;
    }  // Collapses folder
    if (event == ftxui::Event::Escape) {
      screen.Exit();
      return true;
    }
    return false;
  });

  auto appLayout = ftxui::Renderer(appController, [&]() {
    return ftxui::vbox({ftxui::text(" C++ OS FileSystem Tree Explorer Node ") |
                            ftxui::bold | ftxui::center |
                            ftxui::bgcolor(ftxui::Color::GrayDark),
                        treeView.Render()});
  });

  screen.Loop(appLayout);
  return 0;
}

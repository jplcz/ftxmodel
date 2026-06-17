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
  auto treeView = std::make_shared<TreeView>();

  treeView->setItemDelegate(fsDelegate);
  treeView->setModel(fsModel);
  treeView->setShowHorizontalHeaders(true);

  auto appController = ftxui::CatchEvent(treeView, [&](ftxui::Event event) {
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
                        treeView->Render()});
  });

  screen.Loop(appLayout);
  return 0;
}

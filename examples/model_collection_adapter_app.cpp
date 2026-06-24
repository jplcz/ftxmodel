#include <ftxui/component/screen_interactive.hpp>
#include "ftxmodel/model_collection_adapter.hpp"
#include "ftxmodel/polymorphic_vector_table_model.hpp"

using namespace ftxmodel;

int main() {
  auto screen = ftxui::ScreenInteractive::TerminalOutput();

  auto model = std::make_shared<PolymorphicVectorTableModel>();
  model->declareColumn("Database Management Channels");
  model->appendRowItem(std::string("Replica Cluster A"));
  model->appendRowItem(std::string("Staging Database Instance"));
  model->appendRowItem(std::string("Local Memory Volatile Cache"));

  // Instantiate a default component using our factory catalog
  auto tabsControl = std::make_shared<ModelCollectionAdapter>(
      model, Factories::StandardToggle());

  // Instantiate a highly customized menu using an inline factory lambda
  // to pass custom styling rules directly down to FTXUI options
  auto stylizedMenu = std::make_shared<ModelCollectionAdapter>(
      model,
      [](const std::vector<std::string>* strings, int* active) {
        auto customOptions = ftxui::MenuOption::Vertical();
        customOptions.entries_option.transform =
            [](const ftxui::EntryState& state) {
              if (state.focused) {
                return ftxui::text("➔ " + state.label) | ftxui::bold |
                       ftxui::color(ftxui::Color::Yellow);
              }
              return ftxui::text("  " + state.label) | ftxui::dim;
            };
        return ftxui::Menu(strings, active, customOptions);
      },
      [](ftxui::Element element) {
        return element | ftxui::borderRounded |
               ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 35);
      });

  auto container = ftxui::Container::Vertical({tabsControl, stylizedMenu});

  auto appLayout = ftxui::Renderer(container, [&]() {
    return ftxui::vbox(
        {ftxui::text(" Factory-Driven Component Adapter Engine ") |
             ftxui::bold | ftxui::center,
         ftxui::separator(), tabsControl->Render() | ftxui::center,
         ftxui::separator(), stylizedMenu->Render() | ftxui::center});
  });

  screen.Loop(appLayout);
  return 0;
}

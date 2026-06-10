#include <ftxmodel/shortcut_action_model.hpp>
#include <ftxmodel/shortcut_bar.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include "ftxmodel/list_view.hpp"

using namespace ftxmodel;

int main() {
  auto screen = ftxui::ScreenInteractive::TerminalOutput();

  //  Compile baseline commands
  std::vector<ShortcutActionModel::ShortcutItem> base_menu = {
      {.name = "Help",
       .key = "F1",
       .event = ftxui::Event::F1,
       .trigger = []() {}},
      {.name = "Menu",
       .key = "F2",
       .event = ftxui::Event::F2,
       .trigger = []() {}},
      {.name = "View",
       .key = "F3",
       .event = ftxui::Event::F3,
       .trigger = []() {}},
      {.name = "Edit",
       .key = "F4",
       .event = ftxui::Event::F4,
       .trigger = []() {}},
      {.name = "Copy",
       .key = "F5",
       .event = ftxui::Event::F5,
       .trigger = []() {}},
      {.name = "Move",
       .key = "F6",
       .event = ftxui::Event::F6,
       .trigger = []() {}},
      {.name = "Mkddir",
       .key = "F7",
       .event = ftxui::Event::F7,
       .trigger = []() {}},
      {.name = "Delete",
       .key = "F8",
       .event = ftxui::Event::F8,
       .trigger = []() {}},
      {.name = "Config",
       .key = "F9",
       .event = ftxui::Event::F9,
       .trigger = []() {}},
      {.name = "Quit",
       .key = "F10",
       .event = ftxui::Event::F10,
       .trigger = screen.ExitLoopClosure()}};

  auto model = std::make_shared<ShortcutActionModel>(std::move(base_menu));
  auto shortcut_bar = std::make_shared<ShortcutBar>(model);
  auto shortcut_list = std::make_shared<ListView>();
  shortcut_list->setModel(model);
  shortcut_list->setItemDelegate(std::make_shared<StyledTextDelegate>());

  auto simulate_tab_change_button = ftxui::Button("Enter Viewer Mode", [&]() {
    std::vector<ShortcutActionModel::ShortcutItem> viewer_menu = {
        {.name = "Unwrap",
         .key = "F1",
         .event = ftxui::Event::F1,
         .trigger = []() {}},
        {.name = "Hex",
         .key = "F2",
         .event = ftxui::Event::F2,
         .trigger = []() {}},
        {.name = "Goto",
         .key = "F3",
         .event = ftxui::Event::F3,
         .trigger = []() {}},
        {.name = "Search",
         .key = "F4",
         .event = ftxui::Event::F4,
         .trigger = []() {}},
        {.name = "Next",
         .key = "F5",
         .event = ftxui::Event::F5,
         .trigger = []() {}},
        {.name = "Prev",
         .key = "F6",
         .event = ftxui::Event::F6,
         .trigger = []() {}},
        {.name = "Raw",
         .key = "F7",
         .event = ftxui::Event::F7,
         .trigger = []() {}},
        {.name = "Close",
         .key = "F8",
         .event = ftxui::Event::F8,
         .trigger = []() {}},
        {.name = "Quit",
         .key = "F10",
         .event = ftxui::Event::F10,
         .trigger = screen.ExitLoopClosure()}};

    // Update the model on the fly. The ShortcutBar will automatically rebuild
    // itself.
    model->setItems(std::move(viewer_menu));

    shortcut_bar->clearDecorators();
    shortcut_bar->addDecorator(
        [](ftxui::Element el, const ShortcutBar::ShortcutRenderContext& ctx) {
          if (!ctx.is_enabled) {
            return el;  // Leave disabled items grayed out
          }

          // Override the solid block with dynamic bracket frames
          return ftxui::hbox(
              {ftxui::text(ctx.prefix) | ftxui::color(ftxui::Color::Green) |
                   ftxui::bold,
               ftxui::text("[") | ftxui::color(ftxui::Color::GrayLight),
               ftxui::text(ctx.label) | ftxui::color(ftxui::Color::White),
               ftxui::text("]") | ftxui::color(ftxui::Color::GrayLight)});
        });

    shortcut_bar->setBackgroundColor(ftxui::Color::LightCoral);
    shortcut_bar->setGapSpacing(3);
  });

  shortcut_bar->addDecorator(
      [](ftxui::Element el,
         const ShortcutBar::ShortcutRenderContext& ctx) -> ftxui::Element {
        if (ctx.label == "Delete" || ctx.label == "Quit") {
          return ftxui::hbox(
              {ftxui::text(ctx.prefix) | ftxui::color(ftxui::Color::Red) |
                   ftxui::bold,
               ftxui::text(ctx.label) | ftxui::bgcolor(ftxui::Color::DarkRed) |
                   ftxui::color(ftxui::Color::White) | ftxui::bold});
        }
        return el;
      });

  shortcut_bar->setGapSpacing(2);

  shortcut_list->setShowHorizontalHeaders(true);
  shortcut_list->setShowVerticalHeaders(true);

  const auto root_layout =
      ftxui::Container::Vertical({simulate_tab_change_button | ftxui::border,
                                  shortcut_bar, shortcut_list});

  auto global_event_router = ftxui::CatchEvent(
      root_layout,
      [&](ftxui::Event event) { return shortcut_bar->OnEvent(event); });

  screen.Loop(global_event_router);
  return 0;
}

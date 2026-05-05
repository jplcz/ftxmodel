#include <any>
#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <ftxmodel/scrollable_table_view.hpp>
#include <ftxmodel/string_list_model.hpp>

using namespace ftxmodel;

class ThreadSafeQueue {
 private:
  std::queue<std::string> m_q;
  std::mutex m_m;

 public:
  void push(std::string v) {
    std::lock_guard<std::mutex> l(m_m);
    m_q.push(std::move(v));
  }
  bool pop(std::string& o) {
    std::lock_guard<std::mutex> l(m_m);
    if (m_q.empty()) {
      return false;
    }
    o = std::move(m_q.front());
    m_q.pop();
    return true;
  }
};

int main() {
  auto screen = ftxui::ScreenInteractive::TerminalOutput();

  // Initialize data model configurations
  auto list_model =
      std::make_shared<StringListModel>(std::vector<std::string>());
  list_model->append("Initial Baseline Row Entry");

  // Build and setup your ScrollableTableView component
  auto table_view = std::make_shared<ScrollableTableView>();
  table_view->setModel(list_model);
  table_view->setViewportDimensions(10,
                                    1);  // Only 1 column wide for flat lists
  table_view->setItemDelegate(std::make_shared<StyledTextDelegate>());

  std::atomic<bool> thread_running{true};
  ThreadSafeQueue sync_bridge;

  // Kick off worker thread to push elements every 1 second up to 1000
  // seconds
  std::thread generator([&]() {
    int counts = 0;
    while (thread_running && counts < 1000) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      counts++;

      std::string packet =
          "Async System Signal Packet Event [ID: " + std::to_string(counts) +
          "]";
      sync_bridge.push(std::move(packet));

      // Force main FTXUI poll threads to wake up and process the transaction
      screen.PostEvent(ftxui::Event::Custom);
    }
  });

  // Connect rendering loops with memory cache consumption loops
  const auto interface_renderer = ftxui::Renderer(table_view, [&] {
    std::string line;
    while (sync_bridge.pop(line)) {
      list_model->insertAt(0, std::move(line));
    }
    return table_view->Render() |
           ftxui::size(ftxui::HEIGHT, ftxui::GREATER_THAN, 12);
  });

  const auto app_event_handler =
      ftxui::CatchEvent(interface_renderer, [&](ftxui::Event ev) {
        if (ev == ftxui::Event::Custom) {
          return true;
        }
        if (ev == ftxui::Event::Escape) {
          screen.ExitLoopClosure()();
          return true;
        }
        return table_view->OnEvent(ev);
      });

  // Run the TUI
  screen.Loop(app_event_handler);

  // Safe thread termination
  thread_running = false;
  if (generator.joinable()) {
    generator.join();
  }

  return 0;
}

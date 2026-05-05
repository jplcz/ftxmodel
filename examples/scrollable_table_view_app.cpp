#include <sqlite3.h>
#include <any>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <memory>
#include <string>
#include <vector>

// Assumes these header files are placed in your include paths
#include <ftxmodel/abstract_item_model.hpp>
#include <ftxmodel/scrollable_table_view.hpp>
#include <ftxmodel/sqlite_query_model.hpp>
#include <ftxmodel/view_coordinate_mapper.hpp>

using namespace ftxmodel;
using namespace ftxui;

class ExampleDelegate : public ItemDelegate {
 public:
  Element createWidget(const ModelIndex& index,
                       const AbstractItemModel* model) const override {
    const std::string processed_text = model->textData(index);

    // Color coding based on data content (e.g., highlighting "CRITICAL"
    // statuses)
    if (processed_text == "CRITICAL" || processed_text == "ERROR") {
      return text(processed_text) | bold | color(Color::Red);
    } else if (processed_text == "ONLINE" || processed_text == "INFO") {
      return text(processed_text) | color(Color::Green);
    } else if (processed_text == "WARNING") {
      return text(processed_text) | color(Color::Yellow);
    }

    return text(processed_text);
  }

  Dimensions sizeHint(const ModelIndex& index,
                      const AbstractItemModel* model) const override {
    return Dimensions{static_cast<int>(model->textData(index).length()) + 2, 1};
  }
};

sqlite3* setupMockDatabase() {
  sqlite3* db = nullptr;
  sqlite3_open(":memory:", &db);

  sqlite3_exec(db,
               "CREATE TABLE microservices ("
               "  id INTEGER PRIMARY KEY,"
               "  name TEXT,"
               "  status TEXT,"
               "  latency_ms INTEGER,"
               "  owner_team TEXT"
               ");",
               nullptr, nullptr, nullptr);

  // Seed a generous amount of data rows to show off scrolling handles
  sqlite3_exec(db,
               "INSERT INTO microservices VALUES (1, 'auth-gateway', 'ONLINE', "
               "14, 'SecOps');",
               nullptr, nullptr, nullptr);
  sqlite3_exec(db,
               "INSERT INTO microservices VALUES (2, 'payment-processor', "
               "'ONLINE', 85, 'FinanceEng');",
               nullptr, nullptr, nullptr);
  sqlite3_exec(db,
               "INSERT INTO microservices VALUES (3, 'user-profile-db', "
               "'CRITICAL', 3400, 'DataCore');",
               nullptr, nullptr, nullptr);
  sqlite3_exec(db,
               "INSERT INTO microservices VALUES (4, 'notification-broker', "
               "'WARNING', 210, 'Comms');",
               nullptr, nullptr, nullptr);
  sqlite3_exec(db,
               "INSERT INTO microservices VALUES (5, 'search-indexing', "
               "'ONLINE', 45, 'Discovery');",
               nullptr, nullptr, nullptr);
  sqlite3_exec(db,
               "INSERT INTO microservices VALUES (6, 'recommendation-api', "
               "'ONLINE', 110, 'MLOps');",
               nullptr, nullptr, nullptr);
  sqlite3_exec(db,
               "INSERT INTO microservices VALUES (7, 'logging-aggregator', "
               "'INFO', 12, 'Infra');",
               nullptr, nullptr, nullptr);
  sqlite3_exec(db,
               "INSERT INTO microservices VALUES (8, 'cdn-edge-cache', "
               "'ONLINE', 8, 'Infra');",
               nullptr, nullptr, nullptr);
  sqlite3_exec(db,
               "INSERT INTO microservices VALUES (9, 'reporting-bi', 'ERROR', "
               "950, 'Analytics');",
               nullptr, nullptr, nullptr);

  return db;
}

int main() {
  sqlite3* db = setupMockDatabase();
  auto screen = ScreenInteractive::TerminalOutput();

  // 1. Initialize data backing models and query sequences
  auto sql_model = std::make_shared<SqliteQueryModel>(db);
  sql_model->setQuery(
      "SELECT id, name, status, latency_ms, owner_team FROM microservices;");
  sql_model->setIdentityMode(SqliteQueryModel::IdentityMode::IndividualCell);

  // 2. Instantiate view engine and assign delegate configurations
  auto item_delegate = std::make_shared<ExampleDelegate>();
  auto header_delegate = std::make_shared<AdvancedHeaderDelegate>();

  // Use a shared pointer wrapper for the view so it plays nicely with the
  // component hierarchy
  auto table_view = std::make_shared<ScrollableTableView>();
  table_view->setItemDelegate(item_delegate);
  table_view->setHorizontalHeaderDelegate(header_delegate);
  table_view->setModel(sql_model);

  // Set a tight viewport window boundary (e.g., 5 rows high, 4 columns wide)
  // to force your scrolling logic and mouse tracking to work within fixed
  // bounds.
  table_view->setViewportDimensions(5, 4);
  table_view->setShowHorizontalHeaders(true);
  table_view->highlightStyle()
      ->setSelectionBehavior(SelectionBehavior::SelectRows)
      .setActiveFocusStyle(ftxui::underlined | color(Color::Cyan) | bold);

  auto dashboard_renderer = Renderer(table_view, [&] {
    return vbox(
        {text(" Cluster Telemetry Control Panel ") | bold | hcenter,
         separator(),
         hbox({text(" Viewport Config: ") | dim,
               text("5 Visible Rows x 4 Visible Columns (Scrolls "
                    "automatically)") |
                   color(Color::Cyan)}),
         separator(),

         // CRITICAL CHANGE: Use child->Render() instead of table_view->Render()
         // This tells FTXUI to place the container's active child right here.
         table_view->Render() | flex,

         separator(),
         vbox({text(" Navigation Guide: ") | bold,
               text(" • Use [Arrow Keys] or [PageUp / PageDown] to move the "
                    "active cursor highlights."),
               text(" • Use [Mouse Click] to pick single cells, or [Mouse "
                    "Scroll Wheel] to scroll the list."),
               text(" • Press [ESC] to terminate the monitoring shell "
                    "connection.")}) |
             dim});
  });

  auto main_layout = Container::Vertical({dashboard_renderer});

  // Catch the global escape character hook to break runtime loops cleanly
  auto window_event_handler = CatchEvent(main_layout, [&](Event event) {
    if (event == Event::Escape) {
      screen.ExitLoopClosure()();
      return true;
    }
    return false;
  });

  screen.Loop(window_event_handler);

  sqlite3_close(db);
  return 0;
}

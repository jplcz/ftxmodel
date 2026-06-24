#include <ftxmodel/table_view.hpp>
#include <memory>
#include <string>

#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"

#include <ftxmodel/vector_table_model.hpp>

using namespace ftxmodel;

struct TaskItem {
  std::string name;
  bool done;
  int progress;
};

class TableColumnRouterDelegate : public ItemDelegate {
 private:
  StyledTextDelegate text_del_{
      Alignment::Left, ftxui::Color::White,
      FormattingOptions{.max_width = 32, .min_width = 32}};
  CheckBoxDelegate check_del_;
  ProgressBarDelegate progress_del_{100.0f};

 public:
  ftxui::Element createWidget(const ModelIndex& index,
                              const AbstractItemModel* model) const override {
    switch (index.column()) {
      case 0:
        return text_del_.createWidget(index, model);
      case 1:
        return check_del_.createWidget(index, model);
      case 2:
        return progress_del_.createWidget(index, model);
      default:
        return ftxui::text("");
    }
  }

  ftxui::Dimensions sizeHint(const ModelIndex& index,
                             const AbstractItemModel* model) const override {
    switch (index.column()) {
      case 0:
        return text_del_.sizeHint(index, model);
      case 1:
        return check_del_.sizeHint(index, model);
      case 2:
        return progress_del_.sizeHint(index, model);
      default:
        return ftxui::Dimensions{0, 1};
    }
  }
};

int main() {
  auto screen = ftxui::ScreenInteractive::TerminalOutput();

  auto model = std::make_shared<VectorTableModel<TaskItem>>();

  // 4. Configure your column behaviors using modular lambdas
  model->addColumn("Task Target",
                   [](const TaskItem& item, ItemRole role) -> std::any {
                     if (role == ItemRole::DisplayRole) {
                       return item.name;
                     }
                     return {};
                   });

  model->addColumn("Status",
                   [](const TaskItem& item, ItemRole role) -> std::any {
                     if (role == ItemRole::DisplayRole) {
                       return item.done;
                     }
                     return {};
                   });

  model->addColumn("Progress Gauge",
                   [](const TaskItem& item, ItemRole role) -> std::any {
                     if (role == ItemRole::DisplayRole) {
                       return item.progress;
                     }
                     return {};
                   });

  // Optional stable unique ID rule configuration for tracking lookups
  model->setKeyExtractor(
      [](const TaskItem& item) -> UniqueNodeId { return item.name; });

  std::vector<TaskItem> tasks = {{"Fetch Dependencies", true, 100},
                                 {"Compile Source Binary", false, 45},
                                 {"Run Headless Tests", false, 0}};
  model->setVectorData(std::move(tasks));

  auto delegate = std::make_shared<TableColumnRouterDelegate>();

  auto tableView = std::make_shared<TableView>();
  tableView->setItemDelegate(delegate);
  tableView->setModel(model);
  tableView->highlightStyle()->setSelectionBehavior(
      SelectionBehavior::SelectRows);

  auto appController = ftxui::CatchEvent(tableView, [&](ftxui::Event event) {
    if (event == ftxui::Event::Escape) {
      screen.Exit();
      return true;
    }
    return false;
  });

  auto appLayout = ftxui::Renderer(appController, [&]() {
    return ftxui::vbox(
        {ftxui::text(" Decoupled Model-View-Delegate Table View ") |
             ftxui::bold | ftxui::center,
         tableView->Render()});
  });

  screen.Loop(appLayout);
  return 0;
}

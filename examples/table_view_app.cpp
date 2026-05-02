#include <ftxmodel/table_view.hpp>

#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"

using namespace ftxmodel;

// 1. Concrete Multi-Column Data Matrix Backend
class TaskTableModel : public AbstractItemModel {
 private:
  struct TaskItem {
    std::string name;
    bool done;
    int progress;
  };
  std::vector<TaskItem> tasks_ = {{"Fetch Dependencies", true, 100},
                                  {"Compile Source Binary", false, 45},
                                  {"Run Headless Tests", false, 0}};

 public:
  ModelIndex index(int r, int c, const ModelIndex& p) const override {
    if (p.isValid() || r < 0 || r >= (int)tasks_.size() || c < 0 || c >= 3) {
      return {};
    }
    return createIndex(r, c, (void*)&tasks_[(size_t)r]);
  }
  ModelIndex parent(const ModelIndex&) const override { return {}; }
  int rowCount(const ModelIndex& p) const override {
    return p.isValid() ? 0 : (int)tasks_.size();
  }
  int columnCount(const ModelIndex&) const override { return 3; }

  std::any data(const ModelIndex& index, ItemRole role) const override {
    if (!index.isValid()) {
      return {};
    }
    const auto& item = tasks_[(size_t)index.row()];

    if (index.column() == 0 && role == ItemRole::DisplayRole) {
      return item.name;
    }
    if (index.column() == 1 && role == ItemRole::DisplayRole) {
      return item.done;
    }
    if (index.column() == 2 && role == ItemRole::DisplayRole) {
      return item.progress;
    }
    return {};
  }

  std::any headerData(int section,
                      Orientation orient,
                      ItemRole role) const override {
    if (orient == Orientation::Horizontal && role == ItemRole::DisplayRole) {
      if (section == 0) {
        return std::string("Task Target");
      }
      if (section == 1) {
        return std::string("Status");
      }
      if (section == 2) {
        return std::string("Progress Gauge");
      }
    }
    return AbstractItemModel::headerData(section, orient, role);
  }
};

// 2. Multi-Column Router Delegate Mapping Existing Delegate Types
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
        // Routes the 32-character strict fixed-width bounds hint forward
        return text_del_.sizeHint(index, model);
      case 1:
        // Routes the static 3-character width footprint hint forward (" [X] ")
        return check_del_.sizeHint(index, model);
      case 2:
        // Routes the internal aggregate progress width footprint hint forward
        // (17 characters)
        return progress_del_.sizeHint(index, model);
      default:
        // Fallback boundary constraint track
        return ftxui::Dimensions{0, 1};
    }
  }
};

int main() {
  auto screen = ftxui::ScreenInteractive::TerminalOutput();

  auto model = std::make_shared<TaskTableModel>();
  auto delegate = std::make_shared<TableColumnRouterDelegate>();

  TableView tableView([&]() { screen.PostEvent(ftxui::Event::Custom); });
  tableView.setItemDelegate(delegate);
  tableView.setModel(model);

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
        return false;
      });

  auto appLayout = ftxui::Renderer(appController, [&]() {
    return ftxui::vbox(
        {ftxui::text(" Decoupled Model-View-Delegate Table View ") |
             ftxui::bold | ftxui::center,
         tableView.Render()});
  });

  screen.Loop(appLayout);
  return 0;
}

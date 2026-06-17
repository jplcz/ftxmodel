#include <format>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <memory>
#include <string>
#include <vector>

#include <ftxmodel/concat_proxy_model.hpp>
#include <ftxmodel/table_view.hpp>

using namespace ftxmodel;
using namespace ftxui;

// =========================================================================
// A Simple Concrete Data Model for the Demo
// =========================================================================
class StringGridModel : public AbstractItemModel {
 private:
  int m_rows;
  int m_cols;
  std::string m_dataset_name;

 public:
  StringGridModel(int rows, int cols, std::string name)
      : m_rows(rows), m_cols(cols), m_dataset_name(std::move(name)) {}

  int rowCount(const ModelIndex& parent = ModelIndex()) const override {
    return parent.isValid() ? 0 : m_rows;
  }

  int columnCount(const ModelIndex& parent = ModelIndex()) const override {
    return parent.isValid() ? 0 : m_cols;
  }

  ModelIndex index(const int row,
                   const int column,
                   const ModelIndex& parent = ModelIndex()) const override {
    if (parent.isValid() || row < 0 || row >= m_rows || column < 0 ||
        column >= m_cols) {
      return {};
    }
    // Give each index a dummy unique layout identifier pointer
    auto unique_id = static_cast<uintptr_t>(row * 100 + column + 1);
    return createIndex(row, column, reinterpret_cast<void*>(unique_id));
  }

  ModelIndex parent(const ModelIndex&) const override { return {}; }

  std::any data(const ModelIndex& index,
                ItemRole role = ItemRole::DisplayRole) const override {
    if (!index.isValid() || role != ItemRole::DisplayRole) {
      return std::any();
    }
    // Return a readable coordinate string tagged with its origin dataset
    return std::format("[{}] R:{} C:{}", m_dataset_name, index.row(),
                       index.column());
  }

  UniqueNodeId uniqueId(const ModelIndex& index) const override {
    if (!index.isValid()) {
      return {};
    }
    return {std::format("{}_node_{}_{}", m_dataset_name, index.row(),
                        index.column())};
  }

  std::any headerData(int section,
                      Orientation orientation,
                      ItemRole role = ItemRole::DisplayRole) const override {
    if (role != ItemRole::DisplayRole) {
      return {};
    }
    if (orientation == Orientation::Horizontal) {
      return std::format("H:{}:{}", m_dataset_name, section);
    } else {
      return std::format("V:{}:{}", m_dataset_name, section);
    }
  }
};

// =========================================================================
// Application Main Entry Loop
// =========================================================================
int main() {
  // Initialize three standalone underlying models with varying sizes
  auto model_alpha =
      std::make_shared<StringGridModel>(3, 2, "Alpha");  // 3 Rows, 2 Cols
  auto model_beta =
      std::make_shared<StringGridModel>(2, 2, "Beta");  // 2 Rows, 2 Cols
  auto model_gamma =
      std::make_shared<StringGridModel>(4, 3, "Gamma");  // 4 Rows, 3 Cols

  auto concat_proxy = std::make_shared<ConcatProxyModel>();

  // Start with a standard Vertical layout configuration
  concat_proxy->setSourceModels({model_alpha, model_beta, model_gamma},
                                Orientation::Vertical);

  auto screen = ScreenInteractive::TerminalOutput();
  auto table_view = std::make_shared<TableView>();
  table_view->setModel(concat_proxy);
  table_view->setItemDelegate(std::make_shared<StyledTextDelegate>());

  int selected_layout_mode = 0;
  std::vector<std::string> options = {" Vertical Stitching (Stack Rows) ",
                                      " Horizontal Stitching (Merge Columns) "};

  auto toggle_button = Toggle(&options, &selected_layout_mode);

  // Catch selection changes and re-configure our proxy layout boundaries on the
  // fly
  auto interactive_layout = Container::Vertical(
      {toggle_button, Renderer(table_view, [&] {
         if (selected_layout_mode == 0) {
           concat_proxy->setSourceModels({model_alpha, model_beta, model_gamma},
                                         Orientation::Vertical);
         } else {
           concat_proxy->setSourceModels({model_alpha, model_beta, model_gamma},
                                         Orientation::Horizontal);
         }

         return vbox({text(" FTXUI Concatenated Model Architecture Interface "
                           "Dashboard ") |
                          bold | hcenter,
                      separator(), hbox({table_view->Render() | xflex_shrink}),
                      separator(),
                      text(" Use [Left/Right Arrow Keys] to toggle proxy "
                           "orientations. Press [ESC] or [Ctrl+C] to Exit.") |
                          dim});
       })});

  screen.Loop(interactive_layout);
  return 0;
}


#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <ftxmodel/scrollable_table_view.hpp>
#include <ftxmodel/string_list_model.hpp>

using namespace ftxmodel;

class FetchMoreListModel : public AbstractListModel {
 private:
  std::vector<std::string> m_cached_items;
  const size_t m_max_database_records = 100;
  const size_t m_chunk_size = 5;

 public:
  FetchMoreListModel() {
    // Load the initial batch of elements on startup
    FetchMoreListModel::fetchMore(ModelIndex());
  }

  int rowCount(const ModelIndex& parent = ModelIndex()) const override {
    if (parent.isValid()) {
      return 0;
    }
    return static_cast<int>(m_cached_items.size());
  }

  std::any data(const ModelIndex& index, ItemRole role) const override {
    if (!index.isValid() || index.row() >= rowCount()) {
      return {};
    }
    if (role == ItemRole::DisplayRole) {
      return m_cached_items[static_cast<size_t>(index.row())];
    }
    return {};
  }

  bool canFetchMore(const ModelIndex& parent) const override {
    if (parent.isValid()) {
      return false;  // List structures fetch only at root level
    }
    return m_cached_items.size() < m_max_database_records;
  }

  void fetchMore(const ModelIndex& parent) override {
    if (!canFetchMore(parent)) {
      return;
    }

    int firstNewRow = rowCount();
    size_t itemsToFetch =
        std::min(m_chunk_size, m_max_database_records - m_cached_items.size());
    int lastNewRow = firstNewRow + static_cast<int>(itemsToFetch) - 1;

    // Notify listeners that new memory slots are opening up
    beginInsertRows(parent, firstNewRow, lastNewRow);

    // Synthesize/fetch the new continuous slice
    for (size_t i = 0; i < itemsToFetch; ++i) {
      m_cached_items.push_back("Database Record Row Entry #" +
                               std::to_string(firstNewRow + (int)i + 1));
    }

    // Close transaction and refresh view layout
    endInsertRows();
  }
};

int main() {
  auto screen = ftxui::ScreenInteractive::TerminalOutput();

  // Instantiate your new fetchable data list source
  auto list_model = std::make_shared<FetchMoreListModel>();

  // Build and setup your ScrollableTableView component
  auto table_view = std::make_shared<ScrollableTableView>();
  table_view->setModel(list_model);
  table_view->setViewportDimensions(10,
                                    1);  // Only 1 column wide for flat lists
  table_view->setItemDelegate(std::make_shared<StyledTextDelegate>());

  // Wrap with layout metadata decoration frames
  auto layout_container = ftxui::Renderer(table_view, [&] {
    return ftxui::vbox(
               {ftxui::text(" TERMINAL FETCH MRORE SCROLLER ") | ftxui::bold |
                    ftxui::color(ftxui::Color::Cyan),
                ftxui::text(" Scroll or hold [ArrowDown] to dynamically fetch "
                            "records from database...") |
                    ftxui::dim,
                ftxui::separator(), table_view->Render(), ftxui::separator(),
                ftxui::hbox(
                    {ftxui::text(" Current Loaded Memory Frame Count: "),
                     ftxui::text(std::to_string(list_model->rowCount()) +
                                 "/100 items") |
                         ftxui::color(ftxui::Color::Green)})}) |
           ftxui::borderDouble;
  });

  screen.Loop(layout_container);
  return 0;
}

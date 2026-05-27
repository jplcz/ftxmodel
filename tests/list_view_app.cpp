#include <ftxmodel/list_view.hpp>

#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"

using namespace ftxmodel;

struct PackageItem {
  std::string name;
  bool status;
};

class PackageListModel : public AbstractListModel {
 private:
  std::vector<PackageItem> packages_;

 public:
  explicit PackageListModel(std::vector<PackageItem> items)
      : packages_(std::move(items)) {}

  int rowCount(const ModelIndex& parent = ModelIndex()) const override {
    return parent.isValid() ? 0 : static_cast<int>(packages_.size());
  }

  std::any data(const ModelIndex& index, ItemRole role) const override {
    if (!index.isValid() || index.row() >= rowCount()) {
      return {};
    }
    if (role == ItemRole::DisplayRole) {
      return packages_[(size_t)index.row()].name;
    }
    if (role == ItemRole::CheckedRole) {
      return packages_[(size_t)index.row()].status;
    }
    return {};
  }

  bool setData(const ModelIndex& index,
               const std::any& value,
               ItemRole role) override {
    if (!index.isValid() || index.row() >= rowCount()) {
      return false;
    }

    if (role == ItemRole::CheckedRole && value.type() == typeid(bool)) {
      packages_[(size_t)index.row()].status = std::any_cast<bool>(value);
      dataChanged(index, index);  // Alert view via sigslot
      return true;
    }
    return false;
  }

  void addPackage(const std::string& name) {
    int nextRow = rowCount();
    beginInsertRows(ModelIndex(), nextRow, nextRow);
    packages_.push_back({name, false});
    endInsertRows();
  }

 protected:
  void* internalPointerAt(int row) const override {
    return (void*)&packages_.at((size_t)row);
  }
};

class PackageItemDelegate : public ItemDelegate {
 public:
  ftxui::Element createWidget(const ModelIndex& index,
                              const AbstractItemModel* model) const override {
    std::string name =
        std::any_cast<std::string>(model->data(index, ItemRole::DisplayRole));
    bool isChecked =
        std::any_cast<bool>(model->data(index, ItemRole::CheckedRole));

    auto checkboxIcon = isChecked ? ftxui::text("[X]") |
                                        ftxui::color(ftxui::Color::Green) |
                                        ftxui::bold
                                  : ftxui::text("[ ]") | ftxui::dim;

    return ftxui::hbox({checkboxIcon, ftxui::text(" " + name)});
  }
};

int main() {
  using namespace ftxui;

  auto screen = ScreenInteractive::TerminalOutput();

  // Instantiating Initial Backends
  std::vector<PackageItem> dataItems = {{"gcc-toolchain", true},
                                        {"cmake-build-system", true},
                                        {"ftxui-ux-lib", false},
                                        {"sigslot", false}};
  auto model = std::make_unique<PackageListModel>(dataItems);
  auto delegate = std::make_shared<PackageItemDelegate>();

  // Setup view with a screen refresh closure mapping sigslot back into FTXUI
  auto refreshUiLambda = [&]() { screen.PostEvent(Event::Custom); };
  ListView listView(refreshUiLambda);
  listView.setModel(model.get());
  listView.setItemDelegate(delegate);

  // Create interactive key handling component intercept
  auto baseComponent = Make<ComponentBase>();
  int dynamicCount = 1;

  auto appController = CatchEvent(baseComponent, [&](Event event) {
    if (event == Event::ArrowUp) {
      listView.moveUp();
      return true;
    }
    if (event == Event::ArrowDown) {
      listView.moveDown();
      return true;
    }
    if (event == Event::Character(' ')) {  // Spacebar Toggles State
      listView.toggleCurrentItem();
      return true;
    }
    if (event == Event::Character('a') ||
        event == Event::Character('A')) {  // Append item safely
      model->addPackage("custom-extra-package-" +
                        std::to_string(dynamicCount++));
      return true;
    }
    if (event == Event::Escape) {
      screen.Exit();
      return true;
    }
    return false;
  });

  // Map application layout view drawing blocks
  auto uiRenderer = Renderer(appController, [&]() {
    return vbox(
        {text(" FTXUI AbstractListModel Environment Framework Sync Node ") |
             bold | bgcolor(Color::GrayDark) | color(Color::White),
         separator(), listView.render(), separator(),
         vbox({text(" Controls:") | bold,
               text("  • [Up/Down Arrows] : Navigate inside layout tree index "
                    "rows"),
               text("  • [Spacebar]       : Trigger model's setData() checkbox "
                    "switch value state"),
               text("  • [A]              : Dynamically invoke appendRow "
                    "mechanisms down inside model storage tracks"),
               text("  • [Esc]            : Terminate running application "
                    "gracefully")}) |
             dim});
  });

  screen.Loop(uiRenderer);
  return 0;
}

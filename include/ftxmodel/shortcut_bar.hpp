#pragma once
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/dom/elements.hpp>

namespace ftxmodel {

class ShortcutBar : public ftxui::ComponentBase {
 public:
  struct ShortcutRenderContext {
    int row;
    std::string prefix;
    std::string label;
    bool is_enabled;
  };

  // Pipeline slot signature to customize individual shortcut blocks
  using ShortcutDecorator =
      std::function<ftxui::Element(ftxui::Element,
                                   const ShortcutRenderContext&)>;

 private:
  ftxui::Component m_layout;
  std::shared_ptr<AbstractItemModel> m_model;
  std::vector<sigslot::scoped_connection> m_connections;
  std::vector<ShortcutDecorator> m_decorators;
  int m_gap_spacing = 1;  // Default character spacing channels
  ftxui::Color m_background_color = ftxui::Color::Black;

 public:
  explicit ShortcutBar(const std::shared_ptr<AbstractItemModel>& model) {
    m_layout = ftxui::Container::Horizontal({});
    Add(m_layout);
    setModel(model);
  }

  void addDecorator(ShortcutDecorator decorator) {
    m_decorators.push_back(std::move(decorator));
  }

  void setGapSpacing(int spaces) noexcept {
    m_gap_spacing = std::max(0, spaces);
  }

  void clearDecorators() noexcept { m_decorators.clear(); }

  void setModel(const std::shared_ptr<AbstractItemModel>& model) {
    if (model.get() == m_model.get()) {
      return;
    }

    m_connections.clear();
    m_model = model;

    m_connections.emplace_back(m_model->beginResetModel.connect(
        &ShortcutBar::onModelResetBegin, this));
    m_connections.emplace_back(
        m_model->endResetModel.connect(&ShortcutBar::onModelResetEnd, this));
    m_connections.emplace_back(m_model->beginInsertRows.connect(
        &ShortcutBar::onBeginInsertRows, this));
    m_connections.emplace_back(
        m_model->endInsertRows.connect(&ShortcutBar::onEndInsertRows, this));
    m_connections.emplace_back(m_model->beginRemoveRows.connect(
        &ShortcutBar::onBeginRemoveRows, this));
    m_connections.emplace_back(
        m_model->endRemoveRows.connect(&ShortcutBar::onEndRemoveRows, this));
    m_connections.emplace_back(
        m_model->dataChanged.connect(&ShortcutBar::onDataChanged, this));

    rebuildModel();
  }

  ftxui::Element OnRender() override {
    ftxui::Elements elements;
    const int rows = m_model->rowCount();
    for (int i = 0; i < rows; ++i) {
      const auto idx = m_model->index(i, 0);
      if (!idx.isValid()) {
        continue;
      }
      if (!elements.empty() && m_gap_spacing > 0) {
        elements.push_back(
            ftxui::text(std::string(static_cast<size_t>(m_gap_spacing), ' ')));
      }

      ShortcutRenderContext ctx{
          .row = i,
          .prefix = AnyToStringTranslator::Translate(
              idx.data(ItemRole::ShortcutTextRole), " "),
          .label = m_model->textData(idx),
          .is_enabled = (idx.flags() & ItemFlag::ItemIsEnabled) != 0};

      ftxui::Decorator state_style = ftxui::bgcolor(ftxui::Color::Cyan) |
                                     ftxui::color(ftxui::Color::Black);
      if (!ctx.is_enabled) {
        state_style = ftxui::bgcolor(ftxui::Color::GrayDark) |
                      ftxui::color(ftxui::Color::GrayLight) | ftxui::dim;
      }

      ftxui::Element rendered_item =
          ftxui::hbox({ftxui::text(ctx.prefix) |
                           ftxui::color(ftxui::Color::White) | ftxui::bold,
                       ftxui::text(ctx.label) | state_style});

      for (const auto& decorate : m_decorators) {
        rendered_item = decorate(std::move(rendered_item), ctx);
      }

      elements.emplace_back(rendered_item);
    }
    return ftxui::hbox(std::move(elements)) |
           ftxui::bgcolor(m_background_color);
  }

  bool OnEvent(const ftxui::Event event) override {
    const int rows = m_model->rowCount();
    for (int i = 0; i < rows; ++i) {
      const auto idx = m_model->index(i, 0);
      if (!idx.isValid()) {
        continue;
      }
      if (!(idx.flags() & ItemFlag::ItemIsEnabled)) {
        continue;
      }
      const auto shortcut = idx.data(ItemRole::ShortcutRole);
      if (shortcut.type() != typeid(ftxui::Event)) {
        continue;
      }
      if (event == std::any_cast<const ftxui::Event&>(shortcut)) {
        return m_model->setData(idx, true);
      }
    }
    return false;
  }

  [[nodiscard]] ftxui::Color backgroundColor() const noexcept {
    return m_background_color;
  }

  void setBackgroundColor(ftxui::Color bg_color) noexcept {
    m_background_color = bg_color;
  }

 private:
  void rebuildModel() {
    m_layout->DetachAllChildren();
    const int totalActions = m_model->rowCount();
    for (int i = 0; i < totalActions; ++i) {
      auto item = ftxui::Make<ComponentBase>();
      item =
          ftxui::CatchEvent(item, [this, row = i](ftxui::Event event) -> bool {
            if (event.is_mouse() &&
                event.mouse().button == ftxui::Mouse::Left &&
                event.mouse().motion == ftxui::Mouse::Pressed) {
              const auto idx = m_model->index(row, 0);
              if (idx.isValid() &&
                  (m_model->flags(idx) & ItemFlag::ItemIsEnabled)) {
                m_model->setData(idx, true);
                return true;
              }
            }
            return false;
          });
      m_layout->Add(item);
    }
  }

  void onModelResetBegin() {}

  void onModelResetEnd() { rebuildModel(); }

  void onBeginInsertRows(const ModelIndex&, int, int) {}

  void onEndInsertRows() { rebuildModel(); }

  void onBeginRemoveRows(const ModelIndex&, int, int) {}

  void onEndRemoveRows() { rebuildModel(); }

  void onDataChanged(const ModelIndex&, const ModelIndex&) { rebuildModel(); }
};

}  // namespace ftxmodel

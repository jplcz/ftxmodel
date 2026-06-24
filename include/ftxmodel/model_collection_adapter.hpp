#pragma once
#include <algorithm>
#include <any>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "abstract_item_model.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"

namespace ftxmodel {

class ModelCollectionAdapter : public ftxui::ComponentBase {
 public:
  // The factory contract signature: accepts the data sources and returns a
  // generic component handle
  using ComponentFactory =
      std::function<ftxui::Component(const std::vector<std::string>*, int*)>;
  using ElementDecorator = std::function<ftxui::Element(ftxui::Element)>;

  /**
   * @brief Creates a dynamically synchronized collection control wrapper.
   * @param model Shared dataset pipeline source tracker.
   * @param factory Callable factory blueprint instructing how to instantiate
   * the target FTXUI component.
   * @param decorator Optional layout modifier function to apply post-render
   * styling adjustments.
   * @param targetColumn The horizontal data track column to harvest strings
   * from (defaults to 0).
   */
  ModelCollectionAdapter(std::shared_ptr<AbstractItemModel> model,
                         const ComponentFactory& factory,
                         ElementDecorator decorator = nullptr,
                         const int targetColumn = 0)
      : m_model(std::move(model)),
        m_decorator(std::move(decorator)),
        m_column(targetColumn) {
    if (!m_model || !factory) {
      return;
    }

    // Populate initial vector data structures
    syncFromModel();

    // Let the factory bind the component straight to our live, internal vectors
    m_coreComponent = factory(&m_displayStrings, &m_activeIndex);

    // Add the component into the native FTXUI focus/lifecycle hierarchy
    Add(m_coreComponent);

    // Bind data signaling events from our abstract model boundary straight to
    // lazy refresh steps
    auto triggerUpdate = [this](auto...) { refreshControlState(); };
    m_model->dataChanged.connect(triggerUpdate);
    m_model->endResetModel.connect(triggerUpdate);
    m_model->endInsertRows.connect(triggerUpdate);
    m_model->endRemoveRows.connect(triggerUpdate);
  }

  ~ModelCollectionAdapter() override = default;

  ModelIndex activeIndexContext() const noexcept {
    if (!m_model || m_activeIndex < 0 || m_activeIndex >= m_model->rowCount()) {
      return {};
    }
    return m_model->index(m_activeIndex, m_column);
  }

  int activeRowIndex() const noexcept { return m_activeIndex; }

  void setActiveRowIndex(int index) noexcept {
    if (!m_displayStrings.empty()) {
      m_activeIndex =
          std::clamp(index, 0, static_cast<int>(m_displayStrings.size()) - 1);
    }
  }

  ftxui::Element OnRender() override {
    if (m_displayStrings.empty()) {
      return ftxui::text("(No Options Available)") | ftxui::dim;
    }

    ftxui::Element renderedOutput = m_coreComponent->Render();
    if (m_decorator) {
      return m_decorator(std::move(renderedOutput));
    }
    return renderedOutput;
  }

 private:
  void syncFromModel() {
    m_displayStrings.clear();
    if (!m_model) {
      return;
    }

    const int totalRows = m_model->rowCount();
    m_displayStrings.reserve(static_cast<size_t>(totalRows));

    for (int r = 0; r < totalRows; ++r) {
      ModelIndex idx = m_model->index(r, m_column);
      m_displayStrings.emplace_back(
          m_model->textData(idx, ItemRole::DisplayRole));
    }

    if (!m_displayStrings.empty()) {
      m_activeIndex = std::clamp(m_activeIndex, 0,
                                 static_cast<int>(m_displayStrings.size()) - 1);
    } else {
      m_activeIndex = 0;
    }
  }

  void refreshControlState() { syncFromModel(); }

  std::shared_ptr<AbstractItemModel> m_model;
  ElementDecorator m_decorator;
  int m_column = 0;

  int m_activeIndex = 0;
  std::vector<std::string> m_displayStrings;
  ftxui::Component m_coreComponent;
};

namespace Factories {

inline ModelCollectionAdapter::ComponentFactory StandardMenu(
    ftxui::MenuOption option = ftxui::MenuOption::Vertical()) {
  return [option](const std::vector<std::string>* strings, int* active) {
    return ftxui::Menu(strings, active, option);
  };
}

inline ModelCollectionAdapter::ComponentFactory StandardRadiobox(
    ftxui::RadioboxOption option = ftxui::RadioboxOption()) {
  return [option](const std::vector<std::string>* strings, int* active) {
    return ftxui::Radiobox(strings, active, option);
  };
}

inline ModelCollectionAdapter::ComponentFactory StandardToggle() {
  return [](const std::vector<std::string>* strings, int* active) {
    return ftxui::Toggle(strings, active);
  };
}

}  // namespace Factories

}  // namespace ftxmodel

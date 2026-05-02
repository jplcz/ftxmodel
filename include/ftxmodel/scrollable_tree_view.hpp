#pragma once
#include <ftxmodel/flatten_tree_proxy_model.hpp>
#include <ftxmodel/scrollable_tree_view.hpp>

#include "scrollable_table_view.hpp"

namespace ftxmodel {

class ScrollableTreeView : public ftxui::ComponentBase {
  std::shared_ptr<FlattenTreeProxyModel> m_proxy =
      std::make_shared<FlattenTreeProxyModel>();
  std::shared_ptr<ScrollableTableView> m_table_view =
      std::make_shared<ScrollableTableView>();

 public:
  ScrollableTreeView() {
    Add(m_table_view);
    m_table_view->setModel(m_proxy);
    m_table_view->setItemDelegate(std::make_shared<StyledTextDelegate>());
    m_table_view->highlightStyle()->setSelectionBehavior(
        SelectionBehavior::SelectRows);
    m_table_view->setShowHorizontalHeaders(true);
  }

  ~ScrollableTreeView() override = default;

  void setModel(const std::shared_ptr<AbstractItemModel>& model) {
    m_proxy->setSourceModel(model);
    m_proxy->expandAll();
  }

  bool OnEvent(ftxui::Event event) override {
    return m_table_view->OnEvent(std::move(event));
  }
};

}  // namespace ftxmodel

#include <ftxmodel/abstract_proxy_model.hpp>
#include <ftxmodel/item_delegate.hpp>

namespace ftxmodel {

/**
 * @class ProxyItemDelegate
 * @brief A decorator delegate that automatically maps coordinate spaces between
 * views and source delegates.
 *
 * This class acts as an intermediary component inside a proxy-model
 * architecture. It intercepts view-layer requests using visual %Proxy
 * coordinates, handles the index translation down to %Source coordinate spaces
 * via the proxy model, and forwards the rendering and dimension calculations to
 * an underlying source-relative delegate.
 *
 * ### Architectural Layout
 * @code
 * [ View Container ]  ───► Uses Proxy Coordinates (Visual Matrix Index)
 * │
 * ▼
 * [ ProxyItemDelegate ]
 * │
 * ├──► dynamic_cast<const SortFilterProxyModel*>(model)
 * ├──► proxy_model->mapToSource(proxyIndex) ──► [SourceIndex]
 * │
 * ▼
 * [ Custom Source Delegate ] ───► Receives Source Index & Source Model directly
 * @endcode
 */
class ProxyItemDelegate : public ItemDelegate {
 private:
  std::shared_ptr<ItemDelegate> m_source_delegate;

 public:
  /**
   * @brief Constructs a ProxyItemDelegate wrapping a specific source delegate.
   * @param sourceDelegate The concrete delegate that handles formatting for raw
   * source data.
   */
  explicit ProxyItemDelegate(std::shared_ptr<ItemDelegate> sourceDelegate)
      : m_source_delegate(std::move(sourceDelegate)) {}

  /**
   * @brief Default virtual destructor.
   */
  ~ProxyItemDelegate() override = default;

  /**
   * @brief Intercepts the view's layout call, maps coordinates, and routes to
   * the source delegate.
   * If the passed model resolves to a `AbstractProxyModel`, the index is
   * translated from visual coordinates to source data coordinates. If the model
   * is not a proxy, it falls back to a transparent passthrough invocation.
   *
   * @param proxyIndex The contextual visual index provided by the view layout
   * loop.
   * @param model Pointer to the active operational model interface instance.
   * @return ftxui::Element The generated visual widget interface node ready for
   * screen rendering.
   */
  ftxui::Element createWidget(const ModelIndex& proxyIndex,
                              const AbstractItemModel* model) const override {
    if (!m_source_delegate) {
      return ftxui::text("");
    }

    // Attempt to check if the model is a AbstractItemModel
    if (const auto* proxy_model =
            dynamic_cast<const AbstractProxyModel*>(model)) {
      // Translate the index down to source-space coordinates
      const ModelIndex sourceIndex = proxy_model->mapToSource(proxyIndex);
      return m_source_delegate->createWidget(sourceIndex,
                                             proxy_model->sourceModel());
    }

    // Fallback: If no proxy layer exists, pass through unmodified
    return m_source_delegate->createWidget(proxyIndex, model);
  }

  /**
   * @brief Intercepts sizing queries and returns dimension metrics from the
   * underlying delegate.
   * Resolves the correct geometric sizing footprint by performing coordinate
   * translation before checking layout constraints, avoiding viewport line
   * distortion under active sorting/filtering.
   *
   * @param proxyIndex The contextual visual index provided by the view layout
   * loop.
   * @param model Pointer to the active operational model interface instance.
   * @return ftxui::Dimensions The size constraints requested by the element
   * layout layer.
   */
  ftxui::Dimensions sizeHint(const ModelIndex& proxyIndex,
                             const AbstractItemModel* model) const override {
    if (!m_source_delegate) {
      return ItemDelegate::sizeHint(proxyIndex, model);
    }
    // Attempt to check if the model is a AbstractItemModel
    if (const auto* proxy_model =
            dynamic_cast<const AbstractProxyModel*>(model)) {
      // Translate the index down to source-space coordinates
      const ModelIndex sourceIndex = proxy_model->mapToSource(proxyIndex);
      return m_source_delegate->sizeHint(sourceIndex,
                                         proxy_model->sourceModel());
    }

    // Fallback: If no proxy layer exists, pass through unmodified
    return m_source_delegate->sizeHint(proxyIndex, model);
  }
};

}  // namespace ftxmodel

#pragma once
#include <algorithm>
#include <any>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "abstract_item_model.hpp"
#include "unique_id_cache_helper.hpp"

namespace ftxmodel {

/**
 * @class TreeTableModel
 * @brief A compile-time type-safe, multi-column hierarchical model designed for
 * Terminal/View layouts.
 * TreeTableModel eliminates object-oriented inheritance boilerplate by
 * storing rows inside an internal tree node matrix wrapped as a `std::variant`.
 * Custom presentation formatting or editing rules are decoupled into standalone
 * column-definition lambdas.
 * @tparam Types A parameter pack of business domain structs or classes
 * allowed as rows in this tree.
 */
template <typename... Types>
class TreeTableModel : public AbstractItemModel {
 public:
  /** @brief Polymorphic backend storage type matching any valid node
   * configuration. */
  using RowData = std::variant<Types...>;
  /** @brief Function signature used to poll display parameters from variant
   * rows. */
  using DataExtractor = std::function<std::any(const RowData&, int, ItemRole)>;
  /** @brief Function signature used to commit user inputs back down into
   * business variables. */
  using DataMutator =
      std::function<bool(RowData&, int, const std::any&, ItemRole)>;
  /** @brief Function signature used to extract a stable, persistent UI track
   * token from a row configuration. */
  using KeyExtractor = std::function<UniqueNodeId(const RowData&)>;

  /**
   * @struct ColumnDefinition
   * @brief Groups tracking labels alongside evaluation gateways for a single
   * vertical layout track.
   */
  struct ColumnDefinition {
    std::string headerTitle;
    DataExtractor extractor;
    DataMutator mutator = nullptr;
  };

  /**
   * @struct Node
   * @brief Low-level structural vertex element managing parent links, heap
   * lifespans, and row offsets.
   */
  struct Node {
    RowData data;
    Node* parentNode = nullptr;
    std::vector<std::unique_ptr<Node>> children;

    explicit Node(RowData d, Node* p = nullptr)
        : data(std::move(d)), parentNode(p) {}

    /**
     * @brief Calculates the zero-indexed sequential offset position this node
     * occupies within its parent container.
     * @return int The position row integer, or 0 if orphaned or root-level.
     */
    [[nodiscard]] int rowInParent() const noexcept {
      if (!parentNode) {
        return 0;
      }
      const auto& siblings = parentNode->children;
      auto it = std::find_if(
          siblings.begin(), siblings.end(),
          [this](const auto& child) { return child.get() == this; });
      return (it != siblings.end())
                 ? static_cast<int>(std::distance(siblings.begin(), it))
                 : 0;
    }
  };

  struct TreeTableCacheTraits {
    static UniqueNodeId getUniqueId(const TreeTableModel& model, Node* node) {
      auto& nonConstModel = const_cast<TreeTableModel&>(model);
      return nonConstModel.uniqueId(nonConstModel.indexFromNode(node));
    }

    static ModelIndex createIndex(const TreeTableModel& model,
                                  Node* node,
                                  int column) {
      return model.indexFromNode(node, column);
    }
  };

  /**
   * @brief Instantiates a TreeTableModel bound to fixed horizontal layout
   * headers.
   * @param headers An array of string tokens detailing column names.
   */
  explicit TreeTableModel(std::vector<std::string> headers)
      : m_headers(std::move(headers)),
        m_root(std::make_unique<Node>(RowData{})) {
    m_cache.insertDirect({static_cast<const void*>(m_root.get())},
                         m_root.get());
  }

  ~TreeTableModel() override = default;

  /**
   * @brief Registers a universal variant key extractor that extracts a
   * UniqueNodeId based on the underlying type.
   * @param extractor The custom user identity tracking lambda rule.
   */
  void setKeyExtractor(KeyExtractor extractor) {
    m_keyExtractor = std::move(extractor);
    m_cache.invalidate();
  }

  /**
   * @brief Directly exposes the hidden structural root node anchor.
   * @return Node* The top invisible container node pointer context.
   */
  [[nodiscard]] Node* rootNode() const noexcept { return m_root.get(); }

  // ========================================================================
  // AbstractItemModel Contract Overrides
  // ========================================================================

  /**
   * @brief Resolves the stable visual tracking identifier for a designated cell
   * index coordinate.
   * @param index The model tracking node layout cell handle.
   * @return UniqueNodeId The polymorphic lookup key mapping identity stability.
   */
  UniqueNodeId uniqueId(const ModelIndex& index) const override {
    if (!index.isValid()) {
      return {nullptr};
    }
    const auto* node = static_cast<const Node*>(index.internalPointer());

    // Query the custom user-defined type extractor first
    if (m_keyExtractor && node != m_root.get()) {
      return m_keyExtractor(node->data);
    }
    std::string pathString;
    const Node* current = node;

    while (current && current != m_root.get()) {
      pathString += "/" + std::to_string(current->rowInParent());
      current = current->parentNode;
    }

    return {pathString};
  }

  /**
   * @brief Performs a optimized cached lookup pass to discover the
   * coordinates of an active node.
   * @param targetId The stable identity tracking key token being located.
   * @param parent Unused parameter context overridden by universal caching
   * boundaries.
   * @return ModelIndex The discovered cell coordinate layout handle.
   */
  ModelIndex findIndexById(
      const UniqueNodeId& targetId,
      const ModelIndex& parent = ModelIndex()) const override {
    std::ignore = parent;

    return m_cache.findIndexById(*this, targetId, [this](auto& cacheHelper) {
      // Reinsert root anchor context point first
      cacheHelper.insertDirect({static_cast<const void*>(m_root.get())},
                               m_root.get());
      populateCacheRecursively(m_root.get(), cacheHelper);
    });
  }

  /**
   * @brief Overwrites elements inside a specific cell block, managing
   * structural cache updates dynamically.
   * @param index Coordinate pinpoint location of the edited cell.
   * @param value The value data variant being injected.
   * @param role The target application modifier presentation role.
   * @return true if the modification loop accepts and registers changes
   * cleanly.
   */
  bool setData(const ModelIndex& index,
               const std::any& value,
               ItemRole role = ItemRole::EditRole) override {
    if (!index.isValid()) {
      return false;
    }

    auto* node = static_cast<Node*>(index.internalPointer());
    if (index.column() < 0 ||
        index.column() >= static_cast<int>(m_columns.size())) {
      return false;
    }

    const auto& colDef = m_columns[static_cast<size_t>(index.column())];
    if (!colDef.mutator) {
      return false;
    }

    // Capture the old unique ID BEFORE mutating the data structure
    UniqueNodeId oldId = uniqueId(index);

    if (colDef.mutator(node->data, index.column(), value, role)) {
      m_cache.updateKey(oldId, uniqueId(index), node);

      this->dataChanged(index, index);
      return true;
    }
    return false;
  }

  // ========================================================================
  // Structural Modifiers (Ensuring Proper Cache Lifecycles)
  // ========================================================================

  /**
   * @brief Pushes a fresh variant element onto the absolute bottom bounds of a
   * parent sub-branch.
   * @param parentNode Parent context pointer. Pass `nullptr` to target the
   * absolute root frame layer.
   * @param itemData The initialized struct dataset payload.
   * @return Node* Direct pointer access reference straight to the generated
   * heap node item.
   */
  Node* appendChildItem(Node* parentNode, RowData itemData) {
    Node* targetParent = parentNode ? parentNode : m_root.get();
    const int insertPos = static_cast<int>(targetParent->children.size());

    ModelIndex parentIdx =
        (targetParent == m_root.get())
            ? ModelIndex()
            : createIndex(targetParent->rowInParent(), 0, targetParent);

    this->beginInsertRows(parentIdx, insertPos, insertPos);

    auto newNode = std::make_unique<Node>(std::move(itemData), targetParent);
    Node* rawNewNodePtr = newNode.get();

    targetParent->children.push_back(std::move(newNode));
    m_cache.invalidate();

    this->endInsertRows();
    return rawNewNodePtr;
  }

  /**
   * @brief Inserts a variant struct type as a child nested at a specific row
   * position under a parent.
   * @param parentNode Parent context target link. Pass `nullptr` to inject onto
   * the top layer.
   * @param position Zero-indexed layout track offset marking target assignment
   * slot.
   * @param itemData Structured item initialization fields.
   * @return Node* Direct pointer tracking reference straight to the generated
   * node item.
   */
  Node* insertChildItem(Node* parentNode, int position, RowData itemData) {
    Node* targetParent = parentNode ? parentNode : m_root.get();

    // Clamp bounds safely to prevent sequence vector panics
    if (position < 0 ||
        position > static_cast<int>(targetParent->children.size())) {
      position = static_cast<int>(targetParent->children.size());
    }

    ModelIndex parentIdx =
        (targetParent == m_root.get())
            ? ModelIndex()
            : createIndex(targetParent->rowInParent(), 0, targetParent);

    // Notify views to freeze viewport layout coordinates
    this->beginInsertRows(parentIdx, position, position);

    auto newNode = std::make_unique<Node>(std::move(itemData), targetParent);
    Node* rawNewNodePtr = newNode.get();
    targetParent->children.insert(targetParent->children.begin() + position,
                                  std::move(newNode));
    m_cache.invalidate();
    this->endInsertRows();
    return rawNewNodePtr;
  }

  /**
   * @brief Removes a child item at a specific row position from a parent node
   * layout context.
   * @param parentNode Target container context asset. Pass `nullptr` to clean
   * elements from the root frame.
   * @param position Zero-indexed target track index row selected for deletion
   * processing.
   * @return true if operations execute cleanly, false if position thresholds
   * fail boundary matches.
   */
  bool removeChildItem(Node* parentNode, int position) {
    Node* targetParent = parentNode ? parentNode : m_root.get();
    if (position < 0 ||
        position >= static_cast<int>(targetParent->children.size())) {
      return false;
    }

    ModelIndex parentIdx =
        (targetParent == m_root.get())
            ? ModelIndex()
            : createIndex(targetParent->rowInParent(), 0, targetParent);

    this->beginRemoveRows(parentIdx, position, position);

    m_cache.invalidate();

    targetParent->children.erase(targetParent->children.begin() + position);
    this->endRemoveRows();
    return true;
  }

  /**
   * @brief Moves a continuous slice of child rows under a parent from a source
   * index to a destination index.
   * @param parentNode The operational branch context parent. Pass `nullptr` to
   * modify top root arrays.
   * @param sourceRow Base track cell source sequence location identifier.
   * @param destinationRow Target layout landing slot location index.
   * @return true if vectors shift parameters smoothly without boundary
   * conflicts.
   */
  bool moveChildItem(Node* parentNode, int sourceRow, int destinationRow) {
    Node* targetParent = parentNode ? parentNode : m_root.get();
    const int count = static_cast<int>(targetParent->children.size());

    if (sourceRow < 0 || sourceRow >= count || destinationRow < 0 ||
        destinationRow >= count || sourceRow == destinationRow) {
      return false;
    }

    auto& childVec = targetParent->children;
    std::swap(childVec[sourceRow], childVec[destinationRow]);

    ModelIndex topLeft =
        createIndex(std::min(sourceRow, destinationRow), 0,
                    childVec[std::min(sourceRow, destinationRow)].get());
    ModelIndex bottomRight =
        createIndex(std::max(sourceRow, destinationRow), columnCount() - 1,
                    childVec[std::max(sourceRow, destinationRow)].get());

    this->dataChanged(topLeft, bottomRight);
    return true;
  }

  /**
   * @brief Flushes all data rows down to zero, resetting tracking caches clean.
   */
  void clear() {
    this->beginResetModel();
    m_root->children.clear();
    m_cache.clear();
    m_cache.insertDirect({static_cast<const void*>(m_root.get())},
                         m_root.get());
    this->endResetModel();
  }

  // Boilerplate mapping methods remain inline with prior variants
  ModelIndex index(int row,
                   int column,
                   const ModelIndex& parent = ModelIndex()) const override {
    if (row < 0 || column < 0 || column >= static_cast<int>(m_headers.size())) {
      return {};
    }
    const Node* parentNode =
        parent.isValid() ? static_cast<const Node*>(parent.internalPointer())
                         : m_root.get();
    if (parentNode && row < static_cast<int>(parentNode->children.size())) {
      return createIndex(
          row, column,
          const_cast<Node*>(
              parentNode->children[static_cast<size_t>(row)].get()));
    }
    return {};
  }
  ModelIndex parent(const ModelIndex& child) const override {
    if (!child.isValid()) {
      return {};
    }
    const auto* childNode = static_cast<const Node*>(child.internalPointer());
    if (!childNode) {
      return {};
    }
    Node* parentNode = childNode->parentNode;
    if (!parentNode || parentNode == m_root.get()) {
      return {};
    }
    return createIndex(parentNode->rowInParent(), 0, parentNode);
  }
  int rowCount(const ModelIndex& parent = ModelIndex()) const override {
    const Node* parentNode =
        parent.isValid() ? static_cast<const Node*>(parent.internalPointer())
                         : m_root.get();
    return parentNode ? static_cast<int>(parentNode->children.size()) : 0;
  }
  int columnCount(const ModelIndex& parent = ModelIndex()) const override {
    std::ignore = parent;
    return static_cast<int>(m_headers.size());
  }
  std::any data(const ModelIndex& index,
                ItemRole role = ItemRole::DisplayRole) const override {
    if (!index.isValid()) {
      return {};
    }
    if (role == ItemRole::UniqueIdentifierRole) {
      return uniqueId(index);
    }
    const auto* node = static_cast<const Node*>(index.internalPointer());
    if (index.column() < 0 ||
        index.column() >= static_cast<int>(m_columns.size())) {
      return {};
    }
    return m_columns[static_cast<size_t>(index.column())].extractor(
        node->data, index.column(), role);
  }

  /**
   * @brief Sets logic behaviors binding display updates and mutations for an
   * isolated column track section.
   * @param column Target vertical track sequence column index.
   * @param extractor Query lambda formatting cell content returns.
   * @param mutator Optional execution lambda updating states upon receiving
   * input signals.
   */
  void setColumnLogic(int column,
                      DataExtractor extractor,
                      DataMutator mutator = nullptr) {
    if (column < 0) {
      return;
    }
    if (column >= static_cast<int>(m_columns.size())) {
      m_columns.resize(static_cast<size_t>(column + 1));
    }
    m_columns[static_cast<size_t>(column)] = {
        m_headers[static_cast<size_t>(column)], std::move(extractor),
        std::move(mutator)};
  }

  /**
   * @brief Translates a low-level structural Node pointer into its active
   * coordinate handle.
   * @param node Direct raw pointer reference to the targeted structural node
   * item.
   * @param column The sequential target data track metric column (defaults to
   * index 0).
   * @return ModelIndex The localized visual coordinate handle layout token.
   */
  [[nodiscard]] ModelIndex indexFromNode(const Node* node,
                                         int column = 0) const noexcept {
    if (!node || node == m_root.get()) {
      return {};
    }
    return createIndex(node->rowInParent(), column, const_cast<Node*>(node));
  }

  /**
   * @brief Safely extracts the low-level structural Node pointer context from a
   * transient coordinate handle.
   * @param index The targeted layout coordinate handle being evaluated.
   * @return Node* Direct tracking memory address reference, or `nullptr` if the
   * layout index token is invalid.
   */
  [[nodiscard]] Node* nodeFromIndex(const ModelIndex& index) const noexcept {
    if (!index.isValid()) {
      return m_root.get();  // Fall back to our invisible top structural root
                            // container context
    }
    return static_cast<Node*>(index.internalPointer());
  }

 protected:
  void populateCacheRecursively(
      Node* node,
      UniqueIdCacheHelper<TreeTableModel, Node*, TreeTableCacheTraits>&
          cacheHelper) const {
    if (!node) {
      return;
    }
    for (size_t i = 0; i < node->children.size(); ++i) {
      Node* child = node->children[i].get();
      cacheHelper.insertDirect(uniqueId(indexFromNode(child)), child);
      populateCacheRecursively(child, cacheHelper);
    }
  }

  std::vector<std::string> m_headers;
  std::vector<ColumnDefinition> m_columns;
  std::unique_ptr<Node> m_root;
  KeyExtractor m_keyExtractor = nullptr;
  UniqueIdCacheHelper<TreeTableModel<Types...>, Node*, TreeTableCacheTraits>
      m_cache;
};

}  // namespace ftxmodel

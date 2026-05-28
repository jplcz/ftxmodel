#pragma once
#include <cstdint>
#include <variant>

namespace ftxmodel {

class AbstractItemModel;

/**
 * @enum ItemRole
 * @brief Defines the semantic purpose of data requested or sent to a model
 * cell.
 * Data models store different properties (display text, edit states, tooltip
 * strings) inside a single logical cell coordinate. Views and delegates use
 * these roles to extract the exact property variant required for rendering or
 * editing inputs.
 */
enum class ItemRole : int {
  DisplayRole,
  EditRole,
  ToolTipRole,
  CheckedRole,
  UniqueIdentifierRole
};

/**
 * @enum ItemFlag
 * @brief Bitwise flags describing user interaction capabilities for a specific
 * model index.
 */
enum ItemFlag : int {
  NoItemFlags =
      0, /**< Empty state indicator. Item is non-interactive and grayed out. */
  ItemIsEnabled =
      1 << 0, /**< The user can view and structurally interact with the cell. */
  ItemIsSelectable = 1 << 1, /**< The layout navigation engine can lock cursor
                                focus onto this row. */
  ItemIsEditable =
      1 << 2, /**< Input fields can be dynamically spun up to update values. */
  ItemIsUserCheckable =
      1 << 3 /**< Cell acts as an interactive boolean state flag toggle. */
};

/**
 * @brief Type alias for tracking combined bitwise combinations of @see ItemFlag
 * rules.
 */
using ItemFlags = int;

/**
 * @enum Orientation
 * @brief Specifies dimensional axis alignment for drawing headers or split
 * frames.
 */
enum class Orientation {
  Horizontal, /**< Horizontal headers tracking cross-cutting table column
                 metadata tracks. */
  Vertical /**< Vertical headers tracking sequential list or table row metadata
              tracks. */
};

/**
 * @brief A polymorphic type-safe variant proxy identifying a data node across
 * background refreshes. To decouple UI display layouts from volatile transient
 * integer indices, UniqueNodeId holds one of three type allocations (heap
 * address, database long key, or raw path string). This keeps open tree
 * branches expanded and highlighted selection rows centered, even across
 * background sorting loops.
 */
using UniqueNodeId = std::variant<const void*, std::int64_t, std::string>;

/**
 * @struct ModelIndex
 * @brief A lightweight, transient coordinate handle used to locate cell assets
 * inside a data model.
 * ModelIndex serves as the primary navigation unit across the entire
 * Model-View architecture. It is completely stateless, copyable, and
 * stack-allocated. It stores local grid coordinates (row, column), an internal
 * memory optimization pointer, and a weak link back to its model authority.
 * @note ModelIndex handles should never be stored permanently or cached
 * long-term across asynchronous loops. If you need a persistent, long-term
 * anchor to track structural elements, resolve their stable identities via @see
 * uniqueId() instead.
 */
struct ModelIndex {
  /**
   * @brief Construct an invalid default ModelIndex coordinate handle token.
   * Invalid indices represent parent contexts for absolute top root-level
   * nodes.
   */
  constexpr ModelIndex() noexcept = default;

  /**
   * @brief Low-level primary constructor initializing explicit coordinate
   * positions.
   * @param r Zero-indexed row coordinate.
   * @param c Zero-indexed column coordinate.
   * @param ptr Optional, low-level pointer mapping this token straight to a
   * specific backend raw data node.
   * @param m Direct pointer link back to the owning model instance authority.
   */
  constexpr ModelIndex(int r,
                       int c,
                       void* ptr,
                       const AbstractItemModel* m) noexcept
      : m_row(r), m_column(c), m_internal_pointer(ptr), m_model(m) {}

  /**
   * @brief Returns the row position tracker slot location.
   */
  [[nodiscard]] constexpr int row() const noexcept { return m_row; }

  /**
   * @brief Returns the column position tracker slot location.
   */
  [[nodiscard]] constexpr int column() const noexcept { return m_column; }

  /**
   * @brief Returns the internal model node memory optimizations pointer
   * optimization mapping.
   * @warning Consumers outside concrete subclass model definitions should avoid
   * inspect parsing this value directly. Treat it as an opaque token pass.
   */
  [[nodiscard]] constexpr void* internalPointer() const noexcept {
    return m_internal_pointer;
  }

  /**
   * @brief Validates if the index handle refers to a meaningful coordinate
   * block inside an active model.
   * @return true if row and column bounds are non-negative and a target model
   * relationship is configured.
   */
  [[nodiscard]] constexpr bool isValid() const noexcept {
    return m_row >= 0 && m_column >= 0 && m_model != nullptr;
  }

  /**
   * @brief Piecewise equality comparison pass verifying coordinate properties
   * match identically.
   */
  [[nodiscard]] constexpr bool operator==(
      const ModelIndex& rhs) const noexcept {
    return m_row == rhs.m_row && m_column == rhs.m_column &&
           m_internal_pointer == rhs.m_internal_pointer &&
           m_model == rhs.m_model;
  }

  /**
   * @brief Inline convenience route looking upward to find the parent of this
   * index handle.
   * @return ModelIndex The parent handle, or an invalid empty token if this
   * index has no parent.
   */
  [[nodiscard]] ModelIndex parent() const;

  /**
   * @brief Inline convenience route extracting cell variant data payloads based
   * on a targeted role.
   * @param role Semantic purpose of data request configuration lookup.
   * @return std::any The encapsulated cell details, or an empty `std::any` if
   * lookup fails.
   */
  [[nodiscard]] std::any data(ItemRole role = ItemRole::DisplayRole) const;

  /**
   * @brief Inline convenience route reading permissions bitmask behaviors for
   * this cell handle block.
   */
  [[nodiscard]] ItemFlags flags() const;

  /**
   * @brief Inline convenience route looking up the data-first persistent
   * tracking key.
   * @return UniqueNodeId The stable identifier used for UI view position
   * synchronization across data reloads.
   */
  [[nodiscard]] UniqueNodeId uniqueId() const;

  [[nodiscard]] const AbstractItemModel* model() const { return m_model; }

 private:
  int m_row = -1;    /**< Sequential row boundary offset layout locator. */
  int m_column = -1; /**< Sequential column data tracker metric tracker. */
  void* m_internal_pointer =
      nullptr; /**< Low-level opaque data payload hook anchor. */
  const AbstractItemModel* m_model =
      nullptr; /**< Linked source of truth model authority layer. */
};

struct UniqueNodeIdHash {
  std::size_t operator()(const UniqueNodeId& id) const noexcept {
    return std::visit(
        []<typename T0>(const T0& value) -> std::size_t {
          using T = std::decay_t<T0>;
          if constexpr (std::is_same_v<T, const void*>) {
            return std::hash<const void*>{}(value);
          } else if constexpr (std::is_same_v<T, std::int64_t>) {
            return std::hash<std::int64_t>{}(value);
          } else if constexpr (std::is_same_v<T, std::string>) {
            return std::hash<std::string>{}(value);
          }
          return 0;
        },
        id);
  }
};

struct UniqueNodeIdEqual {
  bool operator()(const UniqueNodeId& lhs,
                  const UniqueNodeId& rhs) const noexcept {
    return lhs == rhs;
  }
};

}  // namespace ftxmodel

#pragma once
#include <any>
#include <format>
#include <sigslot/signal.hpp>
#include <string>
#include <string_view>

#include "any_to_string.hpp"
#include "model_index.hpp"

namespace ftxmodel {

/**
 * @class AbstractItemModel
 * @brief The abstract base class defining the standard interface contract for
 * non-contiguous data models.
 * AbstractItemModel provides a unified structure for managing 1D lists, 2D
 * tabular grids, and multi-dimensional hierarchical trees. It separates data
 * storage logic from layout views using transient coordinate handles (@see
 * ModelIndex) and type-safe data-first identifiers (@see UniqueNodeId).
 * Subclasses implementing custom datasets must provide implementations for:
 * - index()
 * - parent()
 * - rowCount()
 * - columnCount()
 * - data()
 * * @note When altering structural hierarchies (e.g., adding or removing
 * entries), custom implementations must wrap modifications in the appropriate
 * `begin...` and `end...` signal lifecycle steps to prevent visual view cache
 * corruption.
 */
class AbstractItemModel {
 public:
  /**
   * @brief Virtual destructor ensuring clean cleanup of inherited
   * implementations.
   */
  virtual ~AbstractItemModel() = default;

  /**
   * @brief Generates a ModelIndex layout handle for a specific cell coordinate.
   * @param row The zero-indexed structural row location.
   * @param column The zero-indexed structural column location.
   * @param parent The parent container index context. Defaults to an invalid
   * root-level index.
   * @return ModelIndex A transient, localized coordinate handle representing
   * the cell space.
   */
  virtual ModelIndex index(int row,
                           int column,
                           const ModelIndex& parent = ModelIndex()) const = 0;

  /**
   * @brief Evaluates the topological layout tree upward to discover the parent
   * of a given child.
   * @param child The index handle whose parent is being requested.
   * @return ModelIndex The parent index handle, or an invalid ModelIndex if the
   * child is a root-level node.
   */
  virtual ModelIndex parent(const ModelIndex& child) const = 0;

  /**
   * @brief Queries the total number of child rows nested beneath a specific
   * parent handle context.
   * @param parent The parent node handle context. Defaults to the root layer.
   * @return int The total number of valid tracking rows.
   */
  virtual int rowCount(const ModelIndex& parent = ModelIndex()) const = 0;

  /**
   * @brief Queries the total number of data metric columns configured for
   * children of a given parent.
   * @param parent The parent node handle context. Defaults to the root layer.
   * @return int The total number of valid tracking data tracks/columns.
   */
  virtual int columnCount(const ModelIndex& parent = ModelIndex()) const = 0;

  /**
   * @brief Optimizes lookups by verifying if a specific parent node context
   * contains child nodes.
   * Views utilize this method as a fast optimization boundary pass to
   * determine whether to draw branch expansion handles (`[+]`/`[-]`)
   * without executing full row count evaluations.
   * @param parent The parent index context being evaluated.
   * @return true if the node contains one or more child rows.
   * @return false if the node is a flat leaf entry.
   */
  virtual bool hasChildren(const ModelIndex& parent = ModelIndex()) const {
    return rowCount(parent) > 0;
  }

  /**
   * @brief Pulls raw data from the model associated with a specific functional
   * presentation role.
   * @param index The cell coordinate handle targeted for inspection.
   * @param role The semantic purpose of the requested data (@see ItemRole).
   * @return std::any An encapsulated polymorphic variant wrapper containing the
   * target data.
   */
  virtual std::any data(const ModelIndex& index,
                        ItemRole role = ItemRole::DisplayRole) const = 0;

  /**
   * @brief Updates backend properties associated with a targeted layout cell
   * coordinate handle.
   * Concrete models that support data modifications must override this
   * method, apply updates to their internal structures, and emit the @see
   * dataChanged signal before returning.
   * @param index The target cell coordinate handle being edited.
   * @param value The raw update data boxed inside a polymorphic container.
   * @param role The specific configuration role target being modified.
   * @return true if the backend data was successfully applied.
   * @return false if edits are blocked, unsupported, or if coordinates are
   * invalid.
   */
  virtual bool setData(const ModelIndex& index,
                       const std::any& value,
                       ItemRole role = ItemRole::EditRole) {
    std::ignore = index;
    std::ignore = value;
    std::ignore = role;
    return false;
  }

  /**
   * @brief Pulls display parameters or text strings used to populate view panel
   * headers.
   * @param section The positional index of the header track (Column number or
   * Row number).
   * @param orientation The structural direction alignment (Horizontal maps
   * columns, Vertical maps rows).
   * @param role The formatting role request target.
   * @return std::any The formatted header data payload, or a stringified
   * section integer by default.
   */
  virtual std::any headerData(int section,
                              Orientation orientation,
                              ItemRole role = ItemRole::DisplayRole) const {
    std::ignore = orientation;
    if (role == ItemRole::DisplayRole) {
      return std::to_string(section);
    }
    return {};
  }

  /**
   * @brief Dynamically updates column or row headers displayed at the frame
   * borders of interactive views. Subclasses must emit @see headerDataChanged
   * if the new header parameters are accepted and applied.
   * @param section The positional track index being updated.
   * @param orientation The directional alignment track mapping.
   * @param value The value container containing the new label properties.
   * @param role The target application configuration modifier role.
   * @return true if header updates were applied successfully.
   * @return false if headers are read-only or section bounds fail validations.
   */
  virtual bool setHeaderData(int section,
                             Orientation orientation,
                             const std::any& value,
                             ItemRole role = ItemRole::EditRole) {
    std::ignore = section;
    std::ignore = orientation;
    std::ignore = value;
    std::ignore = role;
    return false;
  }

  /**
   * @brief Returns operational state rules applied to a cell coordinate,
   * dictating user interaction capabilities. Interacting terminal views call
   * this method to verify if an item can accept cursor selections, trigger
   * input fields, or be rendered in a grayed-out state.
   * @param index The cell coordinate being queried.
   * @return ItemFlags A combined bitmask specifying interaction rules (@see
   * ItemFlag).
   */
  virtual ItemFlags flags(const ModelIndex& index) const {
    if (!index.isValid()) {
      return ItemFlag::NoItemFlags;
    }
    return ItemFlag::ItemIsEnabled |
           ItemFlag::ItemIsSelectable;  // Default sensible fallback
  }

  /**
   * @brief Resolves a stable, data-first unique identity token for a layout
   * index coordinate.
   * To ensure selection highlights and expanded tree branches don't collapse
   * when data reloads, this method provides a persistent key using an embedded
   * three-tier priority fallback chain:
   * 1. Evaluates custom tokens provided explicitly via `data(index,
   * ItemRole::UniqueIdentifierRole)`.
   * 2. Evaluates the zero-allocation raw pointer value
   * (`index.internalPointer()`).
   * 3. Generates a structural grid path coordinate string (e.g.,
   * `"0/3/path:"`).
   * @param index The cell handle targeted for tracking resolution.
   * @return UniqueNodeId A polymorphic variant identifying the data node across
   * refreshes.
   */
  virtual UniqueNodeId uniqueId(const ModelIndex& index) const {
    if (!index.isValid()) {
      return {nullptr};
    }
    const auto roleIdData = data(index, ItemRole::UniqueIdentifierRole);
    if (roleIdData.type() == typeid(UniqueNodeId)) {
      return std::any_cast<UniqueNodeId>(roleIdData);
    }
    if (roleIdData.type() == typeid(std::string)) {
      return {std::any_cast<std::string>(roleIdData)};
    }
    if (roleIdData.type() == typeid(int)) {
      return {std::any_cast<int>(roleIdData)};
    }
    if (roleIdData.type() == typeid(int64_t)) {
      return {std::any_cast<int64_t>(roleIdData)};
    }
    // Fall back to internal pointer address first
    if (index.internalPointer()) {
      return {index.internalPointer()};
    }
    // Final fallback via path
    std::string pathString = "path:";
    ModelIndex current = index;
    while (current.isValid()) {
      pathString = std::format("{}/{}", current.row(), pathString);
      current = parent(current);
    }
    return {pathString};
  }

  /**
   * @brief Executes a reverse-lookup query pass to locate where a specific
   * unique ID currently lives.
   * This method recursively traverses the active model hierarchy to match the
   * structural identity token, allowing views to re-map selections data-first
   * after background re-sorts or layout updates.
   * @param targetId The tracking identity key whose coordinate location is
   * being queried.
   * @param parent The parent context boundary where the recursive query begins.
   * Defaults to root.
   * @return ModelIndex The matching valid layout coordinate handle, or an
   * invalid ModelIndex if not found.
   */
  virtual ModelIndex findIndexById(
      const UniqueNodeId& targetId,
      const ModelIndex& parent = ModelIndex()) const {
    if (targetId == UniqueNodeId{nullptr}) {
      return {};
    }
    const int rows = rowCount(parent);
    for (int r = 0; r < rows; ++r) {
      ModelIndex currentIdx = index(r, 0, parent);
      if (!currentIdx.isValid()) {
        continue;
      }
      if (uniqueId(currentIdx) == targetId) {
        return currentIdx;
      }
      if (rowCount(currentIdx) > 0) {
        ModelIndex childMath = findIndexById(targetId, currentIdx);
        if (childMath.isValid()) {
          return childMath;
        }
      }
    }
    return ModelIndex();
  }

  /**
   * @brief Helper utility designed to safely extract string data payloads for
   * terminal display.
   * Automatically unwraps common text container representations
   * (`std::string`, `std::string_view`, and raw `const char*`) hidden inside
   * the polymorphic cell storage matrix.
   * @param index The cell coordinate handle targeted for string extraction.
   * @param role The target semantic lookup role. Defaults to DisplayRole.
   * @return std::string The primitive string text copy, or an empty string if
   * conversion types fail.
   */
  std::string textData(const ModelIndex& index,
                       const ItemRole role = ItemRole::DisplayRole) const {
    return AnyToStringTranslator::Translate(data(index, role));
  }

 protected:
  /**
   * @brief Low-level internal index manufacturing factory method for concrete
   * subclasses.
   * Instantiates a clean ModelIndex coordinate token pre-bound with a
   * tracking reference to this data model instance as its structural authority
   * source.
   * @param row The row assignment tracker value.
   * @param column The column assignment tracker value.
   * @param ptr An optional, custom raw memory pointer mapping this index
   * directly to a backend data block node.
   * @return ModelIndex The initialized workspace layout handle.
   */
  ModelIndex createIndex(const int row,
                         const int column,
                         void* ptr = nullptr) const {
    return {row, column, ptr, this};
  }

 public:
  // ========================================================================
  // SigSlot Core Notification Signals Channel Maps
  // ========================================================================

  /**
   * @brief Signal emitted whenever content properties inside an existing cell
   * matrix range are modified.
   * @param topLeft The upper-left boundary index coordinate token of the
   * modified cell block.
   * @param bottomRight The lower-right boundary index coordinate token of the
   * modified cell block.
   */
  sigslot::signal_st<const ModelIndex&, const ModelIndex&> dataChanged;

  /**
   * @brief Signal emitted whenever horizontal column layouts or structural
   * header parameters change.
   * @param section The positional index track being updated.
   * @param role The target modification identifier role.
   */
  sigslot::signal_st<int, int> headerDataChanged;

  /**
   * @brief Signal alerting views to freeze their frame caches immediately
   * before rows are added.
   * @param parent The parent destination index handle where fresh rows are
   * being appended.
   * @param start The planned index position that the first newly created row
   * will occupy.
   * @param end The planned index position that the final newly created row will
   * occupy.
   */
  sigslot::signal_st<const ModelIndex&, int, int> beginInsertRows;

  /**
   * @brief Signal alerting views that structural row expansions are complete,
   * triggering a frame layout cache flush.
   */
  sigslot::signal_st<> endInsertRows;

  /**
   * @brief Signal alerting views to freeze their frames immediately before rows
   * are deleted from memory.
   * @param parent The parent container index context targeted for compression.
   * @param start The starting row index boundary marked for removal.
   * @param end The ending row index boundary marked for removal.
   */
  sigslot::signal_st<const ModelIndex&, int, int> beginRemoveRows;

  /**
   * @brief Signal alerting views that structural row deletions are complete,
   * forcing layout dimensions to compress.
   */
  sigslot::signal_st<> endRemoveRows;

  /**
   * @brief Signal alerting views to freeze layout structures before column
   * metrics expand.
   * @param parent The parent container context handle target.
   * @param start The initial layout track location marked for injection.
   * @param end The terminating layout track location marked for injection.
   */
  sigslot::signal_st<const ModelIndex&, int, int> beginInsertColumns;

  /**
   * @brief Signal alerting views that horizontal data columns have been added,
   * triggering scroll-indicator metrics updates.
   */
  sigslot::signal_st<> endInsertColumns;

  /**
   * @brief Signal alerting views that column data tracks are being cut out of
   * memory.
   * @param parent The parent container context handle target.
   * @param start The initial column track index marked for truncation.
   * @param end The final column track index marked for truncation.
   */
  sigslot::signal_st<const ModelIndex&, int, int> beginRemoveColumns;

  /**
   * @brief Signal alerting views that horizontal data columns have been
   * removed, forcing cell widths to re-align.
   */
  sigslot::signal_st<> endRemoveColumns;

  sigslot::signal_st<> beginResetModel;
  sigslot::signal_st<> endResetModel;
};

// ========================================================================
// Inline Forwarding Declarations (Decoupling index footprints from model trees)
// ========================================================================

inline ModelIndex ModelIndex::parent() const {
  return m_model ? m_model->parent(*this) : ModelIndex();
}

inline std::any ModelIndex::data(ItemRole role) const {
  return m_model ? m_model->data(*this, role) : std::any();
}

inline ItemFlags ModelIndex::flags() const {
  return m_model ? m_model->flags(*this) : ItemFlag::NoItemFlags;
}

inline UniqueNodeId ModelIndex::uniqueId() const {
  return m_model ? m_model->uniqueId(*this) : UniqueNodeId{nullptr};
}

}  // namespace ftxmodel

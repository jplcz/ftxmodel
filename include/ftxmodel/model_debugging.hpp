#include <any>
#include <sstream>
#include <string>
#include "abstract_item_model.hpp"

namespace ftxmodel {

/**
 * @brief Traverses a model layout recursively, appending its nodes to a text
 * stream.
 */
inline void dumpModelTreeHelper(const AbstractItemModel& model,
                                const ModelIndex& parent,
                                const int depth,
                                const std::string& prefix,
                                std::stringstream& ss) {
  int rows = model.rowCount(parent);
  int cols = model.columnCount(parent);

  for (int r = 0; r < rows; ++r) {
    // Resolve whether this is the final sibling node in the active branch
    // block
    const bool isLast = (r == rows - 1);
    std::string marker = isLast ? "└── " : "├── ";

    // Fetch the primary anchor column (Column 0 - typically the node Key)
    ModelIndex primaryIdx = model.index(r, 0, parent);
    std::string keyText = "[Invalid Index]";

    if (primaryIdx.isValid()) {
      keyText = model.textData(primaryIdx);
    }

    // Collect and format any supplementary metadata columns (Type, Value,
    // etc.)
    std::string metadataColumns;
    for (int c = 1; c < cols; ++c) {
      if (ModelIndex colIdx = model.index(r, c, parent); colIdx.isValid()) {
        metadataColumns += " | " + model.textData(colIdx);
      }
    }

    // Print the current line to the stream complete with indentation rails
    ss << prefix << marker << keyText << metadataColumns << "\n";

    // If this node has sub-children, recursively branch deeper into the
    // matrix
    if (model.hasChildren(primaryIdx)) {
      // Append vertical guides to the prefix string unless this was the last
      // sibling branch
      std::string nextPrefix = prefix + (isLast ? "    " : "│   ");
      dumpModelTreeHelper(model, primaryIdx, depth + 1, nextPrefix, ss);
    }
  }
}

/**
 * @brief Public interface that returns a formatted visual string of an
 * AbstractItemModel structure.
 * @param model The target data model to audit.
 * @return A tree-structured string representing the model hierarchy and
 * metadata values.
 */
[[nodiscard]] inline std::string dumpModelToString(
    const AbstractItemModel& model) {
  std::stringstream ss;
  ss << "Root\n";
  dumpModelTreeHelper(model, ModelIndex(), 0, "", ss);
  return ss.str();
}

}  // namespace ftxmodel

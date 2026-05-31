#pragma once
#include <sqlite3.h>
#include <any>
#include <cstdint>
#include <format>
#include <ftxmodel/abstract_item_model.hpp>
#include <ftxmodel/any_to_string.hpp>
#include <string>
#include <utility>
#include <vector>

namespace ftxmodel {

class SqliteQueryModel : public AbstractItemModel {
 public:
  enum class IdentityMode {
    RowEntity,       // Keeps focus on the entire row record (Default)
    IndividualCell,  // Focuses individual spreadsheet cells
    PrimaryKey       // Tracks the database primary key token explicitly
  };

 private:
  sqlite3* db_{nullptr};
  std::vector<std::string> headers_;
  std::vector<std::vector<std::any>> cache_;
  IdentityMode m_id_mode = IdentityMode::RowEntity;
  // Only utilized during IdentityMode::PrimaryKey
  int m_primary_key_column = 0;

 public:
  // The model accepts a raw pointer to an actively open database connection
  explicit SqliteQueryModel(sqlite3* db) : db_(db) {}
  ~SqliteQueryModel() override = default;

  /**
   * @brief Configures how unique nodes keys are calculated across view
   * switches.
   */
  void setIdentityMode(IdentityMode mode, int primaryKeyColumn = 0) {
    // Notify views to recalculate selection indexes safely
    this->beginResetModel();
    m_id_mode = mode;
    m_primary_key_column = primaryKeyColumn;
    this->endResetModel();
  }

  /**
   * @brief Evaluates an SQL query string, dynamically parses column types,
   * and populates the row coordinate caches.
   * @return True if the statement safely executed.
   */
  bool setQuery(const std::string& sql,
                const std::vector<std::any>& params = {}) {
    if (!db_) {
      return false;
    }

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
      return false;  // SQL Statement failed compilation or syntax validation
    }

    // --- SAFELY BIND EACH PARAMETER ---
    // SQLite bind array slots are 1-indexed (First '?' is parameter index 1)
    for (int i = 0; i < static_cast<int>(params.size()); ++i) {
      const std::any& p = params[static_cast<size_t>(i)];
      int bind_idx = i + 1;
      int bind_rc = SQLITE_OK;

      if (!p.has_value()) {
        bind_rc = sqlite3_bind_null(stmt, bind_idx);
      } else if (p.type() == typeid(int64_t)) {
        bind_rc = sqlite3_bind_int64(stmt, bind_idx, std::any_cast<int64_t>(p));
      } else if (p.type() == typeid(int)) {
        bind_rc = sqlite3_bind_int(stmt, bind_idx, std::any_cast<int>(p));
      } else if (p.type() == typeid(double)) {
        bind_rc = sqlite3_bind_double(stmt, bind_idx, std::any_cast<double>(p));
      } else if (p.type() == typeid(std::string)) {
        const std::string& str = std::any_cast<const std::string&>(p);
        // SQLITE_TRANSIENT tells SQLite to make its own copy of the string data
        // internally
        bind_rc = sqlite3_bind_text(stmt, bind_idx, str.c_str(), -1,
                                    SQLITE_TRANSIENT);
      } else {
        // Unhandled type constraint fallback
        sqlite3_finalize(stmt);
        return false;
      }

      if (bind_rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return false;
      }
    }

    int col_count = sqlite3_column_count(stmt);
    if (col_count < 0) {
      return false;
    }

    this->beginResetModel();

    headers_.clear();
    cache_.clear();

    headers_.reserve(static_cast<size_t>(col_count));
    for (int i = 0; i < col_count; ++i) {
      const char* col_name = sqlite3_column_name(stmt, i);
      headers_.emplace_back(col_name ? col_name : "");
    }

    // Step through the evaluation rows and pack them polymorphically
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      std::vector<std::any> row_data;
      row_data.reserve(static_cast<size_t>(col_count));

      for (int i = 0; i < col_count; ++i) {
        int type = sqlite3_column_type(stmt, i);

        switch (type) {
          case SQLITE_INTEGER:
            // SQLite integers are signed 64-bit values natively
            row_data.emplace_back(
                static_cast<int64_t>(sqlite3_column_int64(stmt, i)));
            break;
          case SQLITE_FLOAT:
            row_data.emplace_back(sqlite3_column_double(stmt, i));
            break;
          case SQLITE_TEXT: {
            const unsigned char* text_ptr = sqlite3_column_text(stmt, i);
            row_data.emplace_back(std::string(
                text_ptr ? reinterpret_cast<const char*>(text_ptr) : ""));
            break;
          }
          case SQLITE_NULL:
          default:
            row_data.emplace_back(
                std::any());  // Represents an empty/NULL field block
            break;
        }
      }
      cache_.push_back(std::move(row_data));
    }

    sqlite3_finalize(stmt);

    // Conclude the structural switchover, forcing view matrix recalculation
    this->endResetModel();
    return true;
  }

  int rowCount(const ModelIndex& parent = ModelIndex()) const override {
    return parent.isValid() ? 0 : static_cast<int>(cache_.size());
  }

  int columnCount(const ModelIndex& parent = ModelIndex()) const override {
    return parent.isValid() ? 0 : static_cast<int>(headers_.size());
  }

  ModelIndex index(const int row,
                   const int column,
                   const ModelIndex& parent = ModelIndex()) const override {
    if (parent.isValid() || row < 0 || row >= rowCount() || column < 0 ||
        column >= columnCount()) {
      return {};
    }
    // Track row indices directly as an identifier sequence pointer token
    auto identifier_ptr =
        reinterpret_cast<void*>(static_cast<uintptr_t>(row + 1));
    return createIndex(row, column, identifier_ptr);
  }

  ModelIndex parent(const ModelIndex&) const override { return {}; }

  std::any data(const ModelIndex& index,
                ItemRole role = ItemRole::DisplayRole) const override {
    if (!index.isValid() || index.row() >= rowCount() ||
        index.column() >= columnCount()) {
      return {};
    }
    if (role == ItemRole::DisplayRole) {
      return cache_[static_cast<size_t>(index.row())]
                   [static_cast<size_t>(index.column())];
    }
    return {};
  }

  UniqueNodeId uniqueId(const ModelIndex& index) const override {
    if (!index.isValid() || index.row() >= rowCount()) {
      return {};
    }

    switch (m_id_mode) {
      case IdentityMode::IndividualCell: {
        // Cell-Specific Identity (Column matters)
        return {std::format("sql_cell_{}_{}", index.row(), index.column())};
      }

      case IdentityMode::PrimaryKey: {
        // Database Entity Tracking
        if (m_primary_key_column >= 0 && m_primary_key_column < columnCount()) {
          const auto& row_data = cache_[static_cast<size_t>(index.row())];
          const std::any& pk_val =
              row_data[static_cast<size_t>(m_primary_key_column)];

          if (pk_val.has_value()) {
            return {std::format("db_pk_{}",
                                AnyToStringTranslator::Translate(pk_val))};
          }
        }
        // Fallthrough if column index is out of bounds or data is invalid
        [[fallthrough]];
      }

      case IdentityMode::RowEntity:
      default: {
        // Row Record Level Fallback Identity
        return {std::format("sql_row_{}", index.row())};
      }
    }
  }

  std::any headerData(int section,
                      Orientation orientation,
                      ItemRole role = ItemRole::DisplayRole) const override {
    if (role != ItemRole::DisplayRole) {
      return {};
    }
    if (orientation == Orientation::Horizontal) {
      if (section < 0 || section >= columnCount()) {
        return {};
      }
      return headers_[static_cast<size_t>(section)];
    } else {
      if (section < 0 || section >= rowCount()) {
        return std::any();
      }
      return std::to_string(section + 1);
    }
  }

  ModelIndex findIndexById(
      const UniqueNodeId& targetId,
      const ModelIndex& parent = ModelIndex()) const override {
    // Database tables are strictly flat root layouts; bypass if a nested parent
    // is passed
    if (parent.isValid() || targetId == UniqueNodeId{nullptr}) {
      return {};
    }

    const int rows = rowCount();
    const int cols = columnCount();

    // Scan through our flat row cache blocks
    for (int r = 0; r < rows; ++r) {
      // Evaluate row-level or cell-level unique ID match conditions.
      // We check column 0 as the primary record row key identifier.
      const ModelIndex primaryRowIdx = index(r, 0);

      if (uniqueId(primaryRowIdx) == targetId) {
        // Match hit! Return column 0 for this record entry row
        return primaryRowIdx;
      }

      if (m_id_mode == IdentityMode::IndividualCell) {
        for (int c = 1; c < cols; ++c) {
          ModelIndex cellIdx = index(r, c);
          if (uniqueId(cellIdx) == targetId) {
            return cellIdx;  // Return precision cell coordinate match
          }
        }
      }
    }

    return {};  // No structural match found across cache segments
  }
};

}  // namespace ftxmodel

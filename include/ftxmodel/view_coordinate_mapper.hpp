#pragma once
#include <array>
#include <ftxmodel/model_index.hpp>
#include <ftxui/dom/elements.hpp>
#include <memory>
#include <optional>
#include <vector>

namespace ftxmodel {
class ViewCoordinateMapper {
 private:
  struct MappingEntry {
    ftxui::Box box;
    ModelIndex index;
  };

  // Compile-time fixed block dimensions ensuring clean cache-line packing
  static constexpr size_t CHUNK_SIZE = 64;
  using ChunkType = std::array<MappingEntry, CHUNK_SIZE>;

  // List-of-pointers structure keeping allocated buckets entirely stable
  std::vector<std::unique_ptr<ChunkType>> m_chunks;

  size_t m_active_count = 0;

 public:
  ViewCoordinateMapper() {
    // Seed an initial memory bucket immediately on instantiation
    m_chunks.push_back(std::make_unique<ChunkType>());
  }

  /**
   * @brief High-frequency loop reset anchor.
   * Keeps all underlying allocated memory buckets completely intact.
   */
  void reset() noexcept { m_active_count = 0; }

  /**
   * @brief Registers a data cell and provides a completely memory-stable Box
   * handle. Addresses returned here are guaranteed to remain unchanged even if
   * additional capacity buckets are allocated during the pass.
   */
  ftxui::Box& registerCell(const ModelIndex& index) {
    size_t chunk_idx = m_active_count / CHUNK_SIZE;
    size_t element_idx = m_active_count % CHUNK_SIZE;

    // Allocate an incremental stable bucket block only if we spill out of
    // current pools
    if (chunk_idx >= m_chunks.size()) {
      m_chunks.push_back(std::make_unique<ChunkType>());
    }

    auto& entry = (*m_chunks[chunk_idx])[element_idx];
    entry.index = index;

    m_active_count++;
    return entry.box;  // Safe reference return! No reallocations can break this
                       // address.
  }

  /**
   * @brief Evaluates terminal intersection bounds across stable memory chunks.
   */
  [[nodiscard]] std::optional<ModelIndex> findIndexAt(
      const int mouse_x,
      const int mouse_y) const noexcept {
    for (size_t i = 0; i < m_active_count; ++i) {
      const size_t chunk_idx = i / CHUNK_SIZE;
      const size_t element_idx = i % CHUNK_SIZE;

      const auto& entry = (*m_chunks[chunk_idx])[element_idx];
      if (entry.box.Contain(mouse_x, mouse_y)) {
        return entry.index;
      }
    }
    return std::nullopt;
  }
};
}  // namespace ftxmodel

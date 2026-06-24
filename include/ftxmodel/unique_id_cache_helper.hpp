#pragma once
#include <functional>
#include <unordered_map>
#include "model_cache_traits.hpp"

namespace ftxmodel {

/**
 * @class UniqueIdCacheHelper
 * @brief An O(1) lazy-cached identity helper restricted and validated using
 * C++20 Concepts.
 */
template <typename Model, typename InternalHandle, typename Traits>
  requires ModelCachePolicy<Traits, Model, InternalHandle>
class UniqueIdCacheHelper {
 public:
  UniqueIdCacheHelper() = default;
  ~UniqueIdCacheHelper() = default;

  UniqueIdCacheHelper(const UniqueIdCacheHelper&) = delete;
  UniqueIdCacheHelper& operator=(const UniqueIdCacheHelper&) = delete;
  UniqueIdCacheHelper(UniqueIdCacheHelper&&) noexcept = default;
  UniqueIdCacheHelper& operator=(UniqueIdCacheHelper&&) noexcept = default;

  void invalidate() noexcept { m_dirty = true; }

  void clear() noexcept {
    m_idCache.clear();
    m_dirty = false;
  }

  void updateKey(const UniqueNodeId& oldId,
                 const UniqueNodeId& newId,
                 const InternalHandle& handle) {
    if (m_dirty) {
      return;
    }
    m_idCache.erase(oldId);
    m_idCache[newId] = handle;
  }

  ModelIndex findIndexById(
      const Model& model,
      const UniqueNodeId& targetId,
      std::function<void(UniqueIdCacheHelper&)> fallback) const {
    if (m_dirty) {
      auto* mutableThis = const_cast<UniqueIdCacheHelper*>(this);
      mutableThis->m_idCache.clear();
      fallback(*mutableThis);
      mutableThis->m_dirty = false;
    }

    auto it = m_idCache.find(targetId);
    if (it == m_idCache.end()) {
      return {};
    }

    return Traits::createIndex(model, it->second, 0);
  }

  void insertDirect(const UniqueNodeId& id, const InternalHandle& handle) {
    m_idCache[id] = handle;
  }

  [[nodiscard]] bool isDirty() const noexcept { return m_dirty; }

 private:
  bool m_dirty = true;
  std::unordered_map<UniqueNodeId,
                     InternalHandle,
                     UniqueNodeIdHash,
                     UniqueNodeIdEqual>
      m_idCache;
};

}  // namespace ftxmodel

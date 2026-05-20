#pragma once

#include <map>
#include <unordered_map>
#include <vector>

#if defined(__has_include)
#if __has_include(<memory_resource>)
#include <memory_resource>
#define CMF_HAS_STD_PMR 1
#else
#define CMF_HAS_STD_PMR 0
#endif
#else
#define CMF_HAS_STD_PMR 0
#endif

namespace cmf
{

#if CMF_HAS_STD_PMR

using MemoryResource = std::pmr::memory_resource;

template <typename K, typename V, typename Compare = std::less<K>>
using PmrMap = std::pmr::map<K, V, Compare>;

template <typename K, typename V, typename Hash = std::hash<K>,
          typename Eq = std::equal_to<K>>
using PmrUnorderedMap = std::pmr::unordered_map<K, V, Hash, Eq>;

template <typename T>
using PmrVector = std::pmr::vector<T>;

inline MemoryResource* default_memory_resource() noexcept
{
    return std::pmr::get_default_resource();
}

#else

struct MemoryResource
{
};

template <typename K, typename V, typename Compare = std::less<K>>
using PmrMap = std::map<K, V, Compare>;

template <typename K, typename V, typename Hash = std::hash<K>,
          typename Eq = std::equal_to<K>>
using PmrUnorderedMap = std::unordered_map<K, V, Hash, Eq>;

template <typename T>
using PmrVector = std::vector<T>;

inline MemoryResource* default_memory_resource() noexcept { return nullptr; }

#endif

} // namespace cmf

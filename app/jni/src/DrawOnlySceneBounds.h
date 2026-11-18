#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <set>
#include <type_traits>
#include <utility>
#include <vector>

namespace draw_only_scene_bounds {
// Resource budgets, not claims about game-format maxima. Physics accepts larger
// meshes than the separate <1M wireframe display rule, including 1024x1024
// terrain. A bad count is rejected rather than truncated into a partial mesh.
constexpr std::size_t MaxActors = 200000;
constexpr std::size_t MaxScenes = 128;
constexpr std::size_t MaxReadBytes = 64u * 1024u * 1024u;
constexpr std::size_t MaxVertices = 4u * 1024u * 1024u;
constexpr std::size_t MaxIndices = 12u * 1024u * 1024u;
constexpr std::size_t MaxSamples = 4u * 1024u * 1024u;

// A remote object is pending until its whole mesh was built, or a successful
// header read explicitly excluded it. Failed reads never confirm cache state.
template<class Key>
struct LoadReport {
    std::set<Key> pending, built, filtered;
    template<class Range> void Begin(const Range& keys) {
        pending = std::set<Key>(keys.begin(), keys.end());
        built.clear(); filtered.clear();
    }
    void Built(const Key& key) { pending.erase(key); filtered.erase(key); built.insert(key); }
    void Filtered(const Key& key) { pending.erase(key); built.erase(key); filtered.insert(key); }
    void Retry(const Key& key) { built.erase(key); filtered.erase(key); pending.insert(key); }
    bool Complete() const { return pending.empty(); }
    template<class PointerMap, class PointerSet>
    void RetryUnbuiltAliases(const PointerMap& candidates, const PointerSet& loaded,
                             const PointerSet& newlyBuilt) {
        for (const auto& entry : candidates)
            if (filtered.count(entry.first) && !loaded.count(entry.second) && !newlyBuilt.count(entry.second))
                Retry(entry.first);
    }
    template<class BuiltFn, class FilteredFn>
    void Commit(BuiltFn onBuilt, FilteredFn onFiltered) const {
        for (const auto& key : built) onBuilt(key);
        for (const auto& key : filtered) onFiltered(key);
    }
    template<class Timestamp>
    bool AdvanceTimestamp(Timestamp& previous, Timestamp current) const {
        if (!Complete()) return false;
        previous = current;
        return true;
    }
};

inline bool Finite(float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return (bits & 0x7f800000u) != 0x7f800000u;
}
template<class Point>
bool ProjectedTriangleFinite(const Point& a, const Point& b, const Point& c) {
    return Finite(a.X) && Finite(a.Y) && Finite(b.X) && Finite(b.Y) && Finite(c.X) && Finite(c.Y);
}

template<class Count>
bool CountBytes(Count count, std::size_t elementBytes, std::size_t limit, std::size_t& bytes) {
    static_assert(std::is_integral<Count>::value, "integer remote count required");
    bytes = 0;
    if constexpr (std::is_signed<Count>::value) if (count < 0) return false;
    if (!elementBytes || static_cast<std::uintmax_t>(count) > limit) return false;
    const auto size = static_cast<std::size_t>(count);
    if (size > std::numeric_limits<std::size_t>::max() / elementBytes) return false;
    bytes = size * elementBytes;
    return true;
}
inline bool AddressRange(std::uintptr_t address, std::size_t bytes) {
    return bytes == 0 || (address >= 0x1000 && bytes - 1 <= std::numeric_limits<std::uintptr_t>::max() - address);
}
inline bool AddressOffset(std::uintptr_t base, std::size_t offset, std::uintptr_t& result) {
    result = 0;
    if (base < 0x1000 || offset > std::numeric_limits<std::uintptr_t>::max() - base) return false;
    result = base + offset;
    return true;
}
inline bool ArrayValid(std::uintptr_t base, std::int32_t count, std::int32_t capacity,
                       std::size_t limit, std::size_t elementBytes) {
    std::size_t bytes = 0, capacityBytes = 0;
    return count <= capacity && CountBytes(count, elementBytes, limit, bytes) &&
        CountBytes(capacity, elementBytes, limit, capacityBytes) && AddressRange(base, bytes);
}

template<class T, class Count, class Reader>
bool ReadVector(std::uintptr_t address, Count count, std::size_t limit,
                std::vector<T>& output, Reader&& read) {
    output.clear();
    std::size_t bytes = 0;
    if (!CountBytes(count, sizeof(T), limit, bytes) || bytes > MaxReadBytes ||
        !AddressRange(address, bytes)) return false;
    if (bytes == 0) return true;
    std::vector<T> next(static_cast<std::size_t>(count));
    if (!read(address, next.data(), bytes)) return false;
    output = std::move(next);
    return true;
}
inline bool TriangleCounts(std::uint32_t vertices, std::uint32_t triangles, std::size_t& indices) {
    indices = 0;
    if (vertices < 3 || vertices > MaxVertices || triangles == 0 || triangles > MaxIndices / 3) return false;
    indices = static_cast<std::size_t>(triangles) * 3;
    return true;
}

struct HeightfieldLayout {
    std::size_t rows = 0, columns = 0, samples = 0, vertices = 0, indices = 0;
};
inline bool Heightfield(std::uint32_t rows, std::uint32_t columns, std::size_t availableSamples,
                        HeightfieldLayout& output) {
    output = {};
    if (rows < 2 || columns < 2 || availableSamples > MaxSamples) return false;
    std::size_t samples = 0, cells = 0;
    if (!CountBytes(rows, columns, MaxSamples, samples) || samples > availableSamples ||
        !CountBytes(rows - 1, static_cast<std::size_t>(columns - 1), MaxSamples, cells) ||
        cells > MaxVertices / 4 || cells > MaxIndices / 6) return false;
    output = {rows, columns, samples, cells * 4, cells * 6};
    return true;
}

inline bool PolygonRange(std::size_t offset, std::size_t count, std::size_t indices) {
    return count >= 3 && offset <= indices && count <= indices - offset;
}
template<class Polygons>
bool ConvexIndexCount(const Polygons& polygons, std::size_t& count) {
    count = 0;
    if (polygons.empty() || polygons.size() > 255) return false;
    for (const auto& polygon : polygons) {
        const std::size_t offset = polygon.mVRef8, vertices = polygon.mNbVerts;
        if (!PolygonRange(offset, vertices, MaxIndices)) { count = 0; return false; }
        count = std::max(count, offset + vertices);
    }
    return true;
}
template<class Vertices, class Indices>
bool MeshValid(const Vertices& vertices, const Indices& indices) {
    if (vertices.size() < 3 || vertices.size() > MaxVertices || indices.empty() ||
        indices.size() > MaxIndices || indices.size() % 3 != 0) return false;
    for (const auto& vertex : vertices)
        if (!Finite(vertex.x) || !Finite(vertex.y) || !Finite(vertex.z)) return false;
    for (const auto index : indices) if (index >= vertices.size()) return false;
    return true;
}
} // namespace draw_only_scene_bounds

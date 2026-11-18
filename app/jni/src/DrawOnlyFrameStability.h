#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>
#include <unordered_set>
#include <vector>

namespace draw_only_stability {
constexpr double GraceSeconds = .120;

// Read into a temporary: failed/partial reads must not corrupt the last sample.
template<class T, class Reader>
bool ReadValue(Reader&& read, std::uintptr_t address, T& output) {
    static_assert(std::is_trivially_copyable<T>::value,"remote reads require plain data");
    if (address < 0x1000 || sizeof(T)-1 > std::numeric_limits<std::uintptr_t>::max()-address) return false;
    for (int attempt=0; attempt<2; ++attempt) {
        T next{};
        if (read(address, &next, sizeof(next))) { std::memcpy(&output,&next,sizeof(T)); return true; }
    }
    return false;
}

template<class T>
struct Recent {
    T value{};
    double sampledAt=0;
    bool available=false;
    bool Fresh(double now) const {
        return available && now>=sampledAt && now-sampledAt<=GraceSeconds;
    }
    void Store(const T& next, double now) { value=next; sampledAt=now; available=true; }
    void Clear() { value=T{}; sampledAt=0; available=false; }
    bool Restore(T& output, double now) const {
        if (!Fresh(now)) return false;
        output=value; return true;
    }
};

struct ArrayHeader {
    std::uintptr_t data=0;
    std::int32_t count=0;
    bool operator==(const ArrayHeader& other) const { return data==other.data && count==other.count; }
    bool Valid() const { return count>=0 && count<=200000 && (count==0 || data>=0x1000); }
};
template<class Reader>
bool ReadHeader(Reader&& read, std::uintptr_t level, ArrayHeader& output) {
    // Only the two existing fields at ULevel+0xA0/+0xA8 are used.
    unsigned char bytes[12]{};
    if (level<0x1000 || level>std::numeric_limits<std::uintptr_t>::max()-0xAB) return false;
    for (int attempt=0; attempt<2; ++attempt) {
        if (!read(level+0xA0,bytes,sizeof(bytes))) continue;
        std::memcpy(&output.data,bytes,8); std::memcpy(&output.count,bytes+8,4);
        return true;
    }
    return false;
}

// A complete, deduplicated actor list is published only after its header is
// rechecked. A successful empty list or a changed identity never revives cache.
class ActorList {
    std::uintptr_t level_=0;
    ArrayHeader header_{};
    double sampledAt_=0;
    bool available_=false;
    std::vector<std::uintptr_t> actors_;
public:
    bool reused=false;
    void Clear() { level_=0; header_={}; sampledAt_=0; available_=false; actors_.clear(); reused=false; }
    const std::vector<std::uintptr_t>& Values() const { return actors_; }
    const ArrayHeader& Header() const { return header_; }
    template<class Reader>
    bool Refresh(Reader&& read, std::uintptr_t level, double now) {
        reused=false;
        if (level<0x1000) { Clear(); return false; }
        if (level_!=level) { Clear(); level_=level; }
        bool mayReuse=true;
        for (int attempt=0; attempt<2; ++attempt) {
            ArrayHeader before{}, after{};
            if (!ReadHeader(read,level,before)) continue;
            if (!before.Valid()) { Clear(); level_=level; return false; }
            if (available_ && !(before==header_)) mayReuse=false;
            if (before.count==0) {
                actors_.clear(); header_=before; sampledAt_=now; available_=true;
                return true;
            }
            const auto bytes=static_cast<std::size_t>(before.count)*sizeof(std::uintptr_t);
            if (bytes-1>std::numeric_limits<std::uintptr_t>::max()-before.data) {
                Clear(); level_=level; return false;
            }
            std::vector<std::uintptr_t> candidate(static_cast<std::size_t>(before.count));
            bool complete=true;
            for (std::size_t first=0; first<candidate.size(); first+=256) {
                const std::size_t count=std::min<std::size_t>(256,candidate.size()-first);
                bool ok=false;
                for (int retry=0; retry<2 && !ok; ++retry)
                    ok=read(before.data+first*8,candidate.data()+first,count*8);
                if (!ok) { complete=false; break; }
            }
            if (!ReadHeader(read,level,after)) continue;
            if (!(before==after)) { mayReuse=false; continue; }
            if (!complete) continue;
            std::unordered_set<std::uintptr_t> seen;
            std::vector<std::uintptr_t> unique;
            unique.reserve(candidate.size()); seen.reserve(candidate.size());
            for (const auto actor:candidate)
                if (actor>=0x1000 && seen.insert(actor).second) unique.push_back(actor);
            actors_=std::move(unique); header_=before; sampledAt_=now; available_=true;
            return true;
        }
        reused=mayReuse && available_ && now>=sampledAt_ && now-sampledAt_<=GraceSeconds;
        if (!reused) { actors_.clear(); available_=false; }
        return reused;
    }
};

// Only confirmed characters get a cached position. Matching root and class
// identity prevents an actor address reused for a different object inheriting it.
template<class Point>
struct CharacterHistory {
    std::uintptr_t component=0;
    std::uint32_t classId=0;
    bool classKnown=false;
    Recent<Point> position;
    double identifiedAt=0;
    bool identified=false;
    bool Match(std::uintptr_t root, bool rootKnown, std::uint32_t id, bool idKnown) {
        const bool changed=(rootKnown && component && root!=component) ||
            (idKnown && classKnown && id!=classId);
        if (changed) *this=CharacterHistory{};
        if (rootKnown) component=root;
        if (idKnown) { classId=id; classKnown=true; }
        return !changed;
    }
    bool Confirmed(double now) const {
        return identified && now>=identifiedAt && now-identifiedAt<=GraceSeconds;
    }
    void Confirm(double now) { identified=true; identifiedAt=now; }
};
} // namespace draw_only_stability

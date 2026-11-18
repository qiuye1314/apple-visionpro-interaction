#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

// Shared by the actual memory reader and the host regression tests.
namespace draw_only_bones {
constexpr std::size_t JointCount = 15;
constexpr std::uintptr_t TransformStride = 48;
using JointMap = std::array<int, JointCount>;
enum class Rig { Standard, ClawsBoss, GuardBoss };

inline bool Finite(float value) {
    std::uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return (bits & 0x7f800000u) != 0x7f800000u;
}

template<class Vector>
inline bool ValidPosition(const Vector& value) {
    return Finite(value.X) && Finite(value.Y) && Finite(value.Z) &&
        std::fabs(value.X) <= 1.0e8f && std::fabs(value.Y) <= 1.0e8f &&
        std::fabs(value.Z) <= 1.0e8f;
}

template<class Transform>
inline bool ValidTransform(const Transform& value) {
    const auto& r = value.Rotation;
    const auto& s = value.Scale3D;
    if (!ValidPosition(value.Translation) ||
        !Finite(r.X) || !Finite(r.Y) || !Finite(r.Z) || !Finite(r.W) ||
        std::fabs(r.X) > 2 || std::fabs(r.Y) > 2 ||
        std::fabs(r.Z) > 2 || std::fabs(r.W) > 2 ||
        !Finite(s.X) || !Finite(s.Y) || !Finite(s.Z)) return false;
    const float norm = r.X*r.X + r.Y*r.Y + r.Z*r.Z + r.W*r.W;
    return norm >= .5f && norm <= 1.5f &&
        std::fabs(s.X) >= 1.0e-5f && std::fabs(s.X) <= 100.0f &&
        std::fabs(s.Y) >= 1.0e-5f && std::fabs(s.Y) <= 100.0f &&
        std::fabs(s.Z) >= 1.0e-5f && std::fabs(s.Z) <= 100.0f;
}

// Maps keep the source project's existing one-element-shifted indexing.
// -1 denotes a joint whose existing mapping is demonstrably ambiguous.
inline JointMap Mapping(int count, Rig rig = Rig::Standard) {
    if (rig == Rig::ClawsBoss)
        return {5,4,1,28,7,29,8,30,9,57,50,58,51,59,52};
    if (rig == Rig::GuardBoss)
        return {5,4,1,28,7,29,8,30,9,52,48,53,49,54,50};
    switch (count) {
    case 68: return {5,4,1,11,33,12,34,13,35,55,59,56,60,57,61};
    case 69: return {5,4,0,11,33,12,34,13,35,55,59,56,60,57,61};
    case 71: return {5,4,1,6,34,7,35,8,36,55,59,56,60,57,61};
    // The previous 73-bone wrists repeated the shoulder indices.
    case 73: return {5,3,0,13,35,14,36,-1,-1,61,57,62,58,63,59};
    case 63: return {5,4,0,8,30,9,31,10,32,50,54,51,55,52,56};
    case 64: return {5,4,0,7,29,8,30,9,31,51,57,52,58,53,59};
    case 38: return {5,4,0,7,17,9,19,11,21,27,34,28,35,30,36};
    case 30: return {5,4,0,9,14,10,15,11,16,18,22,19,23,20,24};
    case 26: return {5,4,0,13,6,14,7,15,8,21,18,22,19,23,20};
    case 27: return {5,4,0,7,10,8,11,9,12,14,18,15,19,16,20};
    case 46: return {5,4,0,21,8,22,9,23,10,35,40,36,41,38,44};
    case 42: return {16,15,0,20,31,21,32,22,33,3,8,4,9,5,10};
    case 76: return {5,4,0,7,27,8,28,9,29,48,52,49,53,50,54};
    case 61: return {5,4,0,6,27,7,28,8,29,48,52,49,53,50,54};
    // The old fallback reused left-thigh index 53 for the left knee.
    default: return {5,4,1,11,32,12,33,63,62,53,56,-1,57,54,58};
    }
}

inline bool ValidIndex(int index, int count) {
    // The caller passes array + 0x30: index zero is actual array element one.
    return count >= 15 && count <= 512 && index >= 0 && index < count - 1;
}

template<class Transform, class Reader>
inline bool ReadTransform(std::uintptr_t address, Transform& value, Reader&& read) {
    value = {};
    if (address == 0 || address > std::numeric_limits<std::uintptr_t>::max() - sizeof(Transform)) return false;
    // Retry a transient transport failure once. A successful but invalid payload
    // is authoritative for this sample and is never retried or reused as a pose.
    for (int attempt = 0; attempt < 2; ++attempt) {
        Transform candidate{};
        if (!read(address, &candidate, sizeof(Transform))) continue;
        if (!ValidTransform(candidate)) return false;
        value = candidate;
        return true;
    }
    return false;
}

template<class Transform, class Vector>
inline Vector ComponentPoint(const Transform& component, const Vector& local) {
    const auto& q = component.Rotation;
    const auto& s = component.Scale3D;
    const float x = local.X * s.X, y = local.Y * s.Y, z = local.Z * s.Z;
    // Quaternion rotation, with the same row-vector convention as the source FMatrix.
    Vector result{};
    result.X = x*(1-2*(q.Y*q.Y+q.Z*q.Z)) + y*2*(q.X*q.Y-q.W*q.Z) +
        z*2*(q.X*q.Z+q.W*q.Y) + component.Translation.X;
    result.Y = x*2*(q.X*q.Y+q.W*q.Z) + y*(1-2*(q.X*q.X+q.Z*q.Z)) +
        z*2*(q.Y*q.Z-q.W*q.X) + component.Translation.Y;
    result.Z = x*2*(q.X*q.Z-q.W*q.Y) + y*2*(q.Y*q.Z+q.W*q.X) +
        z*(1-2*(q.X*q.X+q.Y*q.Y)) + component.Translation.Z;
    return result;
}

template<class Transform, class Vector, class Reader>
inline std::size_t ReadSkeleton(std::uintptr_t componentAddress,
        std::uintptr_t shiftedBoneArray, int count, const JointMap& map,
        Vector (&points)[17], Reader&& read) {
    for (auto& point : points) point = {};
    if (!shiftedBoneArray || count < 15 || count > 512) return 0;
    Transform component{};
    if (!ReadTransform(componentAddress, component, read)) return 0;
    std::size_t validCount = 0;
    for (std::size_t i = 0; i < JointCount; ++i) {
        if (!ValidIndex(map[i], count)) continue;
        bool duplicate = false;
        for (std::size_t previous = 0; previous < i; ++previous)
            if (map[previous] == map[i]) duplicate = true;
        if (duplicate) continue;
        const auto offset = static_cast<std::uintptr_t>(map[i]) * TransformStride;
        if (shiftedBoneArray > std::numeric_limits<std::uintptr_t>::max() - offset) continue;
        Transform bone{};
        if (!ReadTransform(shiftedBoneArray + offset, bone, read)) continue;
        Vector point = ComponentPoint(component, bone.Translation);
        if (!ValidPosition(point) || (point.X == 0 && point.Y == 0 && point.Z == 0)) continue;
        if (i == 0) point.Z += 7.0f; // Existing head marker lift, only for a valid head.
        if (!ValidPosition(point)) continue;
        points[i] = point;
        ++validCount;
    }
    return validCount;
}
} // namespace draw_only_bones

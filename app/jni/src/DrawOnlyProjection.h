#pragma once
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace draw_only_projection {
inline bool Finite(float value) {
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return (bits & 0x7f800000u) != 0x7f800000u;
}

template<class Point>
inline bool ValidWorldPoint(const Point& point, bool missingZero = true) {
    return Finite(point.X) && Finite(point.Y) && Finite(point.Z) &&
        std::fabs(point.X) <= 1.0e8f && std::fabs(point.Y) <= 1.0e8f &&
        std::fabs(point.Z) <= 1.0e8f &&
        (!missingZero || point.X != 0 || point.Y != 0 || point.Z != 0);
}

template<class Point, class Trace>
inline bool ResolveVisibility(const Point (&joints)[17], const Point& root,
                              bool enabled, bool (&visible)[15], Trace&& trace) {
    bool torsoAvailable = false;
    for (int i = 0; i < 15; ++i) {
        const bool available = ValidWorldPoint(joints[i]);
        if (i < 3) torsoAvailable = torsoAvailable || available;
        visible[i] = !enabled || (available && trace(joints[i]));
    }
    if (enabled && !torsoAvailable)
        return ValidWorldPoint(root, false) && trace(root);
    return visible[0] || visible[1] || visible[2];
}

// Missing joints keep the (0,0) sentinel. Never reflect points behind the
// camera or divide by the near plane; partially offscreen limbs remain valid.
template<class WorldPoint, class ScreenPoint>
inline bool Project(const float* matrix, const WorldPoint& point,
                    float centerX, float centerY, ScreenPoint& screen) {
    screen = {};
    if (!matrix || !ValidWorldPoint(point) || !Finite(centerX) || !Finite(centerY) ||
        centerX <= 0 || centerY <= 0) return false;
    for (int i = 0; i < 16; ++i) if (!Finite(matrix[i])) return false;
    const float depth = matrix[3]*point.X + matrix[7]*point.Y + matrix[11]*point.Z + matrix[15];
    if (!Finite(depth) || depth <= .01f) return false;
    const float x = centerX + (matrix[0]*point.X + matrix[4]*point.Y + matrix[8]*point.Z + matrix[12])/depth*centerX;
    const float y = centerY - (matrix[1]*point.X + matrix[5]*point.Y + matrix[9]*point.Z + matrix[13])/depth*centerY;
    if (!Finite(x) || !Finite(y) || std::fabs(x) >= 100000.f || std::fabs(y) >= 100000.f) return false;
    screen.X = x;
    screen.Y = y;
    return true;
}

// Output uses the existing actor-root convention: left, bottom, width, height.
// Unlike a projected world-Z segment, a finite volume retains area when the
// camera looks down or rolls upside down. Failure only invalidates this fallback.
template<class WorldPoint, class ScreenBounds>
inline bool ProjectActorBounds(const float* matrix, const WorldPoint& root,
                               float centerX, float centerY, ScreenBounds& output) {
    output = {};
    if (!matrix || !ValidWorldPoint(root, false) || !Finite(centerX) || !Finite(centerY) ||
        centerX < 16.f || centerY < 16.f || centerX > 16384.f || centerY > 16384.f) return false;
    for (int i = 0; i < 16; ++i) if (!Finite(matrix[i])) return false;
    constexpr float nearDepth = .01f;
    const float rootDepth = matrix[3]*root.X + matrix[7]*root.Y + matrix[11]*root.Z + matrix[15];
    if (!Finite(rootDepth) || rootDepth <= nearDepth) return false;
    struct ClipPoint { float x, y, depth; };
    ClipPoint corners[8]{};
    for (int i = 0; i < 8; ++i) {
        const float x = root.X + ((i & 1) ? 40.f : -40.f);
        const float y = root.Y + ((i & 2) ? 40.f : -40.f);
        const float z = root.Z + ((i & 4) ? 205.f : -5.f);
        corners[i] = {matrix[0]*x + matrix[4]*y + matrix[8]*z + matrix[12],
                      matrix[1]*x + matrix[5]*y + matrix[9]*z + matrix[13],
                      matrix[3]*x + matrix[7]*y + matrix[11]*z + matrix[15]};
        if (!Finite(corners[i].x) || !Finite(corners[i].y) || !Finite(corners[i].depth)) return false;
    }
    float left = FLT_MAX, top = FLT_MAX, right = -FLT_MAX, bottom = -FLT_MAX;
    int projected = 0;
    const auto include = [&](const ClipPoint& point) {
        const float x = centerX + point.x / point.depth * centerX;
        const float y = centerY - point.y / point.depth * centerY;
        if (!Finite(x) || !Finite(y) || std::fabs(x) >= 100000.f || std::fabs(y) >= 100000.f) return false;
        left = std::min(left, x); right = std::max(right, x);
        top = std::min(top, y); bottom = std::max(bottom, y); ++projected;
        return true;
    };
    for (const auto& corner : corners)
        if (corner.depth >= nearDepth && !include(corner)) return false;
    // Add actual near-plane intersections; never divide a rear-facing corner or
    // take an absolute depth, which would mirror a hidden actor onto the screen.
    for (int i = 0; i < 8; ++i) for (int bit = 1; bit <= 4; bit <<= 1) {
        if (i & bit) continue;
        const auto& a = corners[i]; const auto& b = corners[i | bit];
        if ((a.depth >= nearDepth) == (b.depth >= nearDepth)) continue;
        const float t = (nearDepth - a.depth) / (b.depth - a.depth);
        if (!Finite(t) || !include({a.x + (b.x-a.x)*t, a.y + (b.y-a.y)*t, nearDepth})) return false;
    }
    if (!projected) return false;
    const float width = right-left, height = bottom-top, viewWidth = centerX*2.f, viewHeight = centerY*2.f;
    // Near-plane intersections can be finite but cover many screens. Such a box
    // must not replace usable joints with a giant fallback marker.
    if (!Finite(width) || !Finite(height) || width <= 0.f || height <= 0.f ||
        width > viewWidth*1.5f || height > viewHeight*1.5f ||
        right <= 0.f || left >= viewWidth || bottom <= 0.f || top >= viewHeight) return false;
    const float midX = (left+right)*.5f, midY = (top+bottom)*.5f;
    const float safeWidth = std::max(3.f, width), safeHeight = std::max(8.f, height);
    output.X = midX-safeWidth*.5f; output.Y = midY+safeHeight*.5f;
    output.Z = safeWidth; output.W = safeHeight;
    return true;
}
} // namespace draw_only_projection

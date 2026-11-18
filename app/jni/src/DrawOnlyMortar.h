#pragma once
#include "DrawOnlyProjection.h"
#include "版本数据_1_39.h"
#include <cmath>
#include <cstddef>
#include <cstdint>

// The user-supplied chain is BSS-relative, not relative to the ELF image base.
// Only the terminal three floats are coordinates; each preceding step is a pointer.
namespace draw_only_mortar {
struct Point { float X = 0, Y = 0, Z = 0; };
struct Screen { float X = 0, Y = 0; };
struct Sample {
    Point world{};
    Screen screen{};
    float distanceMeters = 0;
    bool valid = false;
};
constexpr std::uintptr_t AddressMask = 0x0000ffffffffffffULL;
inline std::uintptr_t Canonical(std::uintptr_t address) { return address & AddressMask; }
inline bool PointerValid(std::uintptr_t address) {
    return address >= 0x1000 && address <= AddressMask - 7 && (address & 7) == 0;
}
inline bool AddressAt(std::uintptr_t base, std::uintptr_t offset, std::size_t size,
                      std::uintptr_t& result) {
    result = 0;
    base = Canonical(base);
    if (!PointerValid(base) || !size || size > AddressMask || offset > AddressMask - base ||
        size - 1 > AddressMask - (base + offset)) return false;
    result = base + offset;
    return true;
}
inline bool MatrixValid(const float* matrix) {
    if (!matrix) return false;
    for (int i = 0; i < 16; ++i) if (!draw_only_projection::Finite(matrix[i])) return false;
    return std::fabs(matrix[0]) + std::fabs(matrix[4]) + std::fabs(matrix[8]) > 1.0e-7f &&
           std::fabs(matrix[1]) + std::fabs(matrix[5]) + std::fabs(matrix[9]) > 1.0e-7f &&
           std::fabs(matrix[3]) + std::fabs(matrix[7]) + std::fabs(matrix[11]) +
               std::fabs(matrix[15]) > 1.0e-7f;
}
template<class Reader>
inline bool ReadLanding(std::uintptr_t bss, Point& result, Reader&& read) {
    result = {};
    std::uintptr_t current = bss;
    const std::uintptr_t offsets[] = {
        V139::迫击炮_BSS入口, V139::迫击炮_链1, V139::迫击炮_链2
    };
    for (const auto offset : offsets) {
        std::uintptr_t address = 0, next = 0;
        if (!AddressAt(current, offset, sizeof(next), address) ||
            !read(address, &next, sizeof(next))) return false;
        current = Canonical(next);
        if (!PointerValid(current)) return false;
    }
    std::uintptr_t address = 0;
    Point point{};
    static_assert(sizeof(Point) == sizeof(float) * 3, "Landing coordinates are three packed floats");
    if (!AddressAt(current, V139::迫击炮_落点, sizeof(point), address) ||
        !read(address, &point, sizeof(point)) || !draw_only_projection::ValidWorldPoint(point)) return false;
    result = point;
    return true;
}
template<class Origin, class Reader>
inline bool ReadFrame(bool enabled, bool sceneReady, std::uintptr_t bss,
                      const float* matrix, const Origin& origin, float centerX, float centerY,
                      Sample& result, Reader&& read) {
    result = {};
    if (!enabled || !sceneReady || !MatrixValid(matrix) ||
        !draw_only_projection::ValidWorldPoint(origin, false) ||
        !draw_only_projection::Finite(centerX) || !draw_only_projection::Finite(centerY) ||
        centerX < 16 || centerY < 16) return false;
    Sample sample{};
    if (!ReadLanding(bss, sample.world, read) ||
        !draw_only_projection::Project(matrix, sample.world, centerX, centerY, sample.screen) ||
        sample.screen.X < 0 || sample.screen.X > centerX * 2 ||
        sample.screen.Y < 0 || sample.screen.Y > centerY * 2) return false;
    const double dx = static_cast<double>(sample.world.X) - origin.X;
    const double dy = static_cast<double>(sample.world.Y) - origin.Y;
    const double dz = static_cast<double>(sample.world.Z) - origin.Z;
    sample.distanceMeters = static_cast<float>(std::sqrt(dx * dx + dy * dy + dz * dz) / 100.0);
    if (!draw_only_projection::Finite(sample.distanceMeters)) return false;
    sample.valid = true;
    result = sample;
    return true;
}
} // namespace draw_only_mortar

#include "DrawOnlySceneBounds.h"
#include <cassert>
#include <cstdio>
#include <limits>
#include <map>

using namespace draw_only_scene_bounds;
static void Pass(const char* message) { std::printf("PASS %s\n", message); }
struct Point { float x, y, z; };
struct Screen { float X, Y; };
struct Polygon { std::uint16_t mVRef8; std::uint8_t mNbVerts; };
static float FloatBits(std::uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

int main() {
    std::size_t bytes = 123;
    assert(!CountBytes(-1, sizeof(std::uint64_t), MaxActors, bytes) && bytes == 0);
    assert(!CountBytes(200001, sizeof(std::uint64_t), MaxActors, bytes));
    assert(!CountBytes(std::numeric_limits<std::size_t>::max(), 2,
                       std::numeric_limits<std::size_t>::max(), bytes));
    assert(CountBytes(200000, sizeof(std::uint64_t), MaxActors, bytes) && bytes == 1600000);
    Pass("negative excessive and multiplication-overflow counts are rejected before allocation");

    assert(ArrayValid(0, 0, 0, MaxActors, 8));
    assert(ArrayValid(0x1000, 20, 32, MaxActors, 8));
    assert(!ArrayValid(0x1000, -1, 32, MaxActors, 8));
    assert(!ArrayValid(0x1000, 33, 32, MaxActors, 8));
    assert(!ArrayValid(0x1000, 20, -1, MaxActors, 8));
    assert(!ArrayValid(0x1000, 20, 200001, MaxActors, 8));
    assert(!ArrayValid(0x1000, 129, 129, MaxScenes, 8));
    assert(!ArrayValid(std::numeric_limits<std::uintptr_t>::max() - 3, 2, 2, MaxActors, 8));
    std::uintptr_t address = 5;
    assert(!AddressOffset(std::numeric_limits<std::uintptr_t>::max(), 1, address) && address == 0);
    Pass("scene and actor array headers require consistent bounded counts and nonoverflowing address ranges");

    int reads = 0;
    std::vector<std::uint32_t> output{7,8,9};
    auto read = [&](std::uintptr_t source, void* target, std::size_t length) {
        ++reads;
        assert(source == 0x1000 && length == 12);
        const std::uint32_t expected[] = {1,2,3};
        std::memcpy(target, expected, length);
        return true;
    };
    assert(!ReadVector(0x1000, -1, MaxIndices, output, read) && output.empty() && reads == 0);
    assert(!ReadVector(0x1000, MaxIndices + 1, MaxIndices, output, read) && output.empty() && reads == 0);
    assert(!ReadVector(std::numeric_limits<std::uintptr_t>::max() - 1, 3, MaxIndices, output, read));
    assert(!ReadVector(0x1000, MaxReadBytes, MaxReadBytes, output, read) && reads == 0);
    assert(ReadVector(0x1000, 3, MaxIndices, output, read) && output == std::vector<std::uint32_t>({1,2,3}));
    assert(ReadVector(0, 0, MaxIndices, output, read) && output.empty() && reads == 1);
    Pass("invalid remote vectors issue no read while valid vectors preserve every element and empty arrays stay empty");

    output = {9,9,9};
    assert(!ReadVector(0x1000, 3, MaxIndices, output,
        [&](std::uintptr_t, void* target, std::size_t) {
            ++reads; *static_cast<std::uint32_t*>(target) = 123; return false;
        }));
    assert(output.empty() && reads == 2);
    Pass("failed partially-written vector reads cannot publish a truncated or zero-filled mesh buffer");

    std::size_t indices = 1;
    assert(TriangleCounts(3, 1, indices) && indices == 3);
    assert(!TriangleCounts(2, 1, indices));
    assert(!TriangleCounts(3, 0, indices));
    assert(!TriangleCounts(3, std::numeric_limits<std::uint32_t>::max(), indices) && indices == 0);
    assert(!TriangleCounts(static_cast<std::uint32_t>(MaxVertices + 1), 1, indices));
    assert(TriangleCounts(static_cast<std::uint32_t>(MaxVertices),
                          static_cast<std::uint32_t>(MaxIndices / 3), indices) && indices == MaxIndices);
    Pass("triangle multiplication is validated before expansion and physics meshes retain their separate larger budget");

    HeightfieldLayout layout;
    assert(Heightfield(512, 512, 512u * 512u, layout));
    assert(layout.vertices == 511u * 511u * 4u && layout.indices == 511u * 511u * 6u);
    assert(Heightfield(1024, 1024, 1024u * 1024u, layout));
    assert(layout.vertices == 1023u * 1023u * 4u && layout.indices == 1023u * 1023u * 6u);
    Pass("valid 512-square and 1024-square terrain remains available to collision queries");

    assert(!Heightfield(0, 2, 4, layout));
    assert(!Heightfield(1, 2, 4, layout));
    assert(!Heightfield(2, 2, 3, layout));
    assert(!Heightfield(1024, 1024, 1024u * 1024u - 1, layout));
    assert(!Heightfield(std::numeric_limits<std::uint32_t>::max(),
                        std::numeric_limits<std::uint32_t>::max(), MaxSamples, layout));
    assert(!Heightfield(2048, 2048, 2048u * 2048u, layout));
    assert(!Heightfield(2, 2, MaxSamples + 1, layout));
    assert(layout.vertices == 0 && layout.indices == 0);
    Pass("short samples overflowing dimensions and oversized generated terrain are rejected as whole samples");

    assert(Heightfield(4, 5, 20, layout));
    for (std::size_t row = 0; row + 1 < layout.rows; ++row)
        for (std::size_t col = 0; col + 1 < layout.columns; ++col) {
            const auto lastSample = (row + 1) * layout.columns + col + 1;
            const auto lastVertex = (row * (layout.columns - 1) + col) * 4 + 3;
            assert(lastSample < layout.samples && lastVertex < layout.vertices);
        }
    Pass("validated terrain cell arithmetic keeps the final sample and vertex within their buffers");

    std::vector<Polygon> polygons{{0,3}, {4,3}};
    assert(ConvexIndexCount(polygons, indices) && indices == 7);
    assert(PolygonRange(4, 3, 7) && !PolygonRange(4, 3, 6));
    assert(!PolygonRange(std::numeric_limits<std::size_t>::max(), 3, 7));
    polygons.push_back({7,2});
    assert(!ConvexIndexCount(polygons, indices) && indices == 0);
    Pass("convex references include offsets and padding and malformed polygons reject the whole sample");

    std::vector<Point> vertices{{0,0,0},{1,0,0},{0,1,0}};
    std::vector<std::uint32_t> validIndices{0,1,2};
    assert(MeshValid(vertices, validIndices));
    validIndices[2] = 3; assert(!MeshValid(vertices, validIndices));
    validIndices = {0,1}; assert(!MeshValid(vertices, validIndices));
    validIndices = {0,1,2}; vertices[1].z = FloatBits(0x7f800000u);
    assert(!MeshValid(vertices, validIndices));
    vertices[1].z = FloatBits(0x7fc00000u);
    assert(!MeshValid(vertices, validIndices));
    Pass("out-of-range indices incomplete triangles and nonfinite mesh vertices never reach scene construction");

    Screen a{10,20}, b{30,40}, c{50,60};
    assert(ProjectedTriangleFinite(a,b,c));
    b.X = FloatBits(0x7f800000u); assert(!ProjectedTriangleFinite(a,b,c));
    b.X = FloatBits(0xff800000u); assert(!ProjectedTriangleFinite(a,b,c));
    b.X = FloatBits(0x7fc00000u); assert(!ProjectedTriangleFinite(a,b,c));
    b.X = 30; c.Y = FloatBits(0x7f800000u); assert(!ProjectedTriangleFinite(a,b,c));
    Pass("a finite onscreen corner cannot allow another behind-camera infinite or NaN corner into ImGui");

    {
        LoadReport<int> report;
        std::map<int, int> committedRevision, committedMesh, meshReads;
        const std::map<int, int> wantedRevision{{1,1}, {2,1}};
        const std::vector<int> keys{1,2};
        std::uint32_t timestamp = 10;
        const auto attempt = [&](const std::map<int,int>& revisions, int failingKey) {
            report.Begin(keys);
            std::map<int,int> candidates;
            for (const int key : keys) {
                if (committedRevision.count(key) && committedRevision.at(key) == revisions.at(key)) {
                    report.Filtered(key);
                    continue;
                }
                ++meshReads[key];
                std::vector<std::uint32_t> indices;
                const bool readSucceeded = ReadVector(0x1000, 3, MaxIndices, indices,
                    [&](std::uintptr_t, void* destination, std::size_t length) {
                        const std::uint32_t data[]{0,1,2};
                        std::memcpy(destination, data, length);
                        return key != failingKey;
                    });
                const std::vector<Point> points{{0,0,0}, {1,0,0}, {0,1,0}};
                if (!readSucceeded || !MeshValid(points, indices)) continue;
                candidates[key] = revisions.at(key) * 100;
                report.Built(key);
            }
            report.Commit([&](int key) {
                committedMesh[key] = candidates.at(key);
                committedRevision[key] = revisions.at(key);
            }, [](int) {});
            report.AdvanceTimestamp(timestamp, std::uint32_t{11});
        };
        attempt(wantedRevision, 2);
        assert(timestamp == 10 && committedMesh.at(1) == 100 && !committedMesh.count(2));
        assert(report.pending == std::set<int>{2});
        attempt(wantedRevision, -1);
        assert(timestamp == 11 && committedMesh.at(2) == 100 && report.Complete());
        assert(meshReads.at(1) == 1 && meshReads.at(2) == 2);
        Pass("failed mesh retries at the same timestamp while its successfully loaded peer avoids rebuilding");

        timestamp = 10;
        const std::map<int,int> changedRevision{{1,2}, {2,1}};
        attempt(changedRevision, 1);
        assert(timestamp == 10 && committedMesh.at(1) == 100 && committedRevision.at(1) == 1);
        attempt(changedRevision, -1);
        assert(timestamp == 11 && committedMesh.at(1) == 200 && committedRevision.at(1) == 2);
        assert(meshReads.at(1) == 3 && meshReads.at(2) == 2);
        Pass("failed replacement retains the previous valid mesh and revision until a later complete read succeeds");
    }
    {
        LoadReport<int> report;
        const std::vector<int> shapes{7,8};
        const std::map<int,std::uint64_t> aliases{{7,0x8000}, {8,0x8000}};
        std::set<std::uint64_t> loaded, built;
        std::set<int> confirmed;
        std::uint32_t timestamp = 20;
        report.Begin(shapes);
        report.Filtered(8); // Same-pointer deduplication after shape 7 failed.
        report.RetryUnbuiltAliases(aliases, loaded, built);
        report.Commit([&](int key) { confirmed.insert(key); }, [&](int key) { confirmed.insert(key); });
        assert(!report.AdvanceTimestamp(timestamp, std::uint32_t{21}));
        assert(timestamp == 20 && loaded.empty() && confirmed.empty() && report.pending.size() == 2);
        report.Begin(shapes);
        report.Built(7);
        report.Filtered(8);
        built.insert(0x8000);
        report.RetryUnbuiltAliases(aliases, loaded, built);
        report.Commit([&](int key) { confirmed.insert(key); }, [&](int key) { confirmed.insert(key); });
        loaded.insert(built.begin(), built.end());
        assert(report.AdvanceTimestamp(timestamp, std::uint32_t{21}));
        assert(timestamp == 21 && loaded.size() == 1 && confirmed.size() == 2);
        Pass("heightfield aliases never confirm a failed shared pointer and all recover after one successful copy");
    }
}

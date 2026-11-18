#include "imgui_internal.h"
#include "SpatialChrome.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <set>
#include <vector>

using spatial_chrome::Panel;
using spatial_chrome::Shell;

static float FloatBits(std::uint32_t bits) {
    float value; std::memcpy(&value, &bits, sizeof(value)); return value;
}

static void Reset(ImDrawList* list) {
    list->_ResetForNewFrame();
    list->PushClipRect({-10000.f, -10000.f}, {10000.f, 10000.f}, false);
}

static int FillVertexCount(ImDrawList* list, ImVec2 p, ImVec2 size, float radius) {
    Reset(list);
    list->AddRectFilled(p, {p.x + size.x, p.y + size.y}, IM_COL32_WHITE, radius);
    return list->VtxBuffer.Size;
}

static int ShadowVertexCount(ImDrawList* list, ImVec2 p, ImVec2 size, float radius, float scale) {
    Reset(list);
    const float s = spatial_chrome::Clamp(scale, .1f, 4.f);
    for (int i = 3; i >= 1; --i) {
        const float spread = std::max(0.f, 5.f * s - 1.f) * i / 3.f;
        list->AddRect({p.x - spread, p.y - spread},
            {p.x + size.x + spread, p.y + size.y + spread},
            IM_COL32_WHITE, radius + spread, 0, .9f * s);
    }
    return list->VtxBuffer.Size;
}

static float MaxAlpha(const ImDrawList* list, int first, int end) {
    float result = 0.f;
    for (int i = first; i < end; ++i)
        result = std::max(result, ((list->VtxBuffer[i].col >> IM_COL32_A_SHIFT) & 255) / 255.f);
    return result;
}

static void AssertFinite(const ImDrawList* list, int first) {
    for (int i = first; i < list->VtxBuffer.Size; ++i) {
        const auto& v = list->VtxBuffer[i];
        assert(spatial_chrome::Finite(v.pos.x) && spatial_chrome::Finite(v.pos.y));
        assert(spatial_chrome::Finite(v.uv.x) && spatial_chrome::Finite(v.uv.y));
    }
}

static void AssertBounds(const ImDrawList* list, int first, ImVec2 p, ImVec2 size, float margin) {
    for (int i = first; i < list->VtxBuffer.Size; ++i) {
        const ImVec2 v = list->VtxBuffer[i].pos;
        if (v.x < p.x - margin - .01f || v.y < p.y - margin - .01f ||
            v.x > p.x + size.x + margin + .01f || v.y > p.y + size.y + margin + .01f)
            std::fprintf(stderr, "bounds: v=(%g,%g), p=(%g,%g), size=(%g,%g), margin=%g\n",
                v.x, v.y, p.x, p.y, size.x, size.y, margin);
        assert(v.x >= p.x - margin - .01f && v.y >= p.y - margin - .01f);
        assert(v.x <= p.x + size.x + margin + .01f && v.y <= p.y + size.y + margin + .01f);
    }
}

static std::vector<ImDrawVert> Snapshot(const ImDrawList* list) {
    return {list->VtxBuffer.begin(), list->VtxBuffer.end()};
}

static bool Same(const std::vector<ImDrawVert>& a, const std::vector<ImDrawVert>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (a[i].pos.x != b[i].pos.x || a[i].pos.y != b[i].pos.y ||
            a[i].uv.x != b[i].uv.x || a[i].uv.y != b[i].uv.y || a[i].col != b[i].col) return false;
    return true;
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = {1280.f, 800.f};
    io.DeltaTime = 1.f / 60.f;
    io.Fonts->AddFontDefault();
    unsigned char* pixels = nullptr; int width = 0, height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    ImGui::NewFrame();
    ImDrawList* list = ImGui::GetForegroundDrawList();
    const ImVec2 p(180.f, 140.f), size(560.f, 340.f);
    constexpr float radius = 24.f, scale = 1.6f, alpha = .86f;
    const int fillCount = FillVertexCount(list, p, size, radius);
    const int shadowCount = ShadowVertexCount(list, p, size, radius, scale);
    assert(fillCount > 0 && fillCount < 512);

    std::set<ImU32> themeSignatures;
    int maxPanelVertices = 0, maxShellVertices = 0;
    for (int i = 0; i < spatial_theme::Count; ++i) {
        auto palette = spatial_theme::Get(i);
        palette.surface.w = palette.light ? .22f : .30f;
        Reset(list);
        const int first = list->VtxBuffer.Size;
        Panel(list, p, size, radius, palette, scale, alpha, .45f);
        maxPanelVertices = std::max(maxPanelVertices, list->VtxBuffer.Size - first);
        assert(list->VtxBuffer.Size > first + fillCount);
        assert(list->VtxBuffer.Size - first < 2000);
        AssertFinite(list, first);
        AssertBounds(list, first, p, size, 2.f * scale);
        const float panelLimit = palette.surface.w * alpha + 1.f / 255.f;
        assert(MaxAlpha(list, first, first + fillCount) <= panelLimit);
        themeSignatures.insert(list->VtxBuffer[first + fillCount / 2].col);

        Reset(list);
        const int shellFirst = list->VtxBuffer.Size;
        Shell(list, p, size, radius, palette, scale, alpha, 2.0, false);
        maxShellVertices = std::max(maxShellVertices, list->VtxBuffer.Size - shellFirst);
        assert(list->VtxBuffer.Size > shellFirst + shadowCount + fillCount);
        assert(list->VtxBuffer.Size - shellFirst < 2000);
        AssertFinite(list, shellFirst);
        AssertBounds(list, shellFirst, p, size, 6.f * scale);
        const float shellLimit = std::min(palette.shell.w, palette.light ? .20f : .28f) * alpha + 1.f / 255.f;
        assert(MaxAlpha(list, shellFirst + shadowCount, shellFirst + shadowCount + fillCount) <= shellLimit);
    }
    assert(themeSignatures.size() >= 5);
    std::printf("PASS seven themes: thin alpha, distinct tints, panel <= %d vertices, shell <= %d vertices\n",
        maxPanelVertices, maxShellVertices);

    const auto& palette = spatial_theme::Get(3);
    Reset(list);
    Shell(list, p, size, radius, palette, scale, alpha, 1.0, false);
    const auto stillA = Snapshot(list);
    Reset(list);
    Shell(list, p, size, radius, palette, scale, alpha, 9.0, false);
    const auto stillB = Snapshot(list);
    assert(Same(stillA, stillB));
    Reset(list);
    Shell(list, p, size, radius, palette, scale, alpha, 0.0, true);
    const auto pulseA = Snapshot(list);
    Reset(list);
    Shell(list, p, size, radius, palette, scale, alpha, 3.141592653589793 / .84, true);
    const auto pulseB = Snapshot(list);
    assert(!Same(pulseA, pulseB));
    std::puts("PASS disabled motion is stable; enabled motion gently changes edge light");

    const float nan = FloatBits(0x7fc00000u);
    const float inf = FloatBits(0x7f800000u);
    assert(!spatial_chrome::Finite(nan) && !spatial_chrome::Finite(inf));
    assert(!spatial_chrome::Finite(static_cast<double>(nan)));
    Reset(list);
    const int before = list->VtxBuffer.Size;
    Panel(list, {nan, p.y}, size, radius, palette, scale, alpha);
    Panel(list, p, {0.f, size.y}, radius, palette, scale, alpha);
    Panel(list, p, size, inf, palette, scale, alpha);
    Panel(list, p, size, radius, palette, -1.f, alpha);
    Panel(list, p, size, radius, palette, scale, 0.f);
    Shell(list, p, size, radius, palette, scale, nan, 0.0, false);
    Shell(nullptr, p, size, radius, palette, scale, alpha, 0.0, false);
    assert(list->VtxBuffer.Size == before);
    Shell(list, p, size, radius, palette, scale, alpha, static_cast<double>(nan), true);
    AssertFinite(list, before);
    std::puts("PASS invalid geometry and alpha are rejected under fast-math; invalid time stays finite");

    for (ImVec2 small : {ImVec2(2.01f, 2.01f), ImVec2(4.f, 12.f), ImVec2(12.f, 4.f)}) {
        for (float smallScale : {.001f, .1f, 1.f, 20.f}) {
            Reset(list);
            Panel(list, p, small, 10000.f, palette, smallScale, 1.f);
            AssertFinite(list, 0);
            AssertBounds(list, 0, p, small, 1.5f);
            assert(list->VtxBuffer.Size < 2000);
            Reset(list);
            Shell(list, p, small, 10000.f, palette, smallScale, 1.f, 0., false);
            AssertFinite(list, 0);
            AssertBounds(list, 0, p, small, std::max(1.25f, 6.f * spatial_chrome::Clamp(smallScale, .1f, 4.f)));
            assert(list->VtxBuffer.Size < 2000);
        }
    }
    std::puts("PASS tiny dimensions, excessive radii and scaled surfaces remain bounded");
    ImGui::Render();
    ImGui::DestroyContext();
    return 0;
}

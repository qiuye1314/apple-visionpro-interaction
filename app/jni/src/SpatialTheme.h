#pragma once
// Shared palette data. Every page uses the same selected palette; this file
// contains no UI state, rendering, input or application callbacks.
#include "imgui.h"

namespace spatial_theme {
constexpr int Count = 7;

struct Palette {
    const char* name;
    const char* subtitle;
    bool light;
    ImVec4 shell, sidebar, surface, field, hover, active;
    ImVec4 ink, muted, accent, onAccent, border, hero, heroInk;
};

inline ImVec4 Rgba(int r, int g, int b, int a = 255) {
    return ImVec4(r / 255.f, g / 255.f, b / 255.f, a / 255.f);
}

inline const Palette& Get(int index) {
    // Palette alpha is a material choice, never a per-page choice. Child
    // surfaces add a small matching tint over the shared shell.
    static const Palette palettes[Count] = {
        {
            "瓷白", "主页原色", true,
            Rgba(239,243,247,248), Rgba(245,249,253,245),
            Rgba(235,243,251,238), Rgba(229,232,237,238),
            Rgba(213,220,228,245), Rgba(191,202,214,250),
            Rgba(39,42,46), Rgba(101,111,122),
            Rgba(40,42,46), Rgba(255,255,255),
            Rgba(255,255,255,155), Rgba(28,37,48), Rgba(255,255,255)
        },
        {
            "雾透", "通透玻璃", true,
            Rgba(249,250,252,144), Rgba(249,250,252,36),
            Rgba(249,250,252,34), Rgba(246,249,252,72),
            Rgba(255,255,255,112), Rgba(231,238,245,150),
            Rgba(29,37,45), Rgba(47,62,75),
            Rgba(60,80,99), Rgba(255,255,255),
            Rgba(255,255,255,104), Rgba(224,235,245,174), Rgba(29,37,45)
        },
        {
            "石墨", "沉静深色", false,
            Rgba(23,27,34,236), Rgba(31,37,47,104),
            Rgba(42,49,61,88), Rgba(11,15,22,126),
            Rgba(114,128,148,76), Rgba(140,156,178,104),
            Rgba(240,243,248), Rgba(174,185,201),
            Rgba(216,225,238), Rgba(26,34,46),
            Rgba(226,234,247,56), Rgba(13,18,26,218), Rgba(240,243,248)
        },
        {
            "海湾", "清澈蓝调", false,
            Rgba(20,45,66,222), Rgba(27,60,83,96),
            Rgba(43,83,108,76), Rgba(8,29,46,128),
            Rgba(107,182,220,68), Rgba(120,207,255,92),
            Rgba(236,247,255), Rgba(170,201,222),
            Rgba(120,207,255), Rgba(9,43,67),
            Rgba(169,220,250,66), Rgba(10,31,49,202), Rgba(236,247,255)
        },
        {
            "松林", "柔和绿意", false,
            Rgba(24,46,40,226), Rgba(33,64,54,96),
            Rgba(47,82,68,76), Rgba(10,30,24,128),
            Rgba(125,191,163,68), Rgba(147,222,193,92),
            Rgba(238,249,242), Rgba(176,207,190),
            Rgba(147,222,193), Rgba(12,47,37),
            Rgba(185,232,207,64), Rgba(13,33,27,204), Rgba(238,249,242)
        },
        {
            "鸢尾", "低饱和紫", false,
            Rgba(40,33,57,226), Rgba(56,47,77,98),
            Rgba(75,62,101,76), Rgba(25,18,40,128),
            Rgba(167,144,212,68), Rgba(197,176,255,92),
            Rgba(247,242,255), Rgba(198,186,218),
            Rgba(197,176,255), Rgba(43,25,73),
            Rgba(222,206,252,64), Rgba(27,20,42,204), Rgba(247,242,255)
        },
        {
            "暖砂", "温润米色", true,
            Rgba(240,228,211,236), Rgba(250,240,225,126),
            Rgba(255,248,237,108), Rgba(215,197,173,128),
            Rgba(201,178,149,150), Rgba(182,153,120,168),
            Rgba(61,48,36), Rgba(113,89,65),
            Rgba(126,89,55), Rgba(255,250,243),
            Rgba(255,250,240,154), Rgba(102,74,49,230), Rgba(255,248,237)
        }
    };
    return palettes[index < 0 ? 0 : index >= Count ? Count - 1 : index];
}
} // namespace spatial_theme

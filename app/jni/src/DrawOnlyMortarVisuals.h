#pragma once
#include "DrawOnlyMortar.h"
#include "DrawOnlyVisuals.h"
namespace draw_only_mortar {
inline void Render(ImDrawList* list, const draw_only_mortar::Sample& sample) {
    using namespace draw_only_visuals;
    if (!list || !sample.valid) return;
    const ImVec2 view = ImGui::GetIO().DisplaySize;
    if (!ViewportValid(view)) return;
    const auto palette = PaletteColors();
    const float scale = Scale(view), radius = 8.f * scale;
    const ImVec2 point(sample.screen.X, sample.screen.Y);
    if (!PointValid(point) || point.x < 0 || point.y < 0 || point.x > view.x || point.y > view.y ||
        !draw_only_projection::ValidWorldPoint(sample.world) || !Finite(sample.distanceMeters) ||
        sample.distanceMeters < 0) return;
    list->PushClipRect({0, 0}, view, true);
    list->AddCircle(point, radius, palette.shadow, 32, 3.f * scale);
    list->AddCircle(point, radius, palette.visible, 32, 1.2f * scale);
    list->AddCircleFilled(point, 1.6f * scale, palette.visible, 12);
    for (int sign : {-1, 1}) {
        Line(list, {point.x + sign * 11.f * scale, point.y},
                   {point.x + sign * 16.f * scale, point.y}, palette.visible, 1.f);
        Line(list, {point.x, point.y + sign * 11.f * scale},
                   {point.x, point.y + sign * 16.f * scale}, palette.visible, 1.f);
    }
    char title[96]{}, coordinates[128]{};
    std::snprintf(title, sizeof(title), "迫击炮落点 · %.0f m", sample.distanceMeters);
    std::snprintf(coordinates, sizeof(coordinates), "X %.0f  Y %.0f  Z %.0f",
                  sample.world.X, sample.world.Y, sample.world.Z);
    float labelY = point.y + 21.f * scale;
    if (labelY + 40.f * scale > view.y - 4.f) labelY = point.y - 56.f * scale;
    labelY = Clamp(labelY, 4.f, view.y - 43.f * scale);
    Text(list, point.x, labelY, title, 16.f * scale, palette.visible, true, 310.f * scale);
    Text(list, point.x, labelY + 23.f * scale, coordinates, 12.f * scale,
         palette.secondary, true, 360.f * scale);
    list->PopClipRect();
}
} // namespace draw_only_mortar

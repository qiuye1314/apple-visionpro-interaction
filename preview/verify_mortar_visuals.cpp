#include "DrawOnlyMortarVisuals.h"
#include <cassert>
#include <cfloat>
#include <cstdio>
#include <limits>

static void Pass(const char* text) { std::printf("PASS %s\n",text); }
static bool HasColor(const ImDrawList* list,int start,ImU32 color) {
    for(int i=start;i<list->VtxBuffer.Size;++i)if(list->VtxBuffer[i].col==color)return true;
    return false;
}
int main() {
    using namespace draw_only_visuals;
    using draw_only_mortar::Sample;
    ImGui::CreateContext();auto& io=ImGui::GetIO();
    io.IniFilename=nullptr;io.DisplaySize={1600,1000};io.DeltaTime=1.f/60.f;
    io.Fonts->AddFontDefault();unsigned char* pixels=nullptr;int width=0,height=0;
    io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
    ImGui::NewFrame();auto* list=ImGui::GetForegroundDrawList();
    const Sample good{{12000,34000,5000},{800,500},325,true};
    const float nan=std::numeric_limits<float>::quiet_NaN();
    const float inf=std::numeric_limits<float>::infinity();
    const int empty=list->VtxBuffer.Size;
    draw_only_mortar::Render(nullptr,good);
    for(int reason=0;reason<10;++reason) {
        Sample bad=good;
        switch(reason) {
        case 0:bad.valid=false;break;
        case 1:bad.world={};break;
        case 2:bad.world.X=nan;break;
        case 3:bad.screen.X=nan;break;
        case 4:bad.screen.Y=inf;break;
        case 5:bad.screen.X=-1;break;
        case 6:bad.screen.Y=1001;break;
        case 7:bad.distanceMeters=inf;break;
        case 8:bad.distanceMeters=nan;break;
        case 9:bad.distanceMeters=-1;break;
        }
        draw_only_mortar::Render(list,bad);
        assert(list->VtxBuffer.Size==empty);
    }
    Pass("invalid samples and offscreen anchors emit no mortar vertices");
    for(int theme=0;theme<spatial_theme::Count;++theme) {
        SetTheme(theme);const auto palette=PaletteColors();
        const int start=list->VtxBuffer.Size;
        draw_only_mortar::Render(list,good);
        assert(list->VtxBuffer.Size>start);
        assert(HasColor(list,start,palette.visible));
        assert(HasColor(list,start,palette.glass));
        assert(HasColor(list,start,palette.secondary));
    }
    Pass("all global themes color the shared reticle and two glass information rows");
    SetTheme(3);const auto palette=PaletteColors();const int rows=list->VtxBuffer.Size;
    draw_only_mortar::Render(list,good);
    float titleBottom=-FLT_MAX,coordinatesTop=FLT_MAX;
    for(int i=rows;i<list->VtxBuffer.Size;++i) {
        const auto& vertex=list->VtxBuffer[i];
        if(vertex.col==palette.visible && vertex.pos.y>good.screen.Y+18*Scale(io.DisplaySize))
            titleBottom=std::max(titleBottom,vertex.pos.y);
        if(vertex.col==palette.secondary)coordinatesTop=std::min(coordinatesTop,vertex.pos.y);
    }
    assert(titleBottom>-FLT_MAX && coordinatesTop<FLT_MAX && coordinatesTop>titleBottom+2);
    Pass("distance title and XYZ text occupy separate readable rows");
    ImGui::EndFrame();
    for(ImVec2 view:{ImVec2(360,800),ImVec2(800,360),ImVec2(1904,3040),ImVec2(3840,2160)}) {
        io.DisplaySize=view;ImGui::NewFrame();list=ImGui::GetForegroundDrawList();
        for(ImVec2 anchor:{ImVec2(0,0),ImVec2(view.x,0),ImVec2(0,view.y),ImVec2(view.x,view.y)}) {
            Sample edge=good;edge.screen={anchor.x,anchor.y};const int start=list->VtxBuffer.Size;
            draw_only_mortar::Render(list,edge);assert(list->VtxBuffer.Size>start);
            bool glassFound=false;
            for(int i=start;i<list->VtxBuffer.Size;++i) {
                const auto& vertex=list->VtxBuffer[i];
                assert(Finite(vertex.pos.x)&&Finite(vertex.pos.y));
                if(vertex.col==palette.glass) {
                    glassFound=true;
                    assert(vertex.pos.x>=0&&vertex.pos.x<=view.x);
                    assert(vertex.pos.y>=0&&vertex.pos.y<=view.y);
                }
            }
            assert(glassFound);
        }
        ImGui::EndFrame();
    }
    Pass("corner labels stay inside phone and tablet viewports with finite geometry");
    ImGui::DestroyContext();
}

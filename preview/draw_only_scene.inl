#pragma once
#include "DrawOnlyVisuals.h"
#include "DrawOnlyMortarVisuals.h"

namespace draw_only_scene {
inline draw_only_visuals::Entity Person(float x,float y,float height,bool bot,bool visible){
    draw_only_visuals::Entity entity;const float width=height*.37f;
    entity.box={x-width*.5f,y,x+width*.5f,y+height};entity.bot=bot;entity.visible.fill(visible);
    entity.joints={{{x,y+height*.065f},{x,y+height*.25f},{x,y+height*.53f},
        {x-width*.32f,y+height*.25f},{x+width*.32f,y+height*.25f},
        {x-width*.49f,y+height*.40f},{x+width*.49f,y+height*.40f},
        {x-width*.41f,y+height*.54f},{x+width*.41f,y+height*.54f},
        {x-width*.19f,y+height*.56f},{x+width*.19f,y+height*.56f},
        {x-width*.25f,y+height*.76f},{x+width*.25f,y+height*.76f},
        {x-width*.37f,y+height*.98f},{x+width*.37f,y+height*.98f}}};
    return entity;
}
inline void Silhouette(ImDrawList* draw,const draw_only_visuals::Entity& entity){
    const float h=entity.box.Height();const auto& p=entity.joints;
    const ImU32 color=entity.visible[0]?IM_COL32(47,57,59,245):IM_COL32(59,69,69,130);
    draw->AddCircleFilled(p[0],h*.047f,color,24);
    static constexpr int edges[][2]={{1,2},{1,3},{1,4},{3,5},{4,6},{5,7},{6,8},{2,9},{2,10},{9,11},{10,12},{11,13},{12,14}};
    for(const auto& edge:edges)draw->AddLine(p[edge[0]],p[edge[1]],color,h*.056f);
}
// Call during any active ImGui frame; this uses the exact Android rendering primitives.
inline void Render(int theme=3){
    using namespace draw_only_visuals;SetTheme(theme);
    const auto view=ImGui::GetIO().DisplaySize;auto* draw=ImGui::GetBackgroundDrawList();
    draw->AddRectFilledMultiColor({0,0},view,IM_COL32(83,104,112,255),IM_COL32(83,104,112,255),
                                 IM_COL32(57,66,64,255),IM_COL32(57,66,64,255));
    draw->AddRectFilled({0,view.y*.4f},view,IM_COL32(70,78,72,255));
    const ImU32 line=IM_COL32(160,176,166,15);
    for(int i=-5;i<7;++i)draw->AddLine({view.x*.5f,view.y*.4f},{view.x*.5f+i*view.x*.24f,view.y},line,1);
    for(int i=0;i<7;++i){const float f=i/7.f;const float y=view.y*(.42f+.6f*f*f);draw->AddLine({0,y},{view.x,y},line,1);}
    draw->AddRectFilled({0,view.y*.26f},{view.x*.18f,view.y*.65f},IM_COL32(66,79,80,255));
    draw->AddRectFilled({view.x*.76f,view.y*.22f},{view.x,view.y*.57f},IM_COL32(62,75,77,255));
    draw->AddRectFilled({view.x*.83f,view.y*.45f},{view.x*.91f,view.y*.57f},IM_COL32(38,52,54,255));
    auto first=Person(view.x*.31f,view.y*.41f,view.y*.32f,false,true);
    first.name="清风";first.team=3;first.distance=42;first.weapon="M416";first.ammo=30;first.capacity=40;first.health=78;
    auto second=Person(view.x*.56f,view.y*.42f,view.y*.22f,false,false);
    second.visible[1]=second.visible[3]=second.visible[5]=second.visible[7]=true;
    second.name="远山";second.team=7;second.distance=96;second.weapon="SCAR-L";second.ammo=18;second.capacity=30;second.health=42;
    auto third=Person(view.x*.71f,view.y*.49f,view.y*.17f,true,true);
    third.team=12;third.distance=134;third.weapon="UMP45";third.ammo=25;third.capacity=25;third.health=100;
    Silhouette(draw,first);Silhouette(draw,second);Silhouette(draw,third);
    auto* overlay=ImGui::GetForegroundDrawList();EntityOverlay(overlay,first);EntityOverlay(overlay,second);EntityOverlay(overlay,third);
    Summary(overlay,2,1,true);
    draw_only_mortar::Sample mortar;
    mortar.world={12845.f,6720.f,340.f};
    mortar.screen={view.x*.48f,view.y*.79f};
    mortar.distanceMeters=156.f;mortar.valid=true;
    draw_only_mortar::Render(overlay,mortar);
    Text(overlay,view.x*.5f,view.y-44*Scale(view),"绘制专版  /  1.39",14*Scale(view),PaletteColors().secondary);
}
} // namespace draw_only_scene

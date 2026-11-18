#pragma once
// The Android overlay and desktop preview share these pure screen-space primitives.
#include "imgui.h"
#include "SpatialTheme.h"
#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <string>

namespace draw_only_visuals {
inline bool Finite(float value) {
    std::uint32_t bits;std::memcpy(&bits,&value,sizeof(bits));
    return (bits&0x7f800000u)!=0x7f800000u;
}
inline float Clamp(float value,float low,float high){return std::max(low,std::min(high,value));}
inline int& ThemeIndex(){static int value=0;return value;}
inline void SetTheme(int index){ThemeIndex()=std::max(0,std::min(spatial_theme::Count-1,index));}
inline ImVec4 Mix(ImVec4 a,ImVec4 b,float factor){return {a.x+(b.x-a.x)*factor,a.y+(b.y-a.y)*factor,a.z+(b.z-a.z)*factor,a.w+(b.w-a.w)*factor};}
inline ImU32 WithAlpha(ImU32 color,float factor){
    const int alpha=static_cast<int>((color>>IM_COL32_A_SHIFT)&255);
    return (color&~IM_COL32_A_MASK)|(static_cast<ImU32>(Clamp(alpha*factor,0,255))<<IM_COL32_A_SHIFT);
}
struct Palette {
    ImU32 visible,occluded,bot,text,secondary,glass,border,shadow,danger;
};
inline Palette PaletteColors(){
    const auto& theme=spatial_theme::Get(ThemeIndex());
    const ImVec4 silver(0.89f,0.93f,0.96f,1.f),slate(0.55f,0.61f,0.66f,1.f);
    // Light interface palettes still need a bright, readable color over a game scene.
    ImVec4 accent=theme.light?Mix(silver,theme.accent,.17f):Mix(theme.accent,silver,.15f);
    ImVec4 muted=Mix(accent,slate,.76f);muted.w=.57f;
    ImVec4 bot=Mix(accent,silver,.78f);bot.w=.78f;
    ImVec4 surface=Mix(ImVec4(.045f,.063f,.082f,.78f),theme.hero,.10f);surface.w=.78f;
    return {ImGui::ColorConvertFloat4ToU32(accent),ImGui::ColorConvertFloat4ToU32(muted),
        ImGui::ColorConvertFloat4ToU32(bot),IM_COL32(240,245,248,246),IM_COL32(188,202,212,230),
        ImGui::ColorConvertFloat4ToU32(surface),WithAlpha(ImGui::ColorConvertFloat4ToU32(accent),.23f),
        IM_COL32(9,14,20,205),IM_COL32(234,174,112,245)};
}
inline ImU32 Color(bool visible,bool bot){const auto p=PaletteColors();return !visible?p.occluded:bot?p.bot:p.visible;}
inline bool ViewportValid(ImVec2 size){return Finite(size.x)&&Finite(size.y)&&size.x>=32&&size.y>=32&&size.x<=32768&&size.y<=32768;}
inline float Scale(ImVec2 size){return Clamp(size.y/1080.f,.82f,1.6f);}
inline bool PointValid(ImVec2 point){return Finite(point.x)&&Finite(point.y)&&std::fabs(point.x)<=100000.f&&std::fabs(point.y)<=100000.f;}
inline bool JointValid(ImVec2 point){return PointValid(point)&&(point.x!=0.f||point.y!=0.f);}
struct Box {
    float left=0,top=0,right=0,bottom=0;
    float Width()const{return right-left;}
    float Height()const{return bottom-top;}
    float Center()const{return (left+right)*.5f;}
};
inline bool BoxValid(Box box,ImVec2 size){
    return ViewportValid(size)&&PointValid({box.left,box.top})&&PointValid({box.right,box.bottom})&&
        box.Width()>=3&&box.Height()>=8&&box.Width()<=size.x*2&&box.Height()<=size.y*2&&
        box.right>0&&box.left<size.x&&box.bottom>0&&box.top<size.y;
}
inline constexpr int SkeletonPairs[][2]={{0,1},{1,2},{1,3},{3,5},{5,7},{1,4},{4,6},{6,8},
                                        {2,9},{9,11},{11,13},{2,10},{10,12},{12,14}};
inline std::array<bool,15> ReliableJoints(const std::array<ImVec2,15>& joints,Box reference,ImVec2 view){
    std::array<bool,15> accepted{};std::array<float,15> xs{},ys{},distances{};int count=0;
    if(!ViewportValid(view))return accepted;
    for(const auto& point:joints)if(JointValid(point)){xs[count]=point.x;ys[count++]=point.y;}
    if(!count)return accepted;
    std::sort(xs.begin(),xs.begin()+count);std::sort(ys.begin(),ys.begin()+count);
    const ImVec2 center(xs[(count-1)/2],ys[(count-1)/2]);int index=0;
    for(const auto& point:joints)if(JointValid(point)){
        const float dx=point.x-center.x,dy=point.y-center.y;distances[index++]=std::sqrt(dx*dx+dy*dy);
    }
    std::sort(distances.begin(),distances.begin()+count);
    const bool hasReference=BoxValid(reference,view);
    const float referenceSpan=hasReference?std::max(reference.Width(),reference.Height()):0.f;
    // Coordinate medians prevent one bad head/ankle from enlarging the entire figure.
    const float baseline=hasReference?std::max(28.f,referenceSpan*.35f):std::min(view.x,view.y)*.12f;
    const float radius=std::max(baseline,distances[(count-1)/2]*4.f);
    float referenceRadius=referenceSpan*1.5f;
    if(hasReference&&count>=5){
        const float dx=center.x-reference.Center(),dy=center.y-(reference.top+reference.bottom)*.5f;
        // A world-Z root can collapse in an overhead view while a prone body stays wide.
        // A populated, median-filtered skeleton supplies its own scale in that case.
        referenceRadius=std::max(referenceRadius,radius+std::sqrt(dx*dx+dy*dy));
    }
    for(std::size_t i=0;i<joints.size();++i){
        if(!JointValid(joints[i]))continue;
        const float dx=joints[i].x-center.x,dy=joints[i].y-center.y;
        if(count>=3&&dx*dx+dy*dy>radius*radius)continue;
        if(hasReference){
            const float rx=joints[i].x-reference.Center(),ry=joints[i].y-(reference.top+reference.bottom)*.5f;
            if(rx*rx+ry*ry>referenceRadius*referenceRadius)continue;
        }
        accepted[i]=true;
    }
    return accepted;
}
inline std::array<ImVec2,15> FilterJoints(const std::array<ImVec2,15>& joints,Box reference,ImVec2 view){
    auto filtered=joints;const auto accepted=ReliableJoints(joints,reference,view);
    for(std::size_t i=0;i<filtered.size();++i)if(!accepted[i])filtered[i]={0,0};
    return filtered;
}
inline Box BoxFromJoints(const std::array<ImVec2,15>& joints,Box root,ImVec2 view){
    const bool hasRoot=BoxValid(root,view);const auto accepted=ReliableJoints(joints,root,view);
    Box bounds{FLT_MAX,FLT_MAX,-FLT_MAX,-FLT_MAX};int count=0;
    for(std::size_t i=0;i<joints.size();++i)if(accepted[i]){
        bounds.left=std::min(bounds.left,joints[i].x);bounds.right=std::max(bounds.right,joints[i].x);
        bounds.top=std::min(bounds.top,joints[i].y);bounds.bottom=std::max(bounds.bottom,joints[i].y);++count;
    }
    if(count<2)return hasRoot?root:Box{};
    if(hasRoot){
        if(!accepted[0])bounds.top=std::min(bounds.top,root.top);
        if(!accepted[13]&&!accepted[14])bounds.bottom=std::max(bounds.bottom,root.bottom);
    }
    const float center=bounds.Center(),minWidth=hasRoot?root.Width():std::max(6.f,bounds.Height()*.32f);
    if(bounds.Width()<minWidth){bounds.left=center-minWidth*.5f;bounds.right=center+minWidth*.5f;}
    const float padding=Clamp(std::max(bounds.Width(),bounds.Height())*.035f,2.f*Scale(view),10.f*Scale(view));
    bounds.left-=padding;bounds.right+=padding;bounds.top-=padding;bounds.bottom+=padding;
    if(bounds.Height()<8.f){bounds.top-=4.f;bounds.bottom+=4.f;}
    return BoxValid(bounds,view)?bounds:hasRoot?root:Box{};
}
inline bool ClipLine(ImVec2& a,ImVec2& b,ImVec2 viewport){
    if(!ViewportValid(viewport)||!PointValid(a)||!PointValid(b))return false;
    const float dx=b.x-a.x,dy=b.y-a.y;float first=0,last=1;
    const float p[4]={-dx,dx,-dy,dy},q[4]={a.x,viewport.x-a.x,a.y,viewport.y-a.y};
    for(int i=0;i<4;++i){
        if(std::fabs(p[i])<.00001f){if(q[i]<0)return false;continue;}
        const float t=q[i]/p[i];
        if(p[i]<0)first=std::max(first,t);else last=std::min(last,t);
        if(first>last)return false;
    }
    const ImVec2 start=a;a={start.x+first*dx,start.y+first*dy};b={start.x+last*dx,start.y+last*dy};
    return PointValid(a)&&PointValid(b);
}
inline void Line(ImDrawList* list,ImVec2 a,ImVec2 b,ImU32 color,float width,bool outline=true){
    if(!list||!Finite(width)||!ClipLine(a,b,ImGui::GetIO().DisplaySize))return;
    width=Clamp(width,.6f,3.f)*Scale(ImGui::GetIO().DisplaySize);
    if(outline)list->AddLine(a,b,PaletteColors().shadow,width+1.35f);
    list->AddLine(a,b,color,width);
}
inline ImVec2 Measure(const std::string& text,float size){return ImGui::GetFont()->CalcTextSizeA(size,FLT_MAX,0,text.c_str());}
inline std::string Fit(const std::string& source,float size,float maximum){
    std::string text;const std::size_t count=std::min<std::size_t>(source.size(),256);
    for(std::size_t i=0;i<count;){
        const auto c=static_cast<unsigned char>(source[i]);
        std::size_t bytes=c<0x80?1:(c&0xe0)==0xc0?2:(c&0xf0)==0xe0?3:(c&0xf8)==0xf0?4:1;
        if(i+bytes>count)break;
        const std::string piece=c<32?" ":source.substr(i,bytes);
        if(Measure(text+piece+"...",size).x>maximum){if(!text.empty())text+="...";return text;}
        text+=piece;i+=bytes;
    }
    return text;
}
struct Layout {
    float nameY,healthY,distanceY,weaponY;
    float nameSize,metaSize;
};
inline Layout LayoutFor(Box box,ImVec2 view){
    const float s=Scale(view),name=16*s,meta=14*s;
    const float nameMaximum=std::max(6*s,view.y-name-2*meta-36*s);
    const float nameY=Clamp(box.top-name-15*s,6*s,nameMaximum);
    const float healthY=std::min(std::max(box.top-7*s,nameY+name+5*s),view.y-2*meta-24*s);
    float distanceY=Clamp(box.bottom+5*s,healthY+7*s,view.y-2*meta-12*s);
    // Small, distant figures keep information outside the skeleton footprint.
    distanceY=std::max(distanceY,healthY+6*s);
    return {nameY,healthY,distanceY,distanceY+meta+4*s,name,meta};
}
inline void Text(ImDrawList* list,float center,float y,const std::string& value,float size,ImU32 color,
                 bool plate=false,float maximum=240.f,bool statusDot=false,bool bot=false,ImU32 dotColor=0){
    const auto viewport=ImGui::GetIO().DisplaySize;
    if(!list||!ViewportValid(viewport)||!Finite(center)||!Finite(y)||!Finite(size)||size<8||size>80)return;
    const float s=Scale(viewport),padding=plate?7*s:0,dot=statusDot?11*s:0;
    const auto text=Fit(value,size,std::min(maximum,viewport.x-20*s)-padding*2-dot);
    if(text.empty())return;
    const ImVec2 measure=Measure(text,size);
    const float width=measure.x+padding*2+dot;
    if(width>viewport.x-4||y+measure.y<0||y>viewport.y)return;
    const float x=Clamp(center-width*.5f,4.f,viewport.x-width-4.f);
    y=Clamp(y,3.f,viewport.y-measure.y-4.f);
    const auto palette=PaletteColors();
    list->PushClipRect({0,0},viewport,true);
    if(plate){
        list->AddRectFilled({x,y-3*s},{x+width,y+measure.y+3*s},palette.glass,4*s);
        list->AddRect({x,y-3*s},{x+width,y+measure.y+3*s},palette.border,4*s,0,.7f*s);
    }
    if(statusDot){
        const ImVec2 position(x+padding+3*s,y+size*.5f);
        if(bot)list->AddCircle(position,2.1f*s,dotColor?dotColor:color,12,1*s);
        else list->AddCircleFilled(position,2.1f*s,dotColor?dotColor:color,12);
    }
    const ImVec2 position(x+padding+dot,y);
    list->AddText(ImGui::GetFont(),size,{position.x+1,position.y+1},palette.shadow,text.c_str());
    list->AddText(ImGui::GetFont(),size,position,color,text.c_str());
    list->PopClipRect();
}
inline void Corners(ImDrawList* list,Box box,bool visible,bool bot,float width=1.2f){
    const auto view=ImGui::GetIO().DisplaySize;if(!BoxValid(box,view))return;
    const float s=Scale(view),x=Clamp(box.Width()*.22f,5*s,18*s),y=Clamp(box.Height()*.11f,5*s,20*s);
    const auto color=WithAlpha(Color(visible,bot),.85f);
    for(int i=0;i<4;++i){
        const bool right=(i&1)!=0,bottom=(i&2)!=0;
        const ImVec2 corner(right?box.right:box.left,bottom?box.bottom:box.top);
        Line(list,corner,{corner.x+(right?-x:x),corner.y},color,width);
        Line(list,corner,{corner.x,corner.y+(bottom?-y:y)},color,width);
    }
}
inline void Ray(ImDrawList* list,Box box,bool visible,bool bot,float width=1.f){
    const auto view=ImGui::GetIO().DisplaySize;if(!list||!BoxValid(box,view))return;
    const float s=Scale(view);const auto layout=LayoutFor(box,view);
    const ImVec2 start(view.x*.5f,58*s),end(box.Center(),layout.nameY-9*s);
    if(end.y<=start.y+18*s)return;
    Line(list,start,end,WithAlpha(Color(visible,bot),.50f),width);
    if(end.x>=2&&end.x<=view.x-2)list->AddCircleFilled(end,1.65f*s,Color(visible,bot),12);
}
inline void Skeleton(ImDrawList* list,Box box,const std::array<ImVec2,15>& joints,
                     const std::array<bool,15>& visible,bool bot,float width=1.15f){
    const auto view=ImGui::GetIO().DisplaySize;if(!list||!Finite(width)||!ViewportValid(view))return;
    const auto accepted=ReliableJoints(joints,box,view);
    std::array<float,14> lengths{};int count=0;
    for(const auto& edge:SkeletonPairs)if(accepted[edge[0]]&&accepted[edge[1]]){
        const float dx=joints[edge[1]].x-joints[edge[0]].x,dy=joints[edge[1]].y-joints[edge[0]].y;
        const float length=std::sqrt(dx*dx+dy*dy);if(length>=1.f)lengths[count++]=length;
    }
    float maximum=BoxValid(box,view)?std::max(28.f,std::max(box.Width(),box.Height())*.95f):std::min(view.x,view.y)*.4f;
    if(count>=3){
        std::sort(lengths.begin(),lengths.begin()+count);
        maximum=std::min(maximum,std::max(28.f,lengths[(count-1)/2]*4.f));
    }
    const float maxSquared=maximum*maximum;
    for(const auto& edge:SkeletonPairs){
        const ImVec2 a=joints[edge[0]],b=joints[edge[1]];
        if(!accepted[edge[0]]||!accepted[edge[1]])continue;
        const float dx=b.x-a.x,dy=b.y-a.y;
        if(dx*dx+dy*dy>maxSquared)continue;
        // Both end joints must be visible before a limb receives the brighter color.
        Line(list,a,b,Color(visible[edge[0]]&&visible[edge[1]],bot),width);
    }
    if(accepted[0]&&accepted[1]){
        const float dx=joints[1].x-joints[0].x,dy=joints[1].y-joints[0].y;
        if(dx*dx+dy*dy>maxSquared)return;
        float radius=Clamp(std::sqrt(dx*dx+dy*dy)*.27f,2.5f*Scale(view),11.f*Scale(view));
        if(joints[0].x>=radius&&joints[0].y>=radius&&joints[0].x<view.x-radius&&joints[0].y<view.y-radius){
            list->AddCircle(joints[0],radius,PaletteColors().shadow,20,Clamp(width,.6f,3.f)*Scale(view)+1.25f);
            list->AddCircle(joints[0],radius,Color(visible[0],bot),20,Clamp(width,.6f,3.f)*Scale(view));
        }
    }
}
inline void HeadMarker(ImDrawList* list,ImVec2 point,bool bot,bool visible=true){
    const auto view=ImGui::GetIO().DisplaySize;const float s=Scale(view);
    if(!list||!ViewportValid(view)||!JointValid(point)||point.x<4*s||point.x>view.x-4*s||point.y<4*s||point.y>view.y-4*s)return;
    list->AddCircleFilled(point,2.4f*s,PaletteColors().shadow,12);
    list->AddCircleFilled(point,1.25f*s,Color(visible,bot),12);
}
inline void Health(ImDrawList* list,Box box,float current,float maximum,bool visible,bool bot){
    const auto view=ImGui::GetIO().DisplaySize;
    if(!list||!BoxValid(box,view)||!Finite(current)||!Finite(maximum)||maximum<=0||current<0||maximum>1e6f)return;
    const float s=Scale(view),width=Clamp(box.Width(),42*s,116*s),height=3*s;
    const auto layout=LayoutFor(box,view);const float y=layout.healthY;
    if(y<0||y+height>view.y)return;
    const float x=Clamp(box.Center()-width*.5f,3.f,view.x-width-3.f),ratio=Clamp(current/maximum,0,1);
    const auto palette=PaletteColors();
    list->AddRectFilled({x-1,y-1},{x+width+1,y+height+1},palette.shadow,1.5f*s);
    list->AddRectFilled({x,y},{x+width,y+height},IM_COL32(88,103,115,110),1*s);
    if(ratio>0)list->AddRectFilled({x,y},{x+width*ratio,y+height},ratio<.25f?palette.danger:Color(visible,bot),1*s);
}
inline void Name(ImDrawList* list,Box box,const std::string& name,int team,bool visible,bool bot,
                 bool throwable=false,float seconds=0){
    const auto view=ImGui::GetIO().DisplaySize;if(!BoxValid(box,view))return;
    const float s=Scale(view);const auto layout=LayoutFor(box,view);
    char number[24]={};if(team>=0)std::snprintf(number,sizeof(number),"%02d  ",team);
    const auto text=std::string(number)+(bot?"人机":name.empty()?"玩家":name);
    Text(list,box.Center(),layout.nameY,text,layout.nameSize,PaletteColors().text,true,240*s,true,bot,Color(visible,bot));
    if(throwable&&Finite(seconds)&&seconds>0&&seconds<=10){
        char warning[48];std::snprintf(warning,sizeof(warning),"投掷 %.1fs",seconds);
        const float y=layout.nameY-layout.metaSize-9*s;
        if(y>=4*s)Text(list,box.Center(),y,warning,layout.metaSize,PaletteColors().danger,true,160*s);
    }
}
inline void Distance(ImDrawList* list,Box box,int meters){
    const auto view=ImGui::GetIO().DisplaySize;if(!BoxValid(box,view)||meters<0||meters>5000)return;
    const auto layout=LayoutFor(box,view);char text[32];std::snprintf(text,sizeof(text),"%d m",meters);
    Text(list,box.Center(),layout.distanceY,text,layout.metaSize,PaletteColors().text);
}
inline void Weapon(ImDrawList* list,Box box,const std::string& name,int ammo,int capacity){
    const auto view=ImGui::GetIO().DisplaySize;if(!BoxValid(box,view))return;
    const auto layout=LayoutFor(box,view);std::string text=name;
    if(capacity>0&&capacity<=999&&ammo>=0&&ammo<=999){char count[48];std::snprintf(count,sizeof(count),"  %d/%d",ammo,capacity);text+=count;}
    Text(list,box.Center(),layout.weaponY,text,layout.metaSize*.94f,PaletteColors().secondary,false,240*Scale(view));
}
inline void Summary(ImDrawList* list,int players,int bots,bool ready){
    const auto view=ImGui::GetIO().DisplaySize;if(!list||!ViewportValid(view))return;
    std::string text;
    if(!ready)text="等待进入对局";
    else {char counts[80];std::snprintf(counts,sizeof(counts),"真人 %d   /   人机 %d",std::max(0,players),std::max(0,bots));text=counts;}
    Text(list,view.x*.5f,22*Scale(view),text,15*Scale(view),PaletteColors().text,true,280*Scale(view));
}
struct Entity {
    Box box;std::array<ImVec2,15> joints{};std::array<bool,15> visible{};
    bool bot=false;std::string name="玩家",weapon;int team=1,distance=42,ammo=0,capacity=0;
    float health=75,maximumHealth=100;
};
inline void EntityOverlay(ImDrawList* list,const Entity& entity){
    const bool visible=entity.visible[0]||entity.visible[1]||entity.visible[2];
    Corners(list,entity.box,visible,entity.bot);Ray(list,entity.box,visible,entity.bot);
    Skeleton(list,entity.box,entity.joints,entity.visible,entity.bot);
    Health(list,entity.box,entity.health,entity.maximumHealth,visible,entity.bot);
    Name(list,entity.box,entity.name,entity.team,visible,entity.bot);
    Distance(list,entity.box,entity.distance);
    if(!entity.weapon.empty())Weapon(list,entity.box,entity.weapon,entity.ammo,entity.capacity);
}
} // namespace draw_only_visuals

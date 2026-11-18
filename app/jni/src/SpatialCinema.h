#pragma once
// Product reveal over the embedded user-supplied startup video.
#include "imgui.h"
#include "SpatialVideo.h"
#include <cmath>
#include <algorithm>

namespace spatial_cinema {
constexpr float Duration=spatial_video::Duration;
inline float Unit(float v){return std::max(0.f,std::min(1.f,v));}
inline float Smooth(float v){v=Unit(v);return v*v*(3-2*v);}
inline float Lerp(float a,float b,float t){return a+(b-a)*t;}
inline ImU32 C(int r,int g,int b,float a){return IM_COL32(r,g,b,int(255*Unit(a)));}
struct Camera {
    ImVec2 center;
    float scale,pitch,yaw,roll;
    ImVec2 Project(ImVec2 p,float depth=0) const {
        float x=p.x-560,y=p.y-360,z=-depth;
        float rx=-pitch*IM_PI/180,ry=yaw*IM_PI/180,rz=roll*IM_PI/180;
        float yy=y*std::cos(rx)-z*std::sin(rx),zz=y*std::sin(rx)+z*std::cos(rx);
        float xx=x*std::cos(ry)+zz*std::sin(ry);z=-x*std::sin(ry)+zz*std::cos(ry);
        y=yy*std::cos(rz)+xx*std::sin(rz);x=xx*std::cos(rz)-yy*std::sin(rz);
        float k=scale*1800/std::max(300.f,1800+z);
        return {center.x+x*k,center.y+y*k};
    }
};
inline void Words(ImDrawList* d,ImVec2 p,float size,ImU32 color,const char* text){
    d->AddText(ImGui::GetFont(),size,p,color,text);
}
inline Camera Draw(float time,bool reduced,ImVec2 destination,float finalScale,float pitch,float yaw,float roll){
    spatial_video::Draw(time,reduced);
    const ImVec2 screen=ImGui::GetIO().DisplaySize;
    const float duration=reduced?.45f:Duration;
    float exit=Smooth((time-(duration-.55f))/.55f),alpha=1-exit;
    if(reduced)alpha=1-Smooth(time/duration);
    float unit=std::min(screen.x/1600,screen.y/1000);
    // Keep the existing panel camera choreography. The supplied background
    // plays at its own frame rate and fades during the last 0.6 seconds.
    float pull=Smooth((time-.25f)/1.6f),orbit=Smooth((time-1.75f)/2.5f),settle=Smooth((time-4.25f)/1.35f);
    Camera cam{destination,Lerp(2.2f,.77f,pull)*unit,
        Lerp(20,8,pull)+26*std::sin(orbit*IM_PI),Lerp(78,32,pull)-66*orbit,Lerp(-22,-5,pull)+13*orbit};
    cam.center.x=Lerp(cam.center.x,destination.x,settle);cam.center.y=Lerp(cam.center.y,destination.y,settle);
    cam.scale=Lerp(cam.scale,finalScale,settle);cam.pitch=Lerp(cam.pitch,pitch,settle);cam.yaw=Lerp(cam.yaw,yaw,settle);cam.roll=Lerp(cam.roll,roll,settle);
    if(reduced){cam.center=destination;cam.scale=finalScale;cam.pitch=pitch;cam.yaw=yaw;cam.roll=roll;}
    auto* d=ImGui::GetForegroundDrawList();
    float titleAlpha=Smooth((time-1.6f)/.5f)*(1-Smooth((time-4.4f)/.65f))*alpha;
    float fs=24*unit;const char* title="让空间，围绕你。";
    ImVec2 ts=ImGui::GetFont()->CalcTextSizeA(fs,10000,0,title);
    Words(d,{(screen.x-ts.x)*.5f,screen.y*.85f},fs,C(229,241,253,titleAlpha),title);
    const char* brand="L S R A E L   /   S P A T I A L";fs=11*unit;ts=ImGui::GetFont()->CalcTextSizeA(fs,10000,0,brand);
    Words(d,{(screen.x-ts.x)*.5f,screen.y*.90f},fs,C(154,186,214,titleAlpha*.8f),brand);
    return cam;
}
} // namespace spatial_cinema

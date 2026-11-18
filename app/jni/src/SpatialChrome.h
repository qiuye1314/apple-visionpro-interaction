#pragma once
// Draw-list material primitives. Coordinates and radii are already in pixels;
// motion=true enables a slow edge-light pulse, false keeps a fixed appearance.
// These helpers own no UI state and never add input items or change clipping.
#include "imgui.h"
#include "SpatialTheme.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace spatial_chrome {
inline float Clamp(float value,float low,float high){return std::max(low,std::min(high,value));}
inline ImVec4 Mix(ImVec4 a,ImVec4 b,float amount){
    return {a.x+(b.x-a.x)*amount,a.y+(b.y-a.y)*amount,
            a.z+(b.z-a.z)*amount,a.w+(b.w-a.w)*amount};
}
inline ImVec4 Opacity(ImVec4 color,float value){color.w=value;return color;}
inline ImU32 Color(ImVec4 color,float alpha=1.f){
    color.w=Clamp(color.w*alpha,0.f,1.f);return ImGui::ColorConvertFloat4ToU32(color);
}
inline ImVec2 At(ImVec2 p,float x,float y){return {p.x+x,p.y+y};}
inline bool Finite(float value){
    std::uint32_t bits;std::memcpy(&bits,&value,sizeof(bits));return (bits&0x7f800000u)!=0x7f800000u;
}
inline bool Finite(double value){
    std::uint64_t bits;std::memcpy(&bits,&value,sizeof(bits));return (bits&UINT64_C(0x7ff0000000000000))!=UINT64_C(0x7ff0000000000000);
}
inline bool SurfaceValid(ImDrawList* d,ImVec2 p,ImVec2 size,float radius,float scale,float alpha){
    return d&&Finite(p.x)&&Finite(p.y)&&Finite(size.x)&&Finite(size.y)&&Finite(radius)&&Finite(scale)&&Finite(alpha)&&
        size.x>2.f&&size.y>2.f&&size.x<=100000.f&&size.y<=100000.f&&
        std::fabs(p.x)<=1000000.f&&std::fabs(p.y)<=1000000.f&&scale>0.f&&alpha>0.f;
}

// Shade only this rounded primitive's vertices. Antialias coverage is retained,
// so a translucent gradient follows the curved silhouette without square caps.
inline void Gradient(ImDrawList* d,ImVec2 p,ImVec2 size,float radius,
                     ImVec4 top,ImVec4 bottom,float alpha){
    const int first=d->VtxBuffer.Size;
    d->AddRectFilled(p,At(p,size.x,size.y),IM_COL32_WHITE,radius);
    const float inverseHeight=1.f/std::max(size.y,1.f);
    for(int i=first;i<d->VtxBuffer.Size;++i){
        ImDrawVert& vertex=d->VtxBuffer[i];
        const float coverage=((vertex.col>>IM_COL32_A_SHIFT)&255)/255.f;
        ImVec4 shade=Mix(top,bottom,Clamp((vertex.pos.y-p.y)*inverseHeight,0.f,1.f));
        vertex.col=Color(shade,alpha*coverage);
    }
}

// The body stays thin: the caller's material alpha is an upper limit. A
// diagonal reflection changes the tint, never an opaque white rectangle.
inline void LiquidBody(ImDrawList* d,ImVec2 p,ImVec2 size,float radius,
                       ImVec4 tint,bool light,float alpha){
    const int first=d->VtxBuffer.Size;
    d->AddRectFilled(p,At(p,size.x,size.y),IM_COL32_WHITE,radius);
    const ImVec4 white(1.f,1.f,1.f,1.f);
    const ImVec4 glass=tint;
    const float opacity=Clamp(tint.w,0.f,1.f);
    for(int i=first;i<d->VtxBuffer.Size;++i){
        ImDrawVert& vertex=d->VtxBuffer[i];
        const float x=Clamp((vertex.pos.x-p.x)/size.x,0.f,1.f);
        const float y=Clamp((vertex.pos.y-p.y)/size.y,0.f,1.f);
        const float diagonal=Clamp(1.f-x*.62f-y*1.22f,0.f,1.f);
        const float reflection=diagonal*diagonal;
        ImVec4 shade=Mix(glass,white,(light?.30f:.38f)*reflection);
        shade.w=opacity*(.84f+.13f*reflection);
        const float coverage=((vertex.col>>IM_COL32_A_SHIFT)&255)/255.f;
        vertex.col=Color(shade,alpha*coverage);
    }
}

// Shade the curved stroke itself to carry a highlight around the corner. The
// opposing faint dark edge gives the rim thickness without covering wallpaper.
inline void LiquidRim(ImDrawList* d,ImVec2 p,ImVec2 size,float radius,float width,
                      ImVec4 upper,ImVec4 lower,float alpha){
    if(size.x<=0.f||size.y<=0.f)return;
    const int first=d->VtxBuffer.Size;
    d->AddRect(p,At(p,size.x,size.y),IM_COL32_WHITE,radius,0,width);
    for(int i=first;i<d->VtxBuffer.Size;++i){
        ImDrawVert& vertex=d->VtxBuffer[i];
        const float x=Clamp((vertex.pos.x-p.x)/size.x,0.f,1.f);
        const float y=Clamp((vertex.pos.y-p.y)/size.y,0.f,1.f);
        const float diagonal=Clamp(y*.84f+x*.16f,0.f,1.f);
        const float coverage=((vertex.col>>IM_COL32_A_SHIFT)&255)/255.f;
        vertex.col=Color(Mix(upper,lower,diagonal),alpha*coverage);
    }
}

inline void LiquidEdges(ImDrawList* d,ImVec2 p,ImVec2 size,float radius,
                        const spatial_theme::Palette& palette,float scale,float alpha,
                        float emphasis,float pulse=0.f){
    const ImVec4 white(1.f,1.f,1.f,1.f);
    const ImVec4 shade=Mix(palette.hero,ImVec4(.035f,.045f,.06f,1.f),.54f);
    const float s=std::min(scale,std::min(size.x,size.y)*.125f);
    const float e=Clamp(emphasis,0.f,1.f);
    LiquidRim(d,p,size,radius,.85f*s,
        Opacity(white,(palette.light?.57f:.35f)+.12f*e+.018f*pulse),
        Opacity(shade,palette.light?.22f:.45f),alpha);
    const float inset=1.4f*s;
    if(size.x>2*inset&&size.y>2*inset){
        LiquidRim(d,At(p,inset,inset),{size.x-2*inset,size.y-2*inset},
            std::max(0.f,radius-inset),.65f*s,
            Opacity(white,palette.light?.14f:.085f),
            Opacity(Mix(white,palette.accent,.12f),palette.light?.38f:.22f),alpha);
    }
}

inline void Shell(ImDrawList* d,ImVec2 p,ImVec2 size,float radius,
                  const spatial_theme::Palette& palette,float scale,float alpha,
                  double time,bool motion){
    if(!SurfaceValid(d,p,size,radius,scale,alpha))return;
    const float s=Clamp(scale,.1f,4.f),a=Clamp(alpha,0.f,1.f);
    const float r=Clamp(radius,0.f,std::min(size.x,size.y)*.5f);
    const float pulse=motion&&Finite(time)?.5f+.5f*static_cast<float>(std::sin(time*.42)):.5f;
    const ImVec4 shade=Mix(palette.hero,ImVec4(.03f,.04f,.06f,1.f),.6f);
    // Only a small neutral lift shadow surrounds the glass. It cannot develop
    // bright points or saturated trails over the content behind the window.
    for(int i=3;i>=1;--i){
        const float spread=std::max(0.f,5.f*s-1.f)*i/3.f;
        d->AddRect(At(p,-spread,-spread),At(p,size.x+spread,size.y+spread),
            Color(Opacity(shade,(palette.light?.025f:.045f)*(4-i)/3.f),a),r+spread,0,.9f*s);
    }
    ImVec4 body=palette.shell;
    body.w=std::min(body.w,palette.light?.20f:.28f);
    LiquidBody(d,p,size,r,body,palette.light,a);
    LiquidEdges(d,p,size,r,palette,s,a,.22f,pulse);
}

inline void Panel(ImDrawList* d,ImVec2 p,ImVec2 size,float radius,
                  const spatial_theme::Palette& palette,float scale,float alpha,
                  float emphasis=0.f){
    if(!SurfaceValid(d,p,size,radius,scale,alpha))return;
    const float s=Clamp(scale,.1f,4.f),a=Clamp(alpha,0.f,1.f);
    const float r=Clamp(radius,0.f,std::min(size.x,size.y)*.5f);
    const float e=Finite(emphasis)?Clamp(emphasis,0.f,1.f):0.f;
    LiquidBody(d,p,size,r,palette.surface,palette.light,a);
    LiquidEdges(d,p,size,r,palette,s,a,e);
}

inline void Emblem(ImDrawList* d,ImVec2 p,float size,
                   const spatial_theme::Palette& palette,float alpha,double time,bool motion,bool surface=true){
    if(!d||size<=4.f||alpha<=0.f)return;
    const float a=Clamp(alpha,0.f,1.f),s=size/46.f,r=size*.28f;
    const ImVec4 white(1.f,1.f,1.f,1.f);
    const float pulse=motion?.5f+.5f*static_cast<float>(std::sin(time*.72)):.5f;
    if(surface)for(int i=3;i>=1;--i){
        const float spread=i*1.7f*s;
        d->AddRect(At(p,-spread,-spread),At(p,size+spread,size+spread),
            Color(Opacity(palette.accent,(.035f+.01f*pulse)*(4-i)/3.f),a),r+spread,0,1.6f*s);
    }
    if(surface)Panel(d,p,{size,size},r,palette,s,a,.025f);

    // A single continuous angular ribbon forms the S, independent of fonts.
    const ImVec2 ribbon[]={At(p,size*.72f,size*.28f),At(p,size*.42f,size*.28f),
        At(p,size*.29f,size*.405f),At(p,size*.70f,size*.595f),
        At(p,size*.575f,size*.72f),At(p,size*.28f,size*.72f)};
    d->AddPolyline(ribbon,6,Color(palette.ink,a),0,size*.105f);
    d->AddLine(At(p,size*.43f,size*.265f),At(p,size*.69f,size*.265f),
        Color(Opacity(Mix(palette.ink,white,.3f),.45f),a),.65f*s);
    d->AddCircleFilled(At(p,size*.82f,size*.79f),size*.023f,
        Color(Opacity(palette.ink,.62f),a),10);
}
} // namespace spatial_chrome

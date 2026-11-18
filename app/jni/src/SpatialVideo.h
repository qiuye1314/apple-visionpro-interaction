#pragma once
// Renderer-independent full-screen startup background. The provider owns its
// texture and receives only the current video frame, so memory stays bounded.
#include "imgui.h"
#include <algorithm>
#include <cmath>

namespace spatial_video {
constexpr float Duration=8.02f, Fps=24.f, ReducedDuration=.45f;
constexpr int Width=960,Height=540,FrameCount=192;
using TextureProvider=ImTextureID(*)(int frame);
using ReleaseProvider=void(*)();
struct State {TextureProvider texture=nullptr;ReleaseProvider release=nullptr;int lastFrame=-1;bool visible=false;};
inline State& Get(){static State state;return state;}
inline void SetProvider(TextureProvider texture,ReleaseProvider release){Get().texture=texture;Get().release=release;}
inline float Unit(float value){return std::max(0.f,std::min(1.f,value));}
inline float Smooth(float value){value=Unit(value);return value*value*(3-2*value);}
inline int FrameAt(float time,bool reduced=false){
    if(reduced)return 0;
    return std::max(0,std::min(FrameCount-1,(int)std::floor(std::max(0.f,time)*Fps)));
}
inline float OpacityAt(float time,bool reduced=false){
    if(time<0)return 0;
    if(reduced)return 1-Smooth((time-.1f)/(ReducedDuration-.1f));
    return 1-Smooth((time-(Duration-.6f))/.6f);
}
struct Cover {ImVec2 uv0{0,0},uv1{1,1};};
inline Cover CoverFor(ImVec2 screen){
    Cover result;const float imageAspect=(float)Width/Height;
    const float screenAspect=screen.x/std::max(1.f,screen.y);
    if(screenAspect>imageAspect){
        const float visible=imageAspect/screenAspect;
        result.uv0.y=(1-visible)*.5f;result.uv1.y=1-result.uv0.y;
    }else{
        const float visible=screenAspect/imageAspect;
        result.uv0.x=(1-visible)*.5f;result.uv1.x=1-result.uv0.x;
    }
    return result;
}
inline bool Draw(float time,bool reduced=false){
    auto& state=Get();state.visible=false;
    const float duration=reduced?ReducedDuration:Duration;
    if(time<0||time>=duration)return false;
    const float opacity=OpacityAt(time,reduced);if(opacity<=0)return false;
    const int frame=FrameAt(time,reduced);state.lastFrame=frame;
    ImTextureID texture=state.texture?state.texture(frame):nullptr;
    if(!texture)return false;
    ImVec2 screen=ImGui::GetIO().DisplaySize;Cover cover=CoverFor(screen);
    ImGui::GetBackgroundDrawList()->AddImage(texture,{0,0},screen,cover.uv0,cover.uv1,
        IM_COL32(255,255,255,(int)(opacity*255.f+.5f)));
    state.visible=true;return true;
}
inline void Release(){
    auto& state=Get();if(state.release)state.release();state.visible=false;state.lastFrame=-1;
}
} // namespace spatial_video

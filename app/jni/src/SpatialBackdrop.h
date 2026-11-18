#pragma once
// Configurable, deterministic screen-space backgrounds. No platform, GL, file
// watcher or input dependency; the owner supplies an optional cached texture.
#include "imgui.h"
#include "SpatialTheme.h"
#include "SpatialBackdropPresets.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cctype>

namespace spatial_backdrop {
// Keep the persisted IDs 0..6 unchanged when adding built-in pictures.
enum Scene { Snow=0, Rain=1, Stars=2, Fireflies=3, Aurora=4, Clear=5, Custom=6,
             StarryEye=7, BubbleWitch=8, BlueButterfly=9, SceneCount=10 };
constexpr unsigned RotationScenes=((1u<<SceneCount)-1u)&~(1u<<Custom);
inline bool IsPreset(int scene){return scene>=StarryEye&&scene<SceneCount;}
inline int PresetIndex(int scene){return IsPreset(scene)?scene-StarryEye:-1;}
static_assert(SceneCount-StarryEye==spatial_backdrop_presets::Count,"preset scene count");
struct Settings {
    bool enabled=true,motion=true,followTheme=true;
    int scene=Snow,mode=0;
    unsigned rotationMask=31;
    float interval=30,density=.65f,speed=1,opacity=.75f,dim=.15f,wind=.15f;
    ImVec4 top{.08f,.18f,.30f,1},bottom{.025f,.05f,.12f,1};
    char imagePath[768]{};
    int customEffect=Snow;
};
struct Runtime {
    double clock=0,cycleClock=0,lastNow=0;
    int activeScene=Snow,observedScene=-1,observedMode=-1;
    unsigned observedMask=0;
    float observedInterval=0;
    bool primed=false,wasAdvancing=false;
};
inline Settings& Get(){static Settings value;return value;}
inline Runtime& Run(){static Runtime value;return value;}
inline bool Finite(float value){std::uint32_t bits;std::memcpy(&bits,&value,sizeof(bits));return (bits&0x7f800000u)!=0x7f800000u;}
inline bool Finite(double value){std::uint64_t bits;std::memcpy(&bits,&value,sizeof(bits));return (bits&UINT64_C(0x7ff0000000000000))!=UINT64_C(0x7ff0000000000000);}
inline float Clamp(float value,float low,float high){return std::max(low,std::min(high,value));}
inline float Safe(float value,float fallback,float low,float high){return Finite(value)?Clamp(value,low,high):fallback;}
inline ImVec4 Mix(ImVec4 a,ImVec4 b,float t){return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t,a.w+(b.w-a.w)*t};}
inline void Sanitize(Settings& settings){
    settings.scene=std::max(0,std::min(SceneCount-1,settings.scene));settings.mode=settings.mode==1?1:0;
    settings.rotationMask&=RotationScenes;if(!settings.rotationMask)settings.rotationMask=1;
    settings.interval=Safe(settings.interval,30,5,600);settings.density=Safe(settings.density,.65f,0,1);
    settings.speed=Safe(settings.speed,1,.1f,3);settings.opacity=Safe(settings.opacity,.75f,0,1);
    settings.dim=Safe(settings.dim,.15f,0,.85f);settings.wind=Safe(settings.wind,.15f,-1,1);
    settings.top={Safe(settings.top.x,.08f,0,1),Safe(settings.top.y,.18f,0,1),Safe(settings.top.z,.30f,0,1),1};
    settings.bottom={Safe(settings.bottom.x,.025f,0,1),Safe(settings.bottom.y,.05f,0,1),Safe(settings.bottom.z,.12f,0,1),1};
    settings.customEffect=std::max(0,std::min(5,settings.customEffect));
    settings.imagePath[sizeof(settings.imagePath)-1]=0;
}
inline const char* SceneName(int scene){
    if(IsPreset(scene))return spatial_backdrop_presets::Name(PresetIndex(scene));
    static const char* names[]={"轻雪","细雨","星野","流萤","极光","纯净","自定义"};
    return names[std::max(0,std::min((int)Custom,scene))];
}
inline void ResetClock(){Run()=Runtime{};Run().activeScene=Get().scene;}
inline int FirstInPool(unsigned mask){mask&=RotationScenes;for(int i=0;i<SceneCount;++i)if(mask&(1u<<i))return i;return Snow;}
inline int NextInPool(int current,unsigned mask){
    mask&=RotationScenes;
    for(int step=1;step<=SceneCount;++step){const int candidate=(std::max(-1,std::min(SceneCount-1,current))+step)%SceneCount;if(mask&(1u<<candidate))return candidate;}
    return Snow;
}
inline void SelectScene(int scene){
    auto& settings=Get();settings.scene=std::max(0,std::min(SceneCount-1,scene));
    auto& runtime=Run();runtime.activeScene=settings.scene;runtime.observedScene=settings.scene;
    runtime.cycleClock=0;runtime.primed=false;runtime.wasAdvancing=false;
}
inline int CurrentScene(){
    const auto& settings=Get();const auto& runtime=Run();
    if(settings.mode!=1||runtime.observedScene!=settings.scene||runtime.observedMode!=settings.mode)return std::max(0,std::min(SceneCount-1,settings.scene));
    return std::max(0,std::min(SceneCount-1,runtime.activeScene));
}
inline float RemainingSeconds(){return Get().mode==1?(float)std::max(0.0,(double)Get().interval-Run().cycleClock):0.f;}
inline void Update(double now,bool visible,bool reduceMotion){
    auto& settings=Get();Sanitize(settings);auto& runtime=Run();
    if(runtime.observedScene!=settings.scene||runtime.observedMode!=settings.mode){
        runtime.activeScene=settings.scene;runtime.cycleClock=0;runtime.primed=false;runtime.wasAdvancing=false;
    }
    if(runtime.observedMask!=settings.rotationMask||runtime.observedInterval!=settings.interval)runtime.cycleClock=0;
    runtime.observedScene=settings.scene;runtime.observedMode=settings.mode;
    runtime.observedMask=settings.rotationMask;runtime.observedInterval=settings.interval;
    if(!Finite(now))return;
    const bool advancing=visible&&settings.enabled&&settings.motion&&!reduceMotion;
    if(!runtime.primed){runtime.lastNow=now;runtime.primed=true;runtime.wasAdvancing=advancing;return;}
    // Ignore backwards clocks. A suspended application cannot fast-forward an
    // entire weather cycle: one frame contributes at most a quarter second.
    if(now<runtime.lastNow){runtime.lastNow=now;runtime.wasAdvancing=false;return;}
    const double elapsed=std::min(.25,now-runtime.lastNow);
    runtime.lastNow=now;
    if(!advancing||!runtime.wasAdvancing){runtime.wasAdvancing=advancing;return;}
    runtime.clock+=elapsed*settings.speed;
    if(settings.mode==1){
        runtime.cycleClock+=elapsed;
        if(runtime.cycleClock+1e-8>=settings.interval){
            runtime.activeScene=NextInPool(runtime.activeScene,settings.rotationMask);runtime.cycleClock=0;
        }
    }
    runtime.wasAdvancing=advancing;
}

inline bool TrailingSpace(const char* line,int from){for(const unsigned char* p=(const unsigned char*)line+from;*p;++p)if(!std::isspace(*p))return false;return true;}
inline bool Save(const char* path){
    if(!path||!*path)return false;Settings settings=Get();Sanitize(settings);
    FILE* file=std::fopen(path,"wb");if(!file)return false;
    const int count=std::fprintf(file,"SPATIAL_BACKDROP_1\n%d %d %d %d %d %u %.9g %.9g %.9g %.9g %.9g %.9g %d\n%.9g %.9g %.9g %.9g %.9g %.9g\n",
        settings.enabled,settings.motion,settings.followTheme,settings.scene,settings.mode,settings.rotationMask,
        settings.interval,settings.density,settings.speed,settings.opacity,settings.dim,settings.wind,settings.customEffect,
        settings.top.x,settings.top.y,settings.top.z,settings.bottom.x,settings.bottom.y,settings.bottom.z);
    const size_t length=std::strlen(settings.imagePath);std::fprintf(file,"PATH %zu ",length);
    for(size_t i=0;i<length;++i)std::fprintf(file,"%02x",(unsigned char)settings.imagePath[i]);
    if(!length)std::fputc('-',file);std::fputc('\n',file);
    const bool complete=count>0&&!std::ferror(file);return std::fclose(file)==0&&complete;
}
inline int Hex(char digit){return digit>='0'&&digit<='9'?digit-'0':digit>='a'&&digit<='f'?digit-'a'+10:digit>='A'&&digit<='F'?digit-'A'+10:-1;}
inline bool Load(const char* path){
    if(!path||!*path)return false;FILE* file=std::fopen(path,"rb");if(!file)return false;
    Settings candidate;char magic[64]{},values[512]{},colors[256]{},pathLine[1664]{};
    bool ok=std::fgets(magic,sizeof(magic),file)&&std::fgets(values,sizeof(values),file)&&std::fgets(colors,sizeof(colors),file)&&std::fgets(pathLine,sizeof(pathLine),file);
    int enabled=0,motion=0,follow=0,end=0;
    if(ok){
        ok=std::strncmp(magic,"SPATIAL_BACKDROP_1",18)==0&&TrailingSpace(magic,18);
        ok=ok&&std::sscanf(values,"%d %d %d %d %d %u %f %f %f %f %f %f %d %n",&enabled,&motion,&follow,&candidate.scene,&candidate.mode,&candidate.rotationMask,
            &candidate.interval,&candidate.density,&candidate.speed,&candidate.opacity,&candidate.dim,&candidate.wind,&candidate.customEffect,&end)==13&&TrailingSpace(values,end);
        end=0;ok=ok&&std::sscanf(colors,"%f %f %f %f %f %f %n",&candidate.top.x,&candidate.top.y,&candidate.top.z,&candidate.bottom.x,&candidate.bottom.y,&candidate.bottom.z,&end)==6&&TrailingSpace(colors,end);
        ok=ok&&(enabled==0||enabled==1)&&(motion==0||motion==1)&&(follow==0||follow==1);
        candidate.enabled=enabled!=0;candidate.motion=motion!=0;candidate.followTheme=follow!=0;
        Settings sanitized=candidate;Sanitize(sanitized);
        ok=ok&&candidate.scene==sanitized.scene&&candidate.mode==sanitized.mode&&candidate.rotationMask==sanitized.rotationMask&&candidate.customEffect==sanitized.customEffect;
        const float input[]={candidate.interval,candidate.density,candidate.speed,candidate.opacity,candidate.dim,candidate.wind,candidate.top.x,candidate.top.y,candidate.top.z,candidate.bottom.x,candidate.bottom.y,candidate.bottom.z};
        const float valid[]={sanitized.interval,sanitized.density,sanitized.speed,sanitized.opacity,sanitized.dim,sanitized.wind,sanitized.top.x,sanitized.top.y,sanitized.top.z,sanitized.bottom.x,sanitized.bottom.y,sanitized.bottom.z};
        for(int i=0;i<12;++i)ok=ok&&Finite(input[i])&&input[i]==valid[i];
        unsigned length=0;char encoded[1538]{};end=0;
        ok=ok&&std::sscanf(pathLine,"PATH %u %1537s %n",&length,encoded,&end)==2&&TrailingSpace(pathLine,end)&&length<sizeof(candidate.imagePath);
        if(ok){
            if(length==0)ok=std::strcmp(encoded,"-")==0;
            else{
                ok=std::strlen(encoded)==length*2;
                for(unsigned i=0;ok&&i<length;++i){int high=Hex(encoded[i*2]),low=Hex(encoded[i*2+1]);ok=high>=0&&low>=0&&(high||low);if(ok)candidate.imagePath[i]=(char)((high<<4)|low);}
            }
            candidate.imagePath[length]=0;
        }
        int character=0;while(ok&&(character=std::fgetc(file))!=EOF)if(!std::isspace((unsigned char)character))ok=false;
    }
    const bool readError=std::ferror(file)!=0;std::fclose(file);
    if(!ok||readError)return false;Get()=candidate;ResetClock();return true;
}

inline float Random(unsigned value){value^=value>>16;value*=0x7feb352du;value^=value>>15;value*=0x846ca68bu;value^=value>>16;return (value&0x00ffffffu)/16777216.f;}
inline float Fract(float value){return value-std::floor(value);}
inline ImU32 Color(ImVec4 color,float alpha){color.w=Clamp(alpha,0,1);return ImGui::ColorConvertFloat4ToU32(color);}
inline ImVec2 Add(ImVec2 a,ImVec2 b){return {a.x+b.x,a.y+b.y};}
inline bool Inside(ImVec2 point,ImVec2 p,ImVec2 size,float radius,float pad=0){
    const float x=point.x-p.x,y=point.y-p.y,r=std::max(0.f,radius-pad);
    if(x<pad||y<pad||x>size.x-pad||y>size.y-pad)return false;
    const float cx=Clamp(x,radius,size.x-radius),cy=Clamp(y,radius,size.y-radius);
    const float dx=x-cx,dy=y-cy;return dx*dx+dy*dy<=r*r;
}
inline bool Footprint(ImVec2 center,float xRadius,float yRadius,ImVec2 p,ImVec2 size,float radius){
    return Inside({center.x-xRadius,center.y-yRadius},p,size,radius,1.6f)&&Inside({center.x+xRadius,center.y-yRadius},p,size,radius,1.6f)&&
           Inside({center.x+xRadius,center.y+yRadius},p,size,radius,1.6f)&&Inside({center.x-xRadius,center.y+yRadius},p,size,radius,1.6f);
}
inline void Gradient(ImDrawList* d,ImVec2 p,ImVec2 size,float radius,ImVec4 top,ImVec4 bottom,float alpha){
    const int first=d->VtxBuffer.Size;d->AddRectFilled(p,Add(p,size),IM_COL32_WHITE,radius);
    for(int i=first;i<d->VtxBuffer.Size;++i){auto& vertex=d->VtxBuffer[i];const float coverage=((vertex.col>>IM_COL32_A_SHIFT)&255)/255.f;
        vertex.col=Color(Mix(top,bottom,Clamp((vertex.pos.y-p.y)/std::max(1.f,size.y),0,1)),alpha*coverage);}
}
inline void Draw(ImDrawList* d,ImVec2 p,ImVec2 size,float radius,const spatial_theme::Palette& palette,float alpha,
                 ImTextureID image=nullptr,ImVec2 imageSize={0,0},int sceneOverride=-1,bool thumbnail=false){
    const auto& settings=Get();if(!d||!settings.enabled||!Finite(size.x)||!Finite(size.y)||!Finite(p.x)||!Finite(p.y)||size.x<8||size.y<8||!Finite(alpha)||alpha<=0)return;
    const float a=Clamp(alpha,0,1)*Safe(settings.opacity,.75f,0,1);if(a<=0)return;
    const float scale=Clamp(size.y/720.f,.15f,2.f),r=Safe(radius,16,0,std::min(size.x,size.y)*.5f);
    const int scene=sceneOverride>=0?std::max(0,std::min(SceneCount-1,sceneOverride)):CurrentScene();
    const int effect=scene==Custom?std::max(0,std::min(5,settings.customEffect)):IsPreset(scene)?Clear:scene;
    const float time=Finite(Run().clock)?(float)std::fmod(Run().clock,3600.):0.f;
    ImVec4 top=settings.top,bottom=settings.bottom;
    if(settings.followTheme){
        top=Mix(palette.shell,palette.accent,palette.light?.08f:.13f);bottom=Mix(palette.shell,palette.hero,palette.light?.05f:.25f);
        const ImVec4 blue(.08f,.25f,.40f,1),violet(.27f,.16f,.39f,1),green(.09f,.28f,.24f,1);
        const ImVec4 tint=effect==Aurora?violet:effect==Fireflies?green:blue;
        top=Mix(top,tint,palette.light?.055f:.12f);
    }else{
        // Keep the selected interface's foreground readable over custom colors.
        if(palette.light){top=Mix(top,ImVec4(.97f,.98f,1,1),.28f);bottom=Mix(bottom,ImVec4(.91f,.95f,.99f,1),.28f);}
        else{top=Mix(top,palette.hero,.22f);bottom=Mix(bottom,palette.hero,.34f);}
    }
    const float inset=1.75f;
    d->PushClipRect(p,Add(p,size),true);
    const bool hasImage=(scene==Custom||IsPreset(scene))&&image&&Finite(imageSize.x)&&Finite(imageSize.y)&&imageSize.x>0&&imageSize.y>0;
    if(!hasImage)Gradient(d,Add(p,{inset,inset}),{size.x-2*inset,size.y-2*inset},std::max(0.f,r-inset),top,bottom,a);
    if(hasImage){
        ImVec2 uv0(0,0),uv1(1,1);const float targetAspect=size.x/size.y,imageAspect=imageSize.x/imageSize.y;
        if(imageAspect>targetAspect){const float span=targetAspect/imageAspect;uv0.x=(1-span)*.5f;uv1.x=uv0.x+span;}
        else{const float span=imageAspect/targetAspect;uv0.y=(1-span)*.5f;uv1.y=uv0.y+span;}
        d->AddImageRounded(image,Add(p,{inset,inset}),Add(p,{size.x-inset,size.y-inset}),uv0,uv1,IM_COL32(255,255,255,(int)(a*255)),std::max(0.f,r-inset));
    }
    const ImVec4 luminous=Mix(palette.accent,ImVec4(.89f,.96f,1,1),palette.light?.18f:.78f);
    const ImVec4 particle=palette.light?Mix(palette.accent,ImVec4(.16f,.33f,.47f,1),.5f):luminous;
    const int maximum=thumbnail?32:156;
    const int count=std::max(0,std::min(maximum,(int)(Safe(settings.density,.65f,0,1)*maximum)));
    const float wind=Safe(settings.wind,.15f,-1,1);
    for(int i=0;i<count&&effect!=Clear&&effect!=Aurora;++i){
        const float one=Random(i*23u+17u),two=Random(i*47u+71u),three=Random(i*67u+137u);
        const float depth=.35f+.65f*three;
        ImVec2 point;
        if(effect==Snow){
            point={p.x+Fract(one+time*(.008f*wind+.002f*(depth-.5f))+.012f*std::sin(time*.3f+two*6.28f))*size.x,
                   p.y+Fract(two+time*(.016f+.030f*depth))*size.y};
            const float radiusParticle=(.9f+2.2f*depth)*scale;
            // Include the near-flake halo and its antialiased edge in the
            // rounded-surface test, not just the central snow particle.
            const float footprintRadius=radiusParticle*1.7f+1.5f;
            if(!Footprint(point,footprintRadius,footprintRadius,p,size,r))continue;
            if(depth>.78f)d->AddCircleFilled(point,radiusParticle*1.7f,Color(particle,a*.055f),12);
            d->AddCircleFilled(point,radiusParticle,Color(particle,a*(.21f+.44f*depth)),10);
            if(!thumbnail&&depth>.90f){
                d->AddLine({point.x-radiusParticle*1.5f,point.y},{point.x+radiusParticle*1.5f,point.y},Color(particle,a*.22f),.7f*scale);
            }
        }else if(effect==Rain){
            point={p.x+Fract(one+time*(.018f*wind+.014f))*size.x,p.y+Fract(two+time*(.18f+.28f*depth))*size.y};
            const float length=(9+23*depth)*scale,slant=(2.5f+wind*5)*scale;
            const ImVec2 center{point.x+slant*.5f,point.y+length*.5f};
            if(!Footprint(center,std::fabs(slant)*.5f+1.5f,length*.5f+1.5f,p,size,r))continue;
            d->AddLine(point,{point.x+slant,point.y+length},Color(particle,a*(.13f+.32f*depth)),(.6f+.7f*depth)*scale);
            if(i<8&&!thumbnail){
                const float phase=Fract(time*.42f+one);const float ripple=(4+phase*16)*scale;
                ImVec2 drop{p.x+(.08f+.84f*one)*size.x,p.y+(.70f+.25f*two)*size.y};
                if(Footprint(drop,ripple+1,ripple*.25f+1,p,size,r)){
                    ImVec2 ring[20];for(int k=0;k<20;++k){const float angle=k*6.2831853f/20;ring[k]={drop.x+std::cos(angle)*ripple,drop.y+std::sin(angle)*ripple*.25f};}
                    d->AddPolyline(ring,20,Color(particle,a*.17f*(1-phase)),ImDrawFlags_Closed,.8f*scale);
                }
            }
        }else if(effect==Stars){
            point={p.x+one*size.x,p.y+two*size.y};const float radiusParticle=(.65f+1.1f*depth)*scale;
            if(!Footprint(point,radiusParticle*3+1,radiusParticle*3+1,p,size,r))continue;
            const float twinkle=.42f+.58f*(.5f+.5f*std::sin(time*(.5f+depth)+one*17));
            d->AddCircleFilled(point,radiusParticle,Color(particle,a*(.25f+.50f*twinkle)),8);
            if(i%17==0){d->AddLine({point.x-radiusParticle*3,point.y},{point.x+radiusParticle*3,point.y},Color(particle,a*.28f*twinkle),.7f*scale);
                d->AddLine({point.x,point.y-radiusParticle*3},{point.x,point.y+radiusParticle*3},Color(particle,a*.28f*twinkle),.7f*scale);}
        }else if(effect==Fireflies){
            point={p.x+Fract(one+.013f*std::sin(time*.17f+two*12))*size.x,p.y+Fract(two-.008f*time+.018f*std::sin(time*.22f+one*10))*size.y};
            const float radiusParticle=(1.2f+1.8f*depth)*scale;
            if(!Footprint(point,radiusParticle*3+1,radiusParticle*3+1,p,size,r))continue;
            const float glow=.45f+.55f*(.5f+.5f*std::sin(time*.8f+three*14));
            const ImVec4 fire=Mix(particle,ImVec4(.68f,.96f,.57f,1),palette.light?.12f:.48f);
            d->AddCircleFilled(point,radiusParticle*3,Color(fire,a*.035f*glow),12);
            d->AddCircleFilled(point,radiusParticle*1.7f,Color(fire,a*.085f*glow),12);
            d->AddCircleFilled(point,radiusParticle*.6f,Color(fire,a*.65f*glow),10);
        }
    }
    if(effect==Aurora){
        const float margin=std::max(r+3.f,12.f*scale),available=std::max(1.f,size.x-2*margin);
        for(int layer=0;layer<3;++layer){
            const ImVec4 green=Mix(palette.accent,layer==1?ImVec4(.48f,.45f,.91f,1):ImVec4(.27f,.85f,.70f,1),palette.light?.25f:.55f);
            const int segments=thumbnail?18:48;
            for(int segment=0;segment<segments;++segment){
                const float x0=(float)segment/segments,x1=(float)(segment+1)/segments;
                auto y=[&](float x){return p.y+size.y*(.24f+layer*.115f+.055f*std::sin(x*6.28f+time*.12f+layer)+.025f*std::sin(x*13-time*.09f));};
                const float xA=p.x+margin+x0*available,xB=p.x+margin+x1*available;
                const float yA=y(x0),yB=y(x1),height=size.y*(.085f+.01f*std::sin(time*.11f+layer));
                const ImVec2 band[]={{xA,yA},{xB,yB},{xB,yB+height},{xA,yA+height}};
                if(Footprint({(xA+xB)*.5f,(yA+yB+height)*.5f},(xB-xA)*.5f+1,std::fabs(yB-yA)*.5f+height*.5f+1,p,size,r)){
                    const int first=d->VtxBuffer.Size;d->AddConvexPolyFilled(band,4,IM_COL32_WHITE);
                    for(int vertex=first;vertex<d->VtxBuffer.Size;++vertex){auto& v=d->VtxBuffer[vertex];const float weight=Clamp(1-(v.pos.y-yA)/std::max(1.f,height),0,1);const float coverage=((v.col>>IM_COL32_A_SHIFT)&255)/255.f;
                        v.col=Color(green,a*coverage*(palette.light?.10f:.20f)*weight*(.45f+.55f*std::sin((x0+.05f)*3.14159f)));}
                }
            }
        }
    }
    if(settings.dim>0){const ImVec4 veil=Mix(palette.surface,ImVec4(.94f,.97f,1,1),.35f);
        Gradient(d,Add(p,{inset,inset}),{size.x-2*inset,size.y-2*inset},std::max(0.f,r-inset),veil,veil,a*Safe(settings.dim,.15f,0,.85f)*.20f);}
    d->PopClipRect();
}
} // namespace spatial_backdrop

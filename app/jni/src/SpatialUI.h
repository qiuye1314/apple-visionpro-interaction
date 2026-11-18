#pragma once
// UI-only layer. Dear ImGui 1.90.x; no Android, game or driver dependencies.
#include "imgui.h"
#include "imgui_internal.h"
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <chrono>
#include "SpatialInput.h"
#include "SpatialTransform.h"
#include "SpatialCinema.h"
#include "SpatialTheme.h"
#include "SpatialChrome.h"
#include "SpatialBackdrop.h"
#include "SpatialBackdropFiles.h"
#include <string>
#include "SpatialQuotes.h"
#include "SpatialMotion.h"
#include "DrawOnlyVisuals.h"

namespace spatial {
// Wide layouts retain the established desktop geometry. Narrow viewports reflow.
inline float W=1120.f,H=720.f,PAD=24.f;
struct ResponsiveGeometry {
    bool compact=false;
    ImRect shell,header,content,nav[4],gyro,threeD,explode,theme,background,quick[3];
    float logicalWidth=1120,logicalHeight=720,defaultScale=1;
};
inline ResponsiveGeometry& ResponsiveLayout(){static ResponsiveGeometry value;return value;}
inline bool CompactLayout(){return ResponsiveLayout().compact;}
inline void SelectLayout(ImVec2 display){
    auto& layout=ResponsiveLayout();
    layout.compact=display.x<display.y*1.12f;
    W=layout.compact?384.f:1120.f;PAD=layout.compact?12.f:24.f;
    H=layout.compact?std::max(640.f,std::min(1200.f,384.f*(display.y-36.f)/std::max(240.f,display.x-24.f))):720.f;
    layout.logicalWidth=W;layout.logicalHeight=H;
}
inline void WindowFont(float pixels){
    const auto* window=ImGui::GetCurrentWindow();
    const float parent=window->ParentWindow?window->ParentWindow->FontWindowScale:1.f;
    ImGui::SetWindowFontScale(pixels/std::max(.01f,GImGui->FontBaseSize*parent));
}
enum Action { Save, Load, Initialize, Exit };
enum Glyph { Home, Display, Route, Box, Settings, Sun, Minus, Cross, Center, Arrow };
struct State {
    ImVec2 position{0,0}, display{0,0}, dragMouse{0,0}, dragPosition{0,0};
    float scale=1.f, dragScale=1.f, alpha=0.f, glow=0.f, scalePreview=100.f,layoutBaseScale=1.f;
    bool sliderEditing=false;
    bool initialized=false, depth=true, light=true, reduceMotion=false;
    bool threeD=false,exploded=false;
    bool gyroEnabled=false;
    spatial_motion::Pose gyroOffset{};
    bool detailPage=false;
    int theme=0;
    bool overlayOpen=false;
    ImRect overlayBounds;
    float layerBlend=0;
    bool wasOpen=false, pendingCenter=false;
    bool introFinished=false,introPointerDown=false,waitIntroRelease=false;
    bool multiActive=false, suppressSingle=false, touchConnected=false,orbitCandidate=false;
    float pitch=0,yaw=0,roll=0,targetPitch=0,targetYaw=0,targetRoll=0,distance=0;
    float dragPitch=0,dragYaw=0,multiLength=0,multiAngle=0,multiScale=1,multiRoll=0;
    ImVec2 multiCenter{0,0},multiPosition{0,0};
    spatial_transform::Projection dragProjection;
    spatial_input::Sample touches;
    double startTime=-1,openedAt=-1,pressedAt=0;
    int multiIds[2]={-1,-1};
    int gesture=-1;
    float scrollVelocity=0.f;
    const char* path="lsrael_draw_139_ui.cfg";
    char message[160]{};
    double messageUntil=0;
};
inline State& Get() { static State value; return value; }
inline spatial_quotes::Rotation& HomeQuotes(){static spatial_quotes::Rotation value;return value;}
inline ImRect& HomeQuoteBounds(){static ImRect value;return value;}
inline double& HomeQuoteChangedAt(){static double value=-1;return value;}
inline void UpdateHomeQuotes(double now,bool active){
    static bool seeded=false;
    auto& quotes=HomeQuotes();
    if(!seeded){
        const auto ticks=std::chrono::high_resolution_clock::now().time_since_epoch().count();
        quotes.Seed(static_cast<uint64_t>(ticks)^static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&quotes)));
        seeded=true;HomeQuoteChangedAt()=now;
    }
    const auto previous=quotes.Index();quotes.Update(now,active);
    if(quotes.Index()!=previous)HomeQuoteChangedAt()=now;
}
struct PageScrollState {
    ImGuiWindow* window=nullptr;
    ImGuiID id=0;
    ImRect clip;
    ImVec2 start;
    float startScroll=0;
    bool candidate=false,active=false;
    int page=-1;
    std::vector<ImRect> exclusions;
};
inline PageScrollState& PageScroll(){static PageScrollState value;return value;}
inline void CancelPageScroll(){
    auto& scroll=PageScroll();
    if(GImGui&&scroll.id&&GImGui->ActiveId==scroll.id)ImGui::ClearActiveID();
    scroll=PageScrollState{};
}
struct SidebarActionGeometry {ImRect hide,exit;};
inline SidebarActionGeometry& SidebarActionsLayout(){static SidebarActionGeometry value;return value;}
inline ImRect& GyroButtonBounds(){static ImRect value;return value;}
inline const spatial_theme::Palette& Theme(){return spatial_theme::Get(Get().theme);}
inline spatial_theme::Palette Material(){
    auto palette=Theme();
    // All surfaces share the glass material, including when wallpaper is off.
    // Glass carries a pale tint from the theme; it does not paint a dark tile.
    palette.surface=spatial_chrome::Mix(palette.surface,ImVec4(.94f,.97f,1.f,1.f),palette.light?.18f:.70f);
    palette.surface.w=palette.light?.16f:.12f;
    palette.shell=palette.surface;palette.shell.w=palette.light?.12f:.08f;
    palette.sidebar=palette.surface;
    palette.field=palette.surface;
    palette.hover=palette.surface;palette.hover.w=palette.light?.21f:.17f;
    palette.active=palette.surface;palette.active.w=palette.light?.26f:.22f;
    palette.hero.w=palette.surface.w;
    return palette;
}
struct BackdropImageView{ImTextureID texture=nullptr;ImVec2 size{0,0};const char* error="";};
struct BackdropImageBackend{
    bool(*load)(const char*)=nullptr;
    BackdropImageView(*view)()=nullptr;
    void(*release)()=nullptr;
    BackdropImageView(*preset)(int)=nullptr;
    std::string attempted;
    std::string pending;
    bool attemptedOnce=false;
};
inline BackdropImageBackend& BackdropImages(){static BackdropImageBackend value;return value;}
inline void InstallBackdropImages(bool(*load)(const char*),BackdropImageView(*view)(),void(*release)(),BackdropImageView(*preset)(int)=nullptr){
    auto& images=BackdropImages();images.load=load;images.view=view;images.release=release;images.preset=preset;
}
inline BackdropImageView BackdropImage(){auto& images=BackdropImages();return images.view?images.view():BackdropImageView{};}
inline BackdropImageView BackdropImageForScene(int scene){
    auto& images=BackdropImages();
    if(spatial_backdrop::IsPreset(scene))return images.preset?images.preset(spatial_backdrop::PresetIndex(scene)):BackdropImageView{};
    return scene==spatial_backdrop::Custom?BackdropImage():BackdropImageView{};
}
inline bool SaveBackdropPreferences();
inline void EnsureBackdropImage(){
    auto& images=BackdropImages();const char* path=spatial_backdrop::Get().imagePath;
    if(!images.pending.empty()){
        const std::string requested=images.pending;images.pending.clear();
        if(images.load&&images.load(requested.c_str())){
            std::snprintf(spatial_backdrop::Get().imagePath,sizeof(spatial_backdrop::Get().imagePath),"%s",requested.c_str());
            images.attempted=requested;images.attemptedOnce=true;
            spatial_backdrop::SelectScene(spatial_backdrop::Custom);spatial_backdrop::Get().mode=0;
            SaveBackdropPreferences();
        }
        return;
    }
    if(!images.load||(images.attemptedOnce&&images.attempted==path))return;
    images.attempted=path;images.attemptedOnce=true;
    if(*path)images.load(path);else if(images.release)images.release();
}
inline bool ChooseBackdropImage(const char* path){
    auto& images=BackdropImages();
    if(!images.load||!path||!*path||std::strlen(path)>=sizeof(spatial_backdrop::Get().imagePath))return false;
    images.pending=path;return true;
}
inline std::string BackdropConfigPath(){return std::string(Get().path)+".background";}
inline bool SaveBackdropPreferences(){return spatial_backdrop::Save(BackdropConfigPath().c_str());}
inline void LoadBackdropPreferences(){spatial_backdrop::Load(BackdropConfigPath().c_str());}
inline void BackdropSurface(ImDrawList* d,ImVec2 p,ImVec2 size,float radius,float alpha,int scene=-1,bool thumbnail=false){
    const auto image=BackdropImageForScene(scene<0?spatial_backdrop::CurrentScene():scene);
    spatial_backdrop::Draw(d,p,size,radius,Material(),alpha,image.texture,image.size,scene,thumbnail);
}
inline bool IntroActive(){return !Get().introFinished;}
inline bool SpatialActive(){return Get().threeD||Get().exploded;}
inline float Clamp(float v,float lo,float hi) { return std::max(lo,std::min(v,hi)); }
inline ImVec2 Add(ImVec2 a,ImVec2 b) { return {a.x+b.x,a.y+b.y}; }
inline float Ease(float current,float target,float speed=14.f) {
    return current+(target-current)*(1.f-std::exp(-speed*std::min(ImGui::GetIO().DeltaTime,.05f)));
}
inline ImU32 Color(int r,int g,int b,int a=255) {
    return IM_COL32(r,g,b,(int)(a*Get().alpha));
}
inline ImU32 Paint(ImVec4 color,float opacity=1.f){
    color.w=Clamp(color.w*Get().alpha*opacity,0.f,1.f);
    return ImGui::ColorConvertFloat4ToU32(color);
}
inline ImU32 Ink() { return Paint(Theme().ink); }
inline ImU32 Muted() { return Paint(Theme().muted); }
inline void Notice(const char* text) {
    std::snprintf(Get().message,sizeof(Get().message),"%s",text);
    Get().messageUntil=ImGui::GetTime()+2.5;
}
inline float MaximumScale() {
    ImVec2 d=ImGui::GetIO().DisplaySize;
    return std::max(.1f,std::min((d.x-16.f)/(W+2*PAD),(d.y-24.f)/(H+2*PAD)));
}
inline float DefaultScale(){
    const ImVec2 d=ImGui::GetIO().DisplaySize;
    const float fitted=CompactLayout()?MaximumScale():std::min(MaximumScale(),std::max(.65f,std::min(d.x/1600.f,d.y/1000.f)));
    return fitted*.90f;
}
inline float MinimumScale() { return std::min(MaximumScale(),CompactLayout()?DefaultScale()*.78f:std::max(.1f,DefaultScale()*.60f)); }
inline bool GlassActive(){return Get().detailPage||Get().threeD||Get().exploded;}
inline float LayerDepth(float amount){auto& s=Get();return amount*s.scale*s.layerBlend;}
inline float LayerShift(float amount){return amount*Get().scale*Get().layerBlend;}
inline spatial_transform::Projection ProjectionFor(const State& s,bool target=false,float intro=1) {
    return spatial_transform::Projection::Make({s.position.x+W*s.scale*.5f,s.position.y+H*s.scale*.5f},
        target?((s.threeD||s.exploded)?Clamp(s.targetPitch+s.gyroOffset.pitch,-65,68):0):s.pitch,
        target?((s.threeD||s.exploded)?Clamp(s.targetYaw+s.gyroOffset.yaw,-55,55):0):s.yaw,
        target?((s.threeD||s.exploded)?Clamp(s.targetRoll+s.gyroOffset.roll,-75,75):0):s.roll,
        (.92f+.08f*intro)/(1.f+s.distance*.65f),0);
}
inline ImRect ShellBounds(const State& s,const spatial_transform::Projection& projection,float split){
    auto q=[&](float x,float y){return ImVec2(s.position.x+x*s.scale,s.position.y+y*s.scale);};
    ImRect box=projection.Bounds(q(-PAD,-PAD),q(W+PAD,H+PAD));
    if(split<=0)return box;
    auto add=[&](float x,float y,float w,float h,float depth,float dx,float dy){
        box.Add(projection.AtLayer(depth*s.scale*split,dx*s.scale*split,dy*s.scale*split).Bounds(q(x,y),q(x+w,y+h)));};
    if(CompactLayout()){
        add(8,8,W-16,126,45,-14,0);add(16,140,W-32,88,95,0,-18);
        add(24,244,W-48,H-294,55,0,0);
        return box;
    }
    add(8,8,240,H-16,45,-96,12);add(270,20,W-284,84,95,18,-38);
    add(282,116,W-310,H-174,55,20,0);add(282,116,W-310,122,92,22,-8);
    float cw=W-310,card=(cw-28)/3,left=(cw-16)*.57f;
    for(int i=0;i<3;++i)add(282+i*(card+14),254,card,86,175,(i-1)*18+24,10);
    add(282,356,left,240,130,12,30);add(298+left,356,cw-left-16,240,175,42,30);
    return box;
}
inline ImRect WindowBounds() {
    if(IntroActive())return ImRect({0,0},ImGui::GetIO().DisplaySize);
    auto& s=Get();ImRect bounds=ShellBounds(s,ProjectionFor(s),s.layerBlend);
    if(s.overlayOpen)bounds.Add(s.overlayBounds);
    return bounds;
}
inline void Constrain() {
    State& s=Get(); ImVec2 d=ImGui::GetIO().DisplaySize;
    if (!std::isfinite(s.scale)) s.scale=1.f;
    s.scale=Clamp(s.scale,MinimumScale(),MaximumScale());
    const float margin=CompactLayout()?8.f:28.f;
    s.position.x=Clamp(s.position.x,margin,std::max(margin,d.x-W*s.scale-margin));
    s.position.y=Clamp(s.position.y,margin,std::max(margin,d.y-H*s.scale-margin));
    float split=std::max(s.layerBlend,s.exploded?1.f:0.f);
    auto bounds=[&]{ImRect b=ShellBounds(s,ProjectionFor(s,true),split);b.Add(ShellBounds(s,ProjectionFor(s),split));return b;};
    ImRect box=bounds();
    for(int attempt=0;attempt<4;++attempt){
        float fit=std::min((d.x-16)/std::max(1.f,box.GetWidth()),(d.y-16)/std::max(1.f,box.GetHeight()));
        if(fit>=1)break;
        float old=s.scale;s.scale=std::max(.1f,s.scale*fit*.995f);
        s.position.x+=(old-s.scale)*W*.5f;s.position.y+=(old-s.scale)*H*.5f;box=bounds();
    }
    float dx=box.Min.x<8?8-box.Min.x:box.Max.x>d.x-8?d.x-8-box.Max.x:0;
    float dy=box.Min.y<8?8-box.Min.y:box.Max.y>d.y-8?d.y-8-box.Max.y:0;
    s.position.x+=dx;s.position.y+=dy;
}
inline void Recenter() {
    State& s=Get(); ImVec2 d=ImGui::GetIO().DisplaySize;
    s.position={(d.x-W*s.scale)*.5f,(d.y-H*s.scale)*.5f}; Constrain();
    // Center the visible shell, including an existing 3D pose or separated layers.
    float split=std::max(s.layerBlend,s.exploded?1.f:0.f);
    ImRect box=ShellBounds(s,ProjectionFor(s),split);
    box.Add(ShellBounds(s,ProjectionFor(s,true),split));
    s.position.x+=d.x*.5f-box.GetCenter().x;
    s.position.y+=d.y*.5f-box.GetCenter().y;
}
inline void SetScale(float scale) {
    State& s=Get(); float old=s.scale;
    s.scale=Clamp(scale,MinimumScale(),MaximumScale());
    s.position.x+=(old-s.scale)*W*.5f; s.position.y+=(old-s.scale)*H*.5f; Constrain();
}
inline bool SavePreferences() {
    State& s=Get(); FILE* f=std::fopen(s.path,"w"); if(!f) return false;
    int n=std::fprintf(f,"SPATIAL_UI_4\n%.4f %.4f %.4f %d %d %d\n%.4f %.4f %.4f %.4f %d %d %d\n",s.scale,
        s.position.x/std::max(1.f,s.display.x),s.position.y/std::max(1.f,s.display.y),
        s.depth,Theme().light,s.reduceMotion,s.targetPitch,s.targetYaw,s.targetRoll,s.distance,s.threeD,s.exploded,s.theme);
    bool ok=n>0;
    if(std::fprintf(f,"RESPONSIVE_SCALE %.6f\n",s.scale/std::max(.01f,s.layoutBaseScale))<=0)ok=false;
    if(std::fclose(f)!=0) ok=false; return SaveBackdropPreferences()&&ok;
}
inline void SetTheme(int theme,bool persist=true){
    State& s=Get();s.theme=std::max(0,std::min(theme,spatial_theme::Count-1));s.light=Theme().light;
    draw_only_visuals::SetTheme(s.theme);
    if(persist)SavePreferences();
    char message[96];std::snprintf(message,sizeof(message),"已切换为%s主题",Theme().name);Notice(message);
}
inline bool LoadPreferences() {
    State& s=Get(); FILE* f=std::fopen(s.path,"r"); if(!f) return false;
    char magic[32]{}; float z=1,x=0,y=0,pitch=0,yaw=0,roll=0,distance=0; int bg=0,depth=1,light=1,motion=0,threeD=0,exploded=0,theme=0;
    bool hasTheme=false;
    bool ok=std::fscanf(f,"%31s",magic)==1;
    if(ok&&std::strcmp(magic,"SPATIAL_UI_1")==0)
        ok=std::fscanf(f,"%f %f %f %d %d %d %d",&z,&x,&y,&bg,&depth,&light,&motion)==7;
    else if(ok&&std::strcmp(magic,"SPATIAL_UI_2")==0)
        ok=std::fscanf(f,"%f %f %f %d %d %d %f %f %f %f",&z,&x,&y,&depth,&light,&motion,&pitch,&yaw,&roll,&distance)==10;
    else if(ok&&std::strcmp(magic,"SPATIAL_UI_3")==0)
        ok=std::fscanf(f,"%f %f %f %d %d %d %f %f %f %f %d %d",&z,&x,&y,&depth,&light,&motion,&pitch,&yaw,&roll,&distance,&threeD,&exploded)==12;
    else if(ok&&std::strcmp(magic,"SPATIAL_UI_4")==0){
        hasTheme=true;
        ok=std::fscanf(f,"%f %f %f %d %d %d %f %f %f %f %d %d %d",&z,&x,&y,&depth,&light,&motion,&pitch,&yaw,&roll,&distance,&threeD,&exploded,&theme)==13;
    }
    else ok=false;
    char responsiveMarker[32]{};float relative=0;
    if(ok&&std::fscanf(f,"%31s %f",responsiveMarker,&relative)==2&&std::strcmp(responsiveMarker,"RESPONSIVE_SCALE")==0&&std::isfinite(relative)&&relative>0)
        z=DefaultScale()*Clamp(relative,.1f,2.f);
    std::fclose(f);
    if(!ok || !std::isfinite(z) || !std::isfinite(x) || !std::isfinite(y)||!std::isfinite(pitch)||!std::isfinite(yaw)||!std::isfinite(roll)||!std::isfinite(distance)||
        (hasTheme&&(theme<0||theme>=spatial_theme::Count))) return false;
    s.scale=z; s.position={x*s.display.x,y*s.display.y};
    s.theme=hasTheme?theme:(light?0:2);
    s.depth=depth!=0; s.light=Theme().light; s.reduceMotion=motion!=0;
    s.threeD=threeD!=0;s.exploded=exploded!=0;s.layerBlend=s.exploded?1:0;
    s.targetPitch=Clamp(pitch,-65,68);s.targetYaw=Clamp(yaw,-55,55);s.targetRoll=Clamp(roll,-75,75);
    s.pitch=SpatialActive()?s.targetPitch:0;s.yaw=SpatialActive()?s.targetYaw:0;s.roll=SpatialActive()?s.targetRoll:0;s.distance=Clamp(distance,0,1.2f);
    Constrain();LoadBackdropPreferences();return true;
}
inline void Pose(float pitch,float yaw=0,float roll=0) {
    State& s=Get();s.targetPitch=Clamp(pitch,-65,68);s.targetYaw=Clamp(yaw,-55,55);s.targetRoll=Clamp(roll,-75,75);Constrain();SavePreferences();
}
inline void ResetPose() {auto& s=Get();s.distance=0;s.gyroOffset={};spatial_motion::Reset();Pose(0);Recenter();SavePreferences();}
inline void SetGyroEnabled(bool enabled){
    auto& s=Get();s.gyroEnabled=enabled;s.gyroOffset={};spatial_motion::Reset();
    if(!enabled)spatial_motion::Update(false,false,ImGui::GetTime());
    Notice(enabled?(SpatialActive()?"陀螺仪已开启，以当前姿势为基准。":"陀螺仪已开启，打开 3D 或拆解即可跟随。"):
                   "陀螺仪已关闭，恢复普通交互。");
}
inline bool OnPanel(ImVec2 p) {
    if(Get().overlayOpen&&Get().overlayBounds.Contains(p))return true;
    for(const auto& layer:spatial_transform::Get().previousHits)if(layer.rect.Contains(layer.projection.Inverse(p)))return true;
    auto& s=Get();ImVec2 q=spatial_transform::Get().previous.Inverse(p);
    return q.x>=s.position.x-PAD*s.scale&&q.x<=s.position.x+(W+PAD)*s.scale&&
        q.y>=s.position.y-PAD*s.scale&&q.y<=s.position.y+(H+PAD)*s.scale;
}
inline void BeginFrameInput(bool open) {
    State& s=Get();ImGuiIO& io=ImGui::GetIO();
    spatial_transform::Get().screenOverlay=open&&s.overlayOpen;
    if(!open){CancelPageScroll();spatial_transform::Get().previous.enabled=false;spatial_transform::Get().previousHits.clear();}
    std::vector<spatial_input::Sample> samples;s.touchConnected=spatial_input::Read(samples);
    if(IntroActive()||s.waitIntroRelease){
        s.introPointerDown=io.MouseDown[0];
        for(const auto& event:GImGui->InputEventsQueue)
            if(event.Type==ImGuiInputEventType_MouseButton&&event.MouseButton.Button==0)s.introPointerDown=event.MouseButton.Down;
        for(const auto& sample:samples){s.touches=sample;s.introPointerDown=sample.count>0;
            if(sample.count)io.AddMousePosEvent(sample.points[0].x,sample.points[0].y);
            io.AddMouseButtonEvent(0,sample.count>0);}
        s.waitIntroRelease=s.introPointerDown;
        auto& events=GImGui->InputEventsQueue;
        for(int n=events.Size-1;n>=0;--n)
            if(events[n].Type==ImGuiInputEventType_MouseWheel)events.erase(events.Data+n);
        ImGui::ClearActiveID();s.multiActive=false;s.suppressSingle=false;
        spatial_transform::Get().previous.enabled=false;spatial_transform::Get().previousHits.clear();spatial_transform::Get().inputLocked=false;spatial_transform::PrepareInput();return;
    }
    if(open&&s.overlayOpen){
        // Menus and the keypad use screen coordinates, even with the main
        // panel tilted. Multiple touches must not resize the panel beneath.
        s.multiActive=false;s.orbitCandidate=false;s.gesture=-1;
        spatial_transform::Get().inputLocked=false;
        for(const auto& sample:samples){
            if(sample.count>=2)s.suppressSingle=true;
            if(!s.suppressSingle&&sample.count==1){
                io.AddMousePosEvent(sample.points[0].x,sample.points[0].y);
                io.AddMouseButtonEvent(0,true);
            }else io.AddMouseButtonEvent(0,false);
            if(sample.count==0)s.suppressSingle=false;
            s.touches=sample;
        }
        spatial_transform::PrepareInput();return;
    }
    for(const auto& sample:samples){
        if(sample.count>=2&&open&&s.initialized){
            ImVec2 a(sample.points[0].x,sample.points[0].y),b(sample.points[1].x,sample.points[1].y);
            ImVec2 middle((a.x+b.x)*.5f,(a.y+b.y)*.5f);float dx=b.x-a.x,dy=b.y-a.y;
            float length=std::sqrt(dx*dx+dy*dy),angle=std::atan2(dy,dx);
            bool start=!s.multiActive&&OnPanel(a)&&OnPanel(b)&&length>20;
            bool ids=s.multiIds[0]==sample.points[0].id&&s.multiIds[1]==sample.points[1].id;
            if(start || (s.multiActive&&!ids)){
                s.multiActive=true;s.multiCenter=middle;s.multiPosition=s.position;s.multiScale=s.scale;
                s.multiLength=length;s.multiAngle=angle;s.multiRoll=s.targetRoll;
                s.multiIds[0]=sample.points[0].id;s.multiIds[1]=sample.points[1].id;
                ImGui::ClearActiveID();s.gesture=-1;s.orbitCandidate=false;
                // Cancel a queued first-finger click when the second finger joins
                // before NewFrame consumes it. Otherwise the pinch could toggle a setting.
                auto& events=GImGui->InputEventsQueue;
                for(int n=events.Size-1;n>=0;--n)
                    if(events[n].Type==ImGuiInputEventType_MouseButton&&events[n].MouseButton.Button==0)
                        events.erase(events.Data+n);
            }
            if(s.multiActive){
                s.scale=Clamp(s.multiScale*length/std::max(20.f,s.multiLength),MinimumScale(),MaximumScale());
                float delta=std::atan2(std::sin(angle-s.multiAngle),std::cos(angle-s.multiAngle));
                if(SpatialActive())s.roll=s.targetRoll=Clamp(s.multiRoll+delta*180/IM_PI,-75,75);
                s.position={s.multiPosition.x+middle.x-s.multiCenter.x+(s.multiScale-s.scale)*W*.5f,
                    s.multiPosition.y+middle.y-s.multiCenter.y+(s.multiScale-s.scale)*H*.5f};Constrain();
                s.suppressSingle=true;io.AddMouseButtonEvent(0,false);
            }
        }
        if(sample.count<2&&s.multiActive){s.multiActive=false;SavePreferences();}
        if(!s.suppressSingle){
            if(sample.count){io.AddMousePosEvent(sample.points[0].x,sample.points[0].y);io.AddMouseButtonEvent(0,true);}
            else io.AddMouseButtonEvent(0,false);
        } else if(sample.count==0){s.suppressSingle=false;io.AddMouseButtonEvent(0,false);}
        s.touches=sample;
    }
    spatial_transform::PrepareInput();
}
inline void FinishFrame(){spatial_transform::ApplyDrawData(ImGui::GetDrawData());}
inline void Icon(ImDrawList* d,Glyph g,ImVec2 p,float size,ImU32 c) {
    auto v=[&](float x,float y){return ImVec2(p.x+x*size,p.y+y*size);};
    auto line=[&](float x,float y,float x1,float y1){d->AddLine(v(x,y),v(x1,y1),c,1.65f*size/24.f);};
    float t=1.65f*size/24.f;
    switch(g) {
    case Home: line(.13f,.44f,.5f,.13f);line(.5f,.13f,.87f,.44f);
        line(.23f,.4f,.23f,.85f);line(.77f,.4f,.77f,.85f);line(.23f,.85f,.77f,.85f);
        d->AddRect(v(.42f,.58f),v(.59f,.85f),c,1,0,t); break;
    case Display: d->AddRect(v(.15f,.18f),v(.85f,.7f),c,size*.08f,0,t);
        line(.5f,.7f,.5f,.89f);line(.32f,.89f,.68f,.89f);d->AddCircle(v(.5f,.44f),size*.12f,c,16,t);break;
    case Route: d->AddBezierCubic(v(.21f,.8f),v(.15f,.25f),v(.85f,.8f),v(.79f,.2f),c,t,20);
        d->AddCircle(v(.21f,.8f),size*.1f,c,12,t);d->AddCircle(v(.79f,.2f),size*.1f,c,12,t);break;
    case Box: d->AddRect(v(.19f,.3f),v(.81f,.84f),c,size*.05f,0,t);
        line(.18f,.3f,.5f,.13f);line(.5f,.13f,.82f,.3f);line(.2f,.37f,.5f,.54f);
        line(.8f,.37f,.5f,.54f);line(.5f,.54f,.5f,.84f);break;
    case Settings: case Sun:
        d->AddCircle(v(.5f,.5f),size*(g==Sun?.18f:.25f),c,24,t);
        for(int i=0;i<8;++i){float a=i*IM_PI/4;float r=g==Sun?.32f:.35f;
            line(.5f+std::cos(a)*r,.5f+std::sin(a)*r,.5f+std::cos(a)*.45f,.5f+std::sin(a)*.45f);}break;
    case Minus:line(.25f,.5f,.75f,.5f);break;
    case Cross:line(.3f,.3f,.7f,.7f);line(.7f,.3f,.3f,.7f);break;
    case Center:d->AddRect(v(.2f,.2f),v(.8f,.8f),c,size*.08f,0,t);line(.38f,.5f,.62f,.5f);line(.5f,.38f,.5f,.62f);break;
    case Arrow:line(.38f,.27f,.62f,.5f);line(.62f,.5f,.38f,.73f);break;
    }
}
inline void Text(ImDrawList* d,ImVec2 p,float size,ImU32 color,const char* str) {
    d->AddText(ImGui::GetFont(),size,p,color,str);
}
inline float HoverAnimation(ImGuiID id,bool active) {
    ImGuiStorage* storage=ImGui::GetStateStorage(); float v=storage->GetFloat(id,0.f);
    v=Get().reduceMotion?(active?1.f:0.f):Ease(v,active?1.f:0.f,18.f);
    storage->SetFloat(id,v);return v;
}
inline void Shadow(ImDrawList* d,ImVec2 p,ImVec2 size,float radius,float strength=1.f) {
    if(!Get().depth) return;
    ImVec4 shade=Theme().hero;shade.w=3.f/255.f;
    for(int i=7;i>=1;--i){float v=i*2.f*Get().scale;
        d->AddRectFilled({p.x-v,p.y-v+5*Get().scale},{p.x+size.x+v,p.y+size.y+v+5*Get().scale},Paint(shade,strength),radius+v);}
}
// Compact glass rows for the independent settings pages. No extra child window,
// so content scrolling, projected hit testing and the original controls stay intact.
inline void DetailRowSurface(ImDrawList* d,ImVec2 p,ImVec2 size,float hover=0,float selected=0){
    float s=Get().scale;
    spatial_chrome::Panel(d,p,size,12*s,Material(),s,Get().alpha,.025f+.075f*hover+.08f*selected);
    if(selected>.01f){
        d->AddRectFilled({p.x+2*s,p.y+size.y*.28f},{p.x+4.5f*s,p.y+size.y*.72f},Paint(Theme().accent,.8f*selected),2*s);
        d->AddLine({p.x+16*s,p.y+size.y-s},{p.x+size.x*.45f,p.y+size.y-s},Paint(Theme().accent,.12f*selected),s);
    }
}
inline void ChildSurface(){
    auto* win=ImGui::GetCurrentWindow();auto* d=win->DrawList;
    d->PushClipRect(win->Pos,Add(win->Pos,win->Size),false);
    DetailRowSurface(d,win->Pos,win->Size);
    d->PopClipRect();
}
struct DrawingRowState {
    ImVec2 position;
    float width=0;
    ImDrawListSplitter splitter;
};
inline DrawingRowState& DrawingRow(){static DrawingRowState row;return row;}
inline void BeginDrawingRow(const char* id){
    auto& row=DrawingRow();float s=Get().scale;auto* d=ImGui::GetWindowDrawList();
    row.position=ImGui::GetCursorScreenPos();row.width=ImGui::GetContentRegionAvail().x;
    row.splitter.Split(d,2);row.splitter.SetCurrentChannel(d,1);
    ImGui::PushID(id);ImGui::SetCursorScreenPos(Add(row.position,{12*s,8*s}));ImGui::BeginGroup();
}
inline void EndDrawingRow(){
    auto& row=DrawingRow();float s=Get().scale;auto* d=ImGui::GetWindowDrawList();
    ImGui::EndGroup();float h=std::max(42*s,ImGui::GetItemRectMax().y-row.position.y+8*s);
    // Dragging a value/choice retains ownership of that control. The label on
    // the left remains a large, reliable touch target for vertical scrolling.
    PageScroll().exclusions.emplace_back(ImVec2(row.position.x+row.width*.36f,row.position.y),Add(row.position,{row.width,h}));
    row.splitter.SetCurrentChannel(d,0);DetailRowSurface(d,row.position,{row.width,h});row.splitter.Merge(d);
    ImGui::PopID();ImGui::SetCursorScreenPos(row.position);ImGui::Dummy({row.width,h});
}
inline bool DrawingSection(const char* label,bool defaultOpen=true){
    float s=Get().scale;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{12*s,7*s});
    ImGui::PushStyleColor(ImGuiCol_Header,{0,0,0,0});
    bool open=ImGui::CollapsingHeader(label,defaultOpen?ImGuiTreeNodeFlags_DefaultOpen:0);
    PageScroll().exclusions.emplace_back(ImGui::GetItemRectMin(),ImGui::GetItemRectMax());
    const auto rect=ImRect(ImGui::GetItemRectMin(),ImGui::GetItemRectMax());
    const float labelEnd=rect.Min.x+62*s+ImGui::CalcTextSize(label).x;
    if(labelEnd<rect.Max.x-30*s){
        auto* d=ImGui::GetWindowDrawList();
        d->AddLine({labelEnd,rect.GetCenter().y},{rect.Max.x-20*s,rect.GetCenter().y},Paint(Theme().border,.65f),s);
        d->AddCircleFilled({rect.Max.x-10*s,rect.GetCenter().y},2*s,Paint(Theme().accent,open?.85f:.3f),12);
    }
    ImGui::PopStyleColor();ImGui::PopStyleVar();return open;
}
inline void LayerEnd(ImDrawList* d,int first,ImVec2 p,ImVec2 size,float depth,float shiftX=0,float shiftY=0){
    spatial_transform::LayerRange(d,first,LayerDepth(depth),ImRect(p,Add(p,size)),true,LayerShift(shiftX),LayerShift(shiftY));
    if(LayerDepth(depth)<1)return;
    int bevel=d->VtxBuffer.Size;auto camera=spatial_transform::Get().frame;
    auto front=camera.AtLayer(LayerDepth(depth),LayerShift(shiftX),LayerShift(shiftY)),back=camera.AtLayer(LayerDepth(depth)-10*Get().scale,LayerShift(shiftX),LayerShift(shiftY));
    const ImVec2 corners[]={p,{p.x+size.x,p.y},Add(p,size),{p.x,p.y+size.y}};
    for(int i=0;i<4;++i){
        ImVec2 a=camera.Inverse(front.Project(corners[i])),b=camera.Inverse(back.Project(corners[i]));
        d->AddLine(a,b,Paint(Theme().border,.7f),Get().scale);
    }
    spatial_transform::LayerRange(d,bevel,0,ImRect(),false);
}
inline bool Pill(const char* id,const char* label,ImVec2 size,bool selected=false,Glyph icon=Arrow,bool useIcon=false) {
    ImVec2 p=ImGui::GetCursorScreenPos(); bool clicked=ImGui::InvisibleButton(id,size);
    bool hover=ImGui::IsItemHovered(); float a=HoverAnimation(ImGui::GetID(id),hover||ImGui::IsItemActive());
    ImDrawList* d=ImGui::GetWindowDrawList(); float s=Get().scale;
    spatial_chrome::Panel(d,p,size,size.y*.5f,Material(),s,Get().alpha,.16f+.15f*a);
    if(selected)d->AddRect(p,Add(p,size),Paint(Theme().accent,.9f),size.y*.5f,0,s);
    ImU32 text=Ink();
    if(useIcon) Icon(d,icon,{p.x+18*s,p.y+(size.y-22*s)*.5f},22*s,text);
    float fs=17*s; ImVec2 ts=ImGui::GetFont()->CalcTextSizeA(fs,10000,0,label);
    Text(d,{p.x+(useIcon?52*s:(size.x-ts.x)*.5f),p.y+(size.y-fs)*.5f},fs,text,label);
    if(selected&&useIcon)d->AddCircleFilled({p.x+size.x-19*s,p.y+size.y*.5f},2*s,text);
    return clicked;
}
inline bool Navigation(const char* id,const char* label,const char* subtitle,ImVec2 size,int number,bool selected,Glyph glyph){
    const auto p=ImGui::GetCursorScreenPos();const float s=Get().scale;
    const bool clicked=ImGui::InvisibleButton(id,size);
    const float hover=HoverAnimation(ImGui::GetID(id),ImGui::IsItemHovered()||ImGui::IsItemActive());
    auto* d=ImGui::GetWindowDrawList();
    DetailRowSurface(d,p,size,hover);
    if(selected){
        d->AddRect(p,Add(p,size),Paint(Theme().accent,.65f),17*s,0,1.25f*s);
        d->AddRectFilled(Add(p,{0,17*s}),Add(p,{3*s,size.y-17*s}),Paint(Theme().accent),2*s);
    }
    const ImVec2 iconPos=Add(p,{12*s,(size.y-36*s)*.5f});
    ImVec4 iconFill=selected?Theme().accent:Material().field;iconFill.w=Material().surface.w;
    d->AddRectFilled(iconPos,Add(iconPos,{36*s,36*s}),Paint(iconFill),11*s);
    Icon(d,glyph,Add(iconPos,{7*s,7*s}),22*s,selected?Ink():Muted());
    Text(d,Add(p,{60*s,13*s}),19*s,Ink(),label);
    Text(d,Add(p,{60*s,37*s}),10.5f*s,Muted(),subtitle);
    char index[8];std::snprintf(index,sizeof(index),"%02d",number);
    Text(d,Add(p,{size.x-29*s,16*s}),10*s,Paint(Theme().accent,selected?.95f:.4f),index);
    return clicked;
}
inline bool Toggle(const char* label,bool* value) {
    ImVec2 p=ImGui::GetCursorScreenPos();float s=Get().scale;
    bool glass=Get().detailPage;
    float w=std::max(80*s,ImGui::GetContentRegionAvail().x-(glass?0:4*s)),h=(glass?40:38)*s;
    bool pressed=ImGui::InvisibleButton(label,{w,h});if(pressed)*value=!*value;
    float t=HoverAnimation(ImGui::GetID(label),*value);
    ImDrawList* d=ImGui::GetWindowDrawList();float x=p.x+w-(glass?58:44)*s,y=p.y+(glass?8:8)*s;
    if(glass)DetailRowSurface(d,p,{w,h},ImGui::IsItemHovered()?1.f:0.f,t);
    ImVec4 track=*value?Theme().accent:Material().field;if(*value)track.w=.44f;
    ImU32 bg=Paint(track);
    d->AddRectFilled({x,y},{x+44*s,y+24*s},bg,12*s);
    d->AddRect({x,y},{x+44*s,y+24*s},Paint(*value?Theme().accent:Theme().border,.8f),12*s,0,s);
    const ImVec2 knob{x+(12+20*t)*s,y+12*s};
    d->AddCircleFilled(Add(knob,{0,1.5f*s}),9.8f*s,Paint(Theme().hero,.28f),24);
    d->AddCircleFilled(knob,9*s,Paint(*value?Theme().onAccent:Theme().ink),24);
    if(*value){
        d->AddLine(Add(knob,{-3*s,0}),Add(knob,{-1*s,2*s}),Paint(Theme().accent),1.4f*s);
        d->AddLine(Add(knob,{-1*s,2*s}),Add(knob,{3.5f*s,-2.5f*s}),Paint(Theme().accent),1.4f*s);
    }
    d->PushClipRect(p,{x-8*s,p.y+h},true);
    Text(d,{p.x+(glass?14*s:0),p.y+(glass?11:10)*s},17*s,Ink(),label);d->PopClipRect();
    return pressed;
}
// Decide drag ownership before emitting this frame's widgets. A vertical
// swipe can therefore cancel a row's pending click before its release runs.
inline void BeginPageScroll(int page) {
    auto& scroll=PageScroll();auto& state=Get();auto& io=ImGui::GetIO();auto* w=ImGui::GetCurrentWindow();
    const ImGuiID id=w->GetID("##page-pan");
    if(scroll.window!=w||scroll.page!=page){
        CancelPageScroll();
    }
    scroll.window=w;scroll.id=id;scroll.page=page;scroll.clip=w->InnerRect;scroll.clip.ClipWith(w->ClipRect);
    const bool blocked=state.overlayOpen||IntroActive()||state.waitIntroRelease||state.multiActive||state.suppressSingle;
    if(blocked){
        if(GImGui->ActiveId==id)ImGui::ClearActiveID();
        scroll.candidate=scroll.active=false;
    }else{
        if(ImGui::IsMouseClicked(0)&&w->ScrollMax.y>0&&scroll.clip.Contains(io.MousePos)&&
           ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)){
            bool protectedControl=io.MousePos.x>=w->InnerRect.Max.x-12*state.scale;
            for(const auto& rect:scroll.exclusions)if(rect.Contains(io.MousePos)){protectedControl=true;break;}
            scroll.candidate=!protectedControl;scroll.active=false;scroll.start=io.MousePos;scroll.startScroll=w->Scroll.y;
        }
        if(scroll.candidate){
            const float dx=io.MousePos.x-scroll.start.x,dy=io.MousePos.y-scroll.start.y;
            const float threshold=std::max(3.f,8*state.scale);
            // A final move and release can arrive together. Claim the gesture
            // before a row sees that release, even when MouseDown is now false.
            if(!scroll.active&&std::fabs(dy)>threshold&&std::fabs(dy)>std::fabs(dx)*.85f){
                scroll.active=true;state.orbitCandidate=false;
                ImGui::SetActiveID(id,w);ImGui::SetKeyOwner(ImGuiKey_MouseLeft,id);
            }
            if(scroll.active){
                ImGui::KeepAliveID(id);
                ImGui::SetScrollY(w,Clamp(scroll.startScroll-dy,0,w->ScrollMax.y));
            }else if(std::fabs(dx)>threshold&&std::fabs(dx)>std::fabs(dy)*1.35f)scroll.candidate=false;
            if(!io.MouseDown[0]&&!scroll.active)scroll.candidate=false;
        }
    }
    // Populate these with the current frame's numeric/choice/section geometry.
    scroll.exclusions.clear();
}
inline void ScrollPage() {
    auto& scroll=PageScroll();auto& io=ImGui::GetIO();
    if(scroll.window!=ImGui::GetCurrentWindow()||!scroll.active)return;
    // Hold the pan ID until every row has processed this frame, including the
    // release frame, so ending a swipe cannot toggle whichever row is below it.
    if(GImGui->ActiveId==scroll.id){
        if(io.MouseDown[0])ImGui::KeepAliveID(scroll.id);else ImGui::ClearActiveID();
    }
    if(!io.MouseDown[0])scroll.candidate=scroll.active=false;
}
// An empty-surface drag owns an orbit only if the press started away from a widget.
inline void BlankOrbit(){
    auto& s=Get();auto& io=ImGui::GetIO();auto* win=ImGui::GetCurrentWindow();ImGuiID id=win->GetID("##surface-orbit");
    if(!SpatialActive()||IntroActive()||s.multiActive||s.overlayOpen||PageScroll().candidate||PageScroll().active){s.orbitCandidate=false;if(GImGui->ActiveId==id)ImGui::ClearActiveID();return;}
    ImVec2 raw=spatial_transform::RawPointer();
    if(ImGui::IsMouseClicked(0)&&GImGui->HoveredId==0&&GImGui->ActiveId==0&&OnPanel(raw)){
        s.orbitCandidate=true;s.dragMouse=raw;s.dragPitch=s.targetPitch;s.dragYaw=s.targetYaw;
    }
    if(s.orbitCandidate){
        float dx=raw.x-s.dragMouse.x,dy=raw.y-s.dragMouse.y;
        if(io.MouseDown[0]&&(dx*dx+dy*dy>36*s.scale*s.scale||GImGui->ActiveId==id)){
            ImGui::SetActiveID(id,win);ImGui::KeepAliveID(id);
            s.targetPitch=Clamp(s.dragPitch-dy*.27f/s.scale,-65,68);
            s.targetYaw=Clamp(s.dragYaw+dx*.22f/s.scale,-55,55);Constrain();
        }
        if(!io.MouseDown[0]){s.orbitCandidate=false;if(GImGui->ActiveId==id){ImGui::ClearActiveID();SavePreferences();}}
    }
}
inline void ApplyStyle() {
    State& s=Get();const auto palette=Material();if(palette.light)ImGui::StyleColorsLight();else ImGui::StyleColorsDark();
    ImGuiStyle& st=ImGui::GetStyle(); st.Alpha=s.alpha;
    st.WindowPadding={18,18};st.FramePadding={12,9};st.ItemSpacing={12,12};
    st.ItemInnerSpacing={8,6};st.WindowRounding=26;st.ChildRounding=20;st.FrameRounding=10;
    st.PopupRounding=15;st.GrabRounding=9;st.ScrollbarRounding=8;st.ScrollbarSize=s.detailPage?12:7;
    st.WindowBorderSize=0;st.ChildBorderSize=0;st.FrameBorderSize=0;
    st.Colors[ImGuiCol_WindowBg]=palette.shell;
    st.Colors[ImGuiCol_ChildBg]=palette.surface;
    st.Colors[ImGuiCol_PopupBg]=palette.surface;
    st.Colors[ImGuiCol_Text]=palette.ink;
    st.Colors[ImGuiCol_TextDisabled]=palette.muted;
    st.Colors[ImGuiCol_Border]=palette.border;
    st.Colors[ImGuiCol_FrameBg]=palette.field;
    st.Colors[ImGuiCol_FrameBgHovered]=palette.hover;
    st.Colors[ImGuiCol_FrameBgActive]=palette.active;
    st.Colors[ImGuiCol_Button]=palette.field;
    st.Colors[ImGuiCol_ButtonHovered]=palette.hover;
    st.Colors[ImGuiCol_ButtonActive]=palette.active;
    st.Colors[ImGuiCol_Header]=palette.surface;
    st.Colors[ImGuiCol_HeaderHovered]=palette.hover;
    st.Colors[ImGuiCol_HeaderActive]=palette.active;
    st.Colors[ImGuiCol_SliderGrab]=palette.accent;
    st.Colors[ImGuiCol_SliderGrabActive]=palette.accent;
    st.Colors[ImGuiCol_CheckMark]=palette.accent;
    st.Colors[ImGuiCol_Separator]=palette.border;
    st.Colors[ImGuiCol_SeparatorHovered]=palette.accent;
    st.Colors[ImGuiCol_SeparatorActive]=palette.accent;
    st.Colors[ImGuiCol_ScrollbarBg]=palette.surface;
    ImVec4 scrollGrab=palette.accent;scrollGrab.w=.45f;
    st.Colors[ImGuiCol_ScrollbarGrab]=s.detailPage?scrollGrab:palette.field;
    st.Colors[ImGuiCol_ScrollbarGrabHovered]=palette.hover;
    st.Colors[ImGuiCol_ScrollbarGrabActive]=palette.accent;
    st.Colors[ImGuiCol_ResizeGrip]=palette.border;
    st.Colors[ImGuiCol_ResizeGripHovered]=palette.accent;
    st.Colors[ImGuiCol_ResizeGripActive]=palette.accent;
    st.Colors[ImGuiCol_TextSelectedBg]=palette.hover;
    st.Colors[ImGuiCol_NavHighlight]=palette.accent;
    if(s.detailPage){
        st.ItemSpacing={8,6};st.FramePadding={10,7};st.FrameRounding=10;st.ChildRounding=16;
        st.Colors[ImGuiCol_ChildBg]={0,0,0,0};
    }
    {
        st.Colors[ImGuiCol_ChildBg]=s.detailPage?ImVec4(0,0,0,0):palette.surface;
        st.Colors[ImGuiCol_ScrollbarBg].w=.10f;
    }
    st.ScaleAllSizes(s.scale);
}
#include "SpatialControls.h"
struct ThemePickerGeometry {
    ImRect popup;
    ImRect items[spatial_theme::Count];
};
inline ThemePickerGeometry& ThemePickerLayout(){static ThemePickerGeometry layout;return layout;}
inline void RenderThemePopup(){
    auto& state=Get();auto& layout=ThemePickerLayout();layout=ThemePickerGeometry{};
    if(!ImGui::IsPopupOpen("##theme-popup"))return;
    ImVec2 display=ImGui::GetIO().DisplaySize;
    float s=std::min(state.scale,std::min((display.x-16)/310.f,(display.y-16)/376.f));ImVec2 size{310*s,376*s};
    ImVec2 anchor=CompactLayout()?spatial_transform::ProjectLocal(ResponsiveLayout().theme.GetBL()):spatial_transform::ProjectLocal(Add(state.position,{788*s,92*s}));
    anchor.x=Clamp(anchor.x,8.f,std::max(8.f,display.x-size.x-8));
    anchor.y=Clamp(anchor.y,8.f,std::max(8.f,display.y-size.y-8));
    ImGui::SetNextWindowPos(anchor,ImGuiCond_Always);ImGui::SetNextWindowSize(size,ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,18*s);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,s);
    ImGui::PushStyleColor(ImGuiCol_PopupBg,{0,0,0,0});
    // This popup retains a stable ID and remains in screen space while the
    // selected page may be projected or separated into layers.
    bool begun=ImGui::BeginPopup("##theme-popup",ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoDecoration);
    if(begun){
        state.overlayOpen=true;state.overlayBounds=ImGui::GetCurrentWindow()->Rect();layout.popup=state.overlayBounds;
        ImDrawList* d=ImGui::GetWindowDrawList();ImVec2 p=ImGui::GetWindowPos();
        spatial_chrome::Panel(d,p,size,18*s,Material(),s,state.alpha,.48f);
        Text(d,Add(p,{18*s,12*s}),17*s,Ink(),"界面配色");
        Text(d,Add(p,{18*s,35*s}),10*s,Muted(),"所有页面同步 · 点选即切换");
        d->AddLine(Add(p,{16*s,53*s}),Add(p,{294*s,53*s}),Paint(Theme().border,.65f),s);
        for(int i=0;i<spatial_theme::Count;++i){
            const auto& palette=spatial_theme::Get(i);ImVec2 row=Add(p,{10*s,(59+43*i)*s});
            ImGui::SetCursorScreenPos(row);char id[32];std::snprintf(id,sizeof(id),"##theme-%d",i);
            bool click=ImGui::InvisibleButton(id,{290*s,40*s});
            layout.items[i]=ImRect(ImGui::GetItemRectMin(),ImGui::GetItemRectMax());
            bool selected=state.theme==i,hover=ImGui::IsItemHovered();
            if(selected||hover)d->AddRectFilled(row,Add(row,{290*s,40*s}),Paint(selected?Material().active:Material().hover),10*s);
            ImVec2 swatch=Add(row,{9*s,7*s});ImVec4 shell=palette.shell;shell.w=1;
            d->AddRectFilled(swatch,Add(swatch,{33*s,26*s}),Paint(shell),7*s);
            d->AddRectFilled(Add(swatch,{17*s,0}),Add(swatch,{33*s,26*s}),Paint(palette.accent),7*s,ImDrawFlags_RoundCornersRight);
            d->AddRect(swatch,Add(swatch,{33*s,26*s}),Paint(Theme().border,.9f),7*s,0,.8f*s);
            Text(d,Add(row,{53*s,4*s}),15*s,Ink(),palette.name);
            Text(d,Add(row,{53*s,24*s}),10*s,Muted(),palette.subtitle);
            if(selected){ImVec2 c=Add(row,{270*s,20*s});
                d->AddCircleFilled(c,8*s,Paint(Theme().accent),18);
                d->AddLine(Add(c,{-3.5f*s,0}),Add(c,{-1*s,3*s}),Paint(Theme().onAccent),1.6f*s);
                d->AddLine(Add(c,{-1*s,3*s}),Add(c,{4*s,-3*s}),Paint(Theme().onAccent),1.6f*s);}
            if(click){SetTheme(i);ImGui::CloseCurrentPopup();}
        }
    }
    if(begun)ImGui::EndPopup();ImGui::PopStyleColor();ImGui::PopStyleVar(3);
}
#include "SpatialBackdropStudio.inl"
inline void Gesture(const char* id,ImVec2 local,ImVec2 size,int kind,ImVec2 origin) {
    State& s=Get();ImGui::SetCursorScreenPos(Add(origin,local));
    ImGui::InvisibleButton(id,size);bool hovered=ImGui::IsItemHovered();
    if(hovered) ImGui::SetMouseCursor(kind<4?(kind==0||kind==3?ImGuiMouseCursor_ResizeNWSE:ImGuiMouseCursor_ResizeNESW):ImGuiMouseCursor_ResizeAll);
    if(ImGui::IsItemActivated()) {
        s.gesture=kind;s.dragMouse=spatial_transform::RawPointer();s.dragPosition=s.position;s.dragScale=s.scale;
        s.dragPitch=s.targetPitch;s.dragYaw=s.targetYaw;s.dragProjection=spatial_transform::Get().previous;
        s.pressedAt=ImGui::GetTime();
    }
    if(ImGui::IsItemActive()&&ImGui::GetIO().MouseDown[0]) {
        ImVec2 m=spatial_transform::RawPointer();float dx=m.x-s.dragMouse.x,dy=m.y-s.dragMouse.y;
        if(SpatialActive()&&(kind==8||kind==9)&&s.gesture!=10&&ImGui::GetTime()-s.pressedAt>.45&&dx*dx+dy*dy<36*s.scale*s.scale){
            s.gesture=10;Notice("上下拖动平放 / 竖放，左右拖动转向");
        }
        if(SpatialActive()&&(s.gesture==10 || (s.exploded&&kind==9))) {
            s.targetPitch=Clamp(s.dragPitch-dy*.27f/s.scale,-65,68);
            s.targetYaw=Clamp(s.dragYaw+dx*.22f/s.scale,-55,55);
        } else if(kind<4) {
            ImVec2 local=s.dragProjection.Inverse(m),start=s.dragProjection.Inverse(s.dragMouse);
            dx=local.x-start.x;dy=local.y-start.y;
            bool right=kind==1||kind==3,bottom=kind>=2;
            float ds=((right?dx:-dx)*W+(bottom?dy:-dy)*H)/(W*W+H*H);
            float z=Clamp(s.dragScale+ds,MinimumScale(),MaximumScale());
            s.position={s.dragPosition.x+(right?0:(s.dragScale-z)*W),s.dragPosition.y+(bottom?0:(s.dragScale-z)*H)};s.scale=z;
        } else s.position=Add(s.dragPosition,{dx,dy});
        Constrain();
    }
    if(ImGui::IsItemDeactivated()) {
        if(s.gesture==10||(s.exploded&&kind==9)){
            if(std::fabs(s.targetPitch)<5)s.targetPitch=0;
            if(std::fabs(s.targetPitch-62)<5)s.targetPitch=62;
            if(std::fabs(s.targetYaw)<5)s.targetYaw=0;
        }
        s.gesture=-1;SavePreferences();
    }
    if(kind==8 && hovered&&ImGui::IsMouseDoubleClicked(0)) {s.pendingCenter=true;}
    if(kind<4) {
        float a=HoverAnimation(ImGui::GetID(id),hovered||ImGui::IsItemActive());
        ImVec2 p=Add(origin,local);bool right=kind==1||kind==3,bottom=kind>=2;
        ImVec2 c={p.x+(right?size.x-10:10),p.y+(bottom?size.y-10:10)};
        ImU32 color=Paint(Theme().accent,.3f+.7f*a);ImDrawList* d=ImGui::GetWindowDrawList();
        d->AddLine({c.x,c.y+(bottom?-12:12)},c,color,(2+a)*s.scale);
        d->AddLine(c,{c.x+(right?-12:12),c.y},color,(2+a)*s.scale);
    }
}
template<class Page,class Command>
inline void Render(bool& open,int& tab,Page page,Command command) {
    State& state=Get(); ImGuiIO& io=ImGui::GetIO();
    tab=std::max(0,std::min(tab,3));
    UpdateHomeQuotes(ImGui::GetTime(),open&&tab==0&&state.introFinished&&!state.waitIntroRelease&&
                     !state.overlayOpen&&!state.multiActive&&!state.suppressSingle&&!io.MouseDown[0]);
    state.detailPage=tab!=0;state.light=Theme().light;
    bool cinematic=false;float cinemaTime=0;spatial_cinema::Camera cinemaCamera{};
    if(!state.initialized) {
        SelectLayout(io.DisplaySize);state.display=io.DisplaySize;state.layoutBaseScale=DefaultScale();
        state.scale=state.layoutBaseScale;LoadPreferences();LoadBackdropPreferences();Recenter();
        state.startTime=ImGui::GetTime();state.openedAt=ImGui::GetTime();state.initialized=true;
    }
    EnsureBackdropImage();
    spatial_backdrop::Update(ImGui::GetTime(),open&&state.introFinished,state.reduceMotion);
    state.light=Theme().light;draw_only_visuals::SetTheme(state.theme);
    if(state.display.x!=io.DisplaySize.x||state.display.y!=io.DisplaySize.y) {
        const float relative=state.scale/std::max(.01f,state.layoutBaseScale);
        SelectLayout(io.DisplaySize);state.display=io.DisplaySize;state.layoutBaseScale=DefaultScale();
        state.scale=state.layoutBaseScale*relative;CancelPageScroll();
        state.multiActive=false;state.orbitCandidate=false;state.gesture=-1;ImGui::ClearActiveID();Recenter();
    }
    ResponsiveLayout().defaultScale=state.layoutBaseScale;
    if(!state.introFinished){
        float elapsed=(float)(ImGui::GetTime()-state.startTime);
        float duration=state.reduceMotion?.45f:spatial_cinema::Duration;
        if(elapsed<duration){
            spatial_transform::Get().frame.enabled=false;state.alpha=0;
            cinemaCamera=spatial_cinema::Draw(elapsed,state.reduceMotion,
                {state.position.x+W*state.scale*.5f,state.position.y+H*state.scale*.5f},
                state.scale/(1+state.distance*.65f),state.pitch,state.yaw,state.roll);
            cinemaTime=elapsed;if(!open)return;
            cinematic=true;state.openedAt=ImGui::GetTime()-.6;state.wasOpen=true;
        }else{
            spatial_video::Release();
            state.introFinished=true;state.openedAt=ImGui::GetTime()-.6;state.wasOpen=open;
            ImGui::ClearActiveID();
        }
    }
    if(state.pendingCenter) {ResetPose();state.pendingCenter=false;}
    if(open&&!state.wasOpen) {Recenter();state.alpha=0.f;state.openedAt=ImGui::GetTime();}
    state.wasOpen=open;
    float entry=state.reduceMotion?1.f:Clamp((float)(ImGui::GetTime()-state.openedAt)/.6f,0,1);
    float intro=1-std::pow(1-entry,3.f);state.alpha=intro;
    if(cinematic)state.alpha=spatial_cinema::Smooth(cinemaTime/(state.reduceMotion?.2f:.65f));
    const bool motionActive=state.gyroEnabled&&open&&SpatialActive()&&state.introFinished&&!cinematic;
    const bool holdMotion=io.MouseDown[0]||state.multiActive||state.suppressSingle||state.overlayOpen||state.waitIntroRelease;
    state.gyroOffset=spatial_motion::Update(motionActive,holdMotion,ImGui::GetTime());
    if(motionActive&&spatial_motion::Get().status==spatial_motion::Status::Unavailable){
        state.gyroEnabled=false;Notice("陀螺仪暂不可用，已恢复普通交互。");
    }
    float goalPitch=SpatialActive()?Clamp(state.targetPitch+state.gyroOffset.pitch,-65,68):0,
          goalYaw=SpatialActive()?Clamp(state.targetYaw+state.gyroOffset.yaw,-55,55):0,
          goalRoll=SpatialActive()?Clamp(state.targetRoll+state.gyroOffset.roll,-75,75):0;
    state.pitch=state.reduceMotion?goalPitch:Ease(state.pitch,goalPitch,16);
    state.yaw=state.reduceMotion?goalYaw:Ease(state.yaw,goalYaw,16);
    state.roll=state.reduceMotion?goalRoll:Ease(state.roll,goalRoll,16);
    state.layerBlend=state.reduceMotion?(state.exploded?1.f:0.f):Ease(state.layerBlend,state.exploded?1.f:0.f,16);
    if(motionActive&&!holdMotion)Constrain();
    ImGuiStyle savedStyle=ImGui::GetStyle();
    if(!open) {
        // Release screen-space overlays before restoring a hidden window.
        CancelPageScroll();
        state.overlayOpen=false;state.overlayBounds=ImRect();
        spatial_transform::Get().screenOverlay=false;
        spatial_transform::Get().frame.enabled=false;state.alpha=1;ApplyStyle();
        ImGui::SetNextWindowPos({io.DisplaySize.x-160,io.DisplaySize.y-82},ImGuiCond_Always);
        ImGui::SetNextWindowSize({140,54});ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
        ImGui::Begin("##spatial-restore",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoMove);
        const ImVec2 restore=ImGui::GetCursorScreenPos();
        if(ImGui::InvisibleButton("##restore",{140,50}))open=true;
        auto* rd=ImGui::GetWindowDrawList();
        spatial_chrome::Panel(rd,restore,{140,50},17,Material(),1,state.alpha,.16f);
        spatial_chrome::Emblem(rd,Add(restore,{7,7}),36,Material(),state.alpha,ImGui::GetTime(),false,false);
        Text(rd,Add(restore,{54,9}),19,Ink(),"lsrael");
        Text(rd,Add(restore,{55,32}),10,Muted(),"展开空间");
        Icon(rd,Arrow,Add(restore,{117,17}),16,Paint(Theme().accent));
        ImGui::End();ImGui::PopStyleVar();ImGui::GetStyle()=savedStyle;return;
    }
    ApplyStyle();
    const float s=state.scale; const ImVec2 p=state.position;
    const bool compact=CompactLayout();auto& geometry=ResponsiveLayout();
    const ImVec2 size={W*s,H*s},origin={p.x-PAD*s,p.y-PAD*s};
    geometry.shell=ImRect(p,Add(p,size));
    spatial_transform::Get().frame=ProjectionFor(state,false,intro);
    if(cinematic){
        ImVec2 center(p.x+W*s*.5f,p.y+H*s*.5f);
        auto camera=spatial_transform::Projection::Make(center,cinemaCamera.pitch,cinemaCamera.yaw,cinemaCamera.roll,cinemaCamera.scale/s);
        camera.offsetX=cinemaCamera.center.x-center.x;camera.offsetY=cinemaCamera.center.y-center.y;
        spatial_transform::Get().frame=camera;
    }
    int firstOrder=GImGui->WindowsActiveCount;
    ImGui::SetNextWindowPos(origin);ImGui::SetNextWindowSize({size.x+2*PAD*s,size.y+2*PAD*s});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
    ImGui::Begin("##lsrael-spatial",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoBackground|((cinematic||state.waitIntroRelease)?ImGuiWindowFlags_NoInputs:0));
    ImGui::PopStyleVar();
    WindowFont(21*s);
    ImDrawList* d=ImGui::GetWindowDrawList();
    auto pt=[&](float x,float y){return ImVec2(p.x+x*s,p.y+y*s);};
    auto cursor=[&](float x,float y){ImGui::SetCursorScreenPos(pt(x,y));};
    auto label=[&](float x,float y,float fs,ImU32 c,const char* text){Text(d,pt(x,y),fs*s,c,text);};
    float split=state.layerBlend;
    if(split<.99f){Shadow(d,p,size,28*s,(1-split)*2.f);
        if(state.depth)spatial_chrome::Shell(d,p,size,28*s,Material(),s,state.alpha*(1-split),ImGui::GetTime(),!state.reduceMotion);
        else{
            d->AddRectFilled(p,Add(p,size),Paint(Material().shell,1-split),28*s);
            d->AddRect(p,Add(p,size),Paint(Theme().border,1-split),28*s,0,s);
        }
    }
    const float bodyLeft=compact?12:264;
    if(split>.01f){d->AddRectFilled(pt(bodyLeft,12),pt(W-8,H-12),Paint(Material().surface,split*.55f),8*s);
        d->AddRect(pt(bodyLeft,12),pt(W-8,H-12),Paint(Theme().border,split),8*s,0,s);}
    if(split<.99f)BackdropSurface(d,pt(3,3),{(W-6)*s,(H-6)*s},25*s,state.alpha*(1-split));
    if(split>.01f)BackdropSurface(d,pt(bodyLeft,12),{(W-bodyLeft-8)*s,(H-24)*s},8*s,state.alpha*split);
    ImVec2 m=io.MousePos;float dist=std::min(std::min(std::fabs(m.x-p.x),std::fabs(m.x-p.x-size.x)),std::min(std::fabs(m.y-p.y),std::fabs(m.y-p.y-size.y)));
    bool nearby=m.x>p.x-30&&m.x<p.x+size.x+30&&m.y>p.y-30&&m.y<p.y+size.y+30&&dist<30*s;
    state.glow=Ease(state.glow,nearby?1.f:0.f);
    if(state.depth)for(int i=3;i>=1;--i)d->AddRect({p.x-i*s,p.y-i*s},{p.x+size.x+i*s,p.y+size.y+i*s},Paint(Theme().border,state.glow*.35f/i),(26+i)*s,0,1.5f*s);
    const char* names[]={"主页","绘制","物资","设置"};
    const char* descriptions[]={"概览与个性化","人物与信息标记","物资与载具","显示与布局"};
    Glyph icons[]={Home,Display,Box,Settings};
    if(!compact){
    int sideStart=d->VtxBuffer.Size;
    if(split>.01f)BackdropSurface(d,pt(8,8),{240*s,(H-16)*s},(24-16*split)*s,state.alpha*split);
    spatial_chrome::LiquidEdges(d,pt(8,8),{240*s,(H-16)*s},(24-16*split)*s,Material(),s,state.alpha,.07f);
    DetailRowSurface(d,pt(22,22),{208*s,76*s});
    spatial_chrome::Emblem(d,pt(26,29),58*s,Material(),state.alpha,ImGui::GetTime(),false,false);
    label(98,38,25,Ink(),"lsrael");label(99,70,9,Muted(),"SPATIAL / 1.39");
    d->AddLine(pt(32,111),pt(220,111),Paint(Theme().border,.85f),s);
    d->AddCircleFilled(pt(36,137),2.5f*s,Paint(Theme().accent),16);
    label(46,130,11,Muted(),"功能空间");label(201,131,9,Muted(),"04");
    for(int i=0;i<3;++i){cursor(22,164+i*76);if(Navigation(names[i],names[i],descriptions[i],{208*s,64*s},i+1,tab==i,icons[i]))tab=i;
        geometry.nav[i]=ImRect(ImGui::GetItemRectMin(),ImGui::GetItemRectMax());}
    cursor(22,420);if(ImGui::InvisibleButton("##background-entry",{208*s,118*s})){
        ImGui::OpenPopup("##background-studio");state.overlayOpen=true;
    }
    BackdropLayout().entry=ImRect(ImGui::GetItemRectMin(),ImGui::GetItemRectMax());geometry.background=BackdropLayout().entry;
    BackdropSurface(d,pt(22,420),{208*s,118*s},17*s,state.alpha,-1,true);
    spatial_chrome::Panel(d,pt(22,420),{208*s,118*s},17*s,Material(),s,state.alpha,.16f);
    label(38,436,10,Muted(),"背景空间");label(38,457,24,Ink(),spatial_backdrop::SceneName(spatial_backdrop::CurrentScene()));
    label(38,507,11,Muted(),"点此调整  /  固定或轮换");
    Icon(d,Settings,pt(190,452),22*s,Paint(Theme().accent));
    cursor(22,H-155);if(Navigation("##settings","设置",descriptions[3],{208*s,64*s},4,tab==3,Settings))tab=3;
    geometry.nav[3]=ImRect(ImGui::GetItemRectMin(),ImGui::GetItemRectMax());
    cursor(22,H-82);if(Pill("##sidebar-hide","收起",{99*s,36*s},false,Minus,true))open=false;
    SidebarActionsLayout().hide=ImRect(ImGui::GetItemRectMin(),ImGui::GetItemRectMax());
    cursor(131,H-82);if(Pill("##sidebar-exit","退出",{99*s,36*s},false,Cross,true))command(Exit);
    SidebarActionsLayout().exit=ImRect(ImGui::GetItemRectMin(),ImGui::GetItemRectMax());
    label(32,H-28,9,Muted(),"LSRAEL  /  DRAWING EDITION");
    LayerEnd(d,sideStart,pt(8,8),{240*s,(H-16)*s},45,-96,12);
    int headerStart=d->VtxBuffer.Size;
    spatial_chrome::Panel(d,pt(270,20),{(W-284)*s,84*s},20*s,Material(),s,state.alpha,.2f);
    d->AddRectFilled(pt(282,36),pt(285,70),Paint(Theme().accent),2*s);
    const char* pageEyebrows[]={"OVERVIEW  /  01","DRAWING  /  02","COLLECTION  /  03","PREFERENCES  /  04"};
    label(296,31,9,Muted(),pageEyebrows[tab]);
    label(294,47,28,Ink(),names[tab]);
    label(296,83,11,Muted(),state.gyroEnabled&&SpatialActive()?"转动设备跟随视角，触摸时固定，双击横条归位。":state.exploded?"拖动空白处转动观察，双指移动与缩放。":state.threeD?"拖动横条移动，长按转动，双指缩放。":"拖动移动，点击 3D 开启空间交互。");
    Gesture("##title-drag",{(PAD+274)*s,(PAD+20)*s},{394*s,78*s},9,origin);
    cursor(682,39);if(Pill("##gyro","陀螺仪",{90*s,38*s},state.gyroEnabled))SetGyroEnabled(!state.gyroEnabled);
    GyroButtonBounds()=ImRect(ImGui::GetItemRectMin(),ImGui::GetItemRectMax());geometry.gyro=GyroButtonBounds();
    cursor(784,39);if(Pill("##three-d","3D",{78*s,38*s},state.threeD)){state.threeD=!state.threeD;Constrain();SavePreferences();}
    geometry.threeD=ImRect(ImGui::GetItemRectMin(),ImGui::GetItemRectMax());
    cursor(874,39);if(Pill("##explode","拆解",{78*s,38*s},state.exploded)){state.exploded=!state.exploded;Constrain();SavePreferences();}
    geometry.explode=ImRect(ImGui::GetItemRectMin(),ImGui::GetItemRectMax());
    cursor(970,39);if(Pill("##theme-picker",Theme().name,{128*s,38*s})){ImGui::OpenPopup("##theme-popup");state.overlayOpen=true;}
    geometry.theme=ImRect(ImGui::GetItemRectMin(),ImGui::GetItemRectMax());
    geometry.header=ImRect(pt(270,20),pt(W-14,104));
    LayerEnd(d,headerStart,pt(270,20),{(W-284)*s,84*s},95,18,-38);
    }else{
        const int sideStart=d->VtxBuffer.Size;
        spatial_chrome::LiquidEdges(d,pt(8,8),{(W-16)*s,126*s},20*s,Material(),s,state.alpha,.07f);
        DetailRowSurface(d,pt(16,14),{(W-222)*s,48*s});
        spatial_chrome::Emblem(d,pt(19,20),36*s,Material(),state.alpha,ImGui::GetTime(),false,false);
        label(64,20,20,Ink(),"lsrael");label(65,46,8,Muted(),"SPATIAL / 1.39");
        cursor(W-198,24);if(Pill("##background-entry","背景",{60*s,34*s})){
            ImGui::OpenPopup("##background-studio");state.overlayOpen=true;
        }
        BackdropLayout().entry=ImRect(ImGui::GetItemRectMin(),ImGui::GetItemRectMax());geometry.background=BackdropLayout().entry;
        cursor(W-130,24);if(Pill("##sidebar-hide","收起",{52*s,34*s}))open=false;
        SidebarActionsLayout().hide=ImRect(ImGui::GetItemRectMin(),ImGui::GetItemRectMax());
        cursor(W-70,24);if(Pill("##sidebar-exit","退出",{52*s,34*s}))command(Exit);
        SidebarActionsLayout().exit=ImRect(ImGui::GetItemRectMin(),ImGui::GetItemRectMax());
        d->AddLine(pt(20,68),pt(W-20,68),Paint(Theme().border,.65f),s);
        const float nav=(W-50)/4;
        for(int i=0;i<4;++i){cursor(16+i*(nav+6),80);
            if(Pill(i==3?"##settings":names[i],names[i],{nav*s,46*s},tab==i))tab=i;
            geometry.nav[i]=ImRect(ImGui::GetItemRectMin(),ImGui::GetItemRectMax());
        }
        LayerEnd(d,sideStart,pt(8,8),{(W-16)*s,126*s},45,-14,0);
        const int headerStart=d->VtxBuffer.Size;
        spatial_chrome::Panel(d,pt(16,140),{(W-32)*s,88*s},17*s,Material(),s,state.alpha,.14f);
        label(27,148,24,Ink(),names[tab]);label(98,158,10,Muted(),"你的空间，保持简洁。");
        Gesture("##title-drag",{(PAD+20)*s,(PAD+141)*s},{(W-40)*s,43*s},9,origin);
        cursor(24,190);if(Pill("##gyro","陀螺仪",{78*s,34*s},state.gyroEnabled))SetGyroEnabled(!state.gyroEnabled);
        geometry.gyro=GyroButtonBounds()=ImRect(ImGui::GetItemRectMin(),ImGui::GetItemRectMax());
        cursor(110,190);if(Pill("##three-d","3D",{56*s,34*s},state.threeD)){state.threeD=!state.threeD;Constrain();SavePreferences();}
        geometry.threeD=ImRect(ImGui::GetItemRectMin(),ImGui::GetItemRectMax());
        cursor(174,190);if(Pill("##explode","拆解",{64*s,34*s},state.exploded)){state.exploded=!state.exploded;Constrain();SavePreferences();}
        geometry.explode=ImRect(ImGui::GetItemRectMin(),ImGui::GetItemRectMax());
        cursor(246,190);if(Pill("##theme-picker",Theme().name,{(W-270)*s,34*s})){ImGui::OpenPopup("##theme-popup");state.overlayOpen=true;}
        geometry.theme=ImRect(ImGui::GetItemRectMin(),ImGui::GetItemRectMax());geometry.header=ImRect(pt(16,140),pt(W-16,228));
        LayerEnd(d,headerStart,pt(16,140),{(W-32)*s,88*s},95,0,-18);
    }
    const float contentX=compact?24:282,contentY=compact?244:116,contentW=compact?W-48:W-310,contentH=compact?H-294:H-174;
    cursor(contentX,contentY);geometry.content=ImRect(pt(contentX,contentY),pt(contentX+contentW,contentY+contentH));
    ImGui::PushStyleColor(ImGuiCol_ChildBg,{0,0,0,0});
    ImGui::BeginChild("##content",{contentW*s,contentH*s},false,(tab||compact)?ImGuiWindowFlags_AlwaysVerticalScrollbar:0);
    ImGui::PopStyleColor();WindowFont(21*s);
    BeginPageScroll(tab);
    if(tab==0) {
        ImVec2 start=ImGui::GetCursorScreenPos();float cw=ImGui::GetContentRegionAvail().x;ImDrawList* cd=ImGui::GetWindowDrawList();
        auto cp=[&](float x,float y){return ImVec2(start.x+x*s,start.y+y*s);};
        auto ct=[&](float x,float y,float fs,ImU32 col,const char* str){Text(cd,cp(x,y),fs*s,col,str);};
        int heroStart=cd->VtxBuffer.Size;
        ImGui::SetCursorScreenPos(start);
        const float heroHeight=compact?156.f:122.f;
        const bool quoteClicked=ImGui::InvisibleButton("##home-quote",{cw,heroHeight*s});
        HomeQuoteBounds()=ImRect(ImGui::GetItemRectMin(),ImGui::GetItemRectMax());
        const bool quoteHovered=ImGui::IsItemHovered();
        if(quoteHovered&&!cinematic&&!state.waitIntroRelease)ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if(quoteClicked&&!cinematic&&!state.waitIntroRelease&&!state.multiActive&&!state.suppressSingle&&
           !state.overlayOpen&&io.MouseDragMaxDistanceSqr[0]<=36*s*s){
            HomeQuotes().Refresh(ImGui::GetTime());HomeQuoteChangedAt()=ImGui::GetTime();
        }
        const auto& quote=HomeQuotes().Current();
        const float copyAlpha=state.reduceMotion?1.f:.65f+.35f*Clamp((float)(ImGui::GetTime()-HomeQuoteChangedAt())/.25f,0.f,1.f);
        DetailRowSurface(cd,start,{cw,heroHeight*s});
        cd->PushClipRect(start,{start.x+cw,start.y+heroHeight*s},true);
        ct(24,20,10,Paint(Theme().ink,.78f),"DRAWING EDITION / 1.39");
        ct(cw/s-72,19,11,Paint(Theme().ink,quoteHovered?.95f:.65f),"换一句");
        if(compact){
            cd->AddText(ImGui::GetFont(),24*s,cp(20,47),Paint(Theme().ink,copyAlpha),quote.title,nullptr,cw-40*s);
            cd->AddText(ImGui::GetFont(),13*s,cp(20,112),Paint(Theme().ink,.72f*copyAlpha),quote.subtitle,nullptr,cw-40*s);
        }else{
            ct(24,45,26,Paint(Theme().ink,copyAlpha),quote.title);
            ct(24,91,13,Paint(Theme().ink,.72f*copyAlpha),quote.subtitle);
        }
        cd->PopClipRect();LayerEnd(cd,heroStart,start,{cw,heroHeight*s},92,compact?0:22,compact?0:-8);
        float card=compact?cw:(cw-28*s)/3.f;
        const char* quick[]={"绘制设置","物资设置","显示设置"};
        for(int i=0;i<3;++i){ImVec2 q={compact?start.x:start.x+i*(card+14*s),start.y+(compact?170+i*98:138)*s};ImGui::SetCursorScreenPos(q);ImGui::PushID(i);
            int cardStart=cd->VtxBuffer.Size;
            bool click=ImGui::InvisibleButton("##quick",{card,86*s});geometry.quick[i]=ImRect(ImGui::GetItemRectMin(),ImGui::GetItemRectMax());float a=HoverAnimation(ImGui::GetID("##quick"),ImGui::IsItemHovered());
            DetailRowSurface(cd,q,{card,86*s},a);
            cd->AddRectFilled({q.x+16*s,q.y+23*s},{q.x+54*s,q.y+61*s},Paint(Material().field),11*s);
            Icon(cd,icons[i+1],{q.x+24*s,q.y+31*s},22*s,Ink());
            Text(cd,{q.x+66*s,q.y+23*s},17*s,Ink(),quick[i]);Text(cd,{q.x+66*s,q.y+51*s},12*s,Muted(),"点击进入");
            Icon(cd,Arrow,{q.x+card-24*s,q.y+34*s},16*s,Muted());if(click)tab=i+1;ImGui::PopID();
            LayerEnd(cd,cardStart,q,{card,86*s},175,compact?0:(i-1)*18+24,compact?0:10);}
        float left=compact?cw:(cw-16*s)*.57f,right=compact?cw:cw-left-16*s;
        const float configurationY=compact?480.f:240.f,interactionY=compact?736.f:240.f;
        ImGui::SetCursorScreenPos(cp(0,configurationY));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{10*s,6*s});
        ImGui::PushStyleColor(ImGuiCol_ChildBg,{0,0,0,0});
        ImGui::BeginChild("##configuration",{left,240*s},ImGuiChildFlags_AlwaysUseWindowPadding);
        ImGui::PopStyleColor();ChildSurface();
        WindowFont(18*s);
        ImGui::TextUnformatted("界面与配置");ImGui::Spacing();
        if(!state.sliderEditing)state.scalePreview=s*100;
        ImGui::TextDisabled("界面大小");ImGui::SameLine();ImGui::Text("%.0f%%",state.scalePreview);
        float trackWidth=ImGui::GetContentRegionAvail().x-76*s;
        if(Pill("##smaller","-",{28*s,30*s})){SetScale(s-.05f);state.scalePreview=state.scale*100;}
        ImGui::SameLine(0,10*s);ImVec2 trackPos=ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##scale",{trackWidth,30*s});
        if(ImGui::IsItemActive()) {
            float ratio=Clamp((io.MousePos.x-trackPos.x-6*s)/std::max(1.f,trackWidth-12*s),0,1);
            state.scalePreview=(MinimumScale()+ratio*(MaximumScale()-MinimumScale()))*100;
        }
        if(ImGui::IsItemDeactivated()&&state.sliderEditing)SetScale(state.scalePreview/100);
        state.sliderEditing=ImGui::IsItemActive();
        float trackRatio=Clamp((state.scalePreview/100-MinimumScale())/std::max(.001f,MaximumScale()-MinimumScale()),0,1);
        ImDrawList* sd=ImGui::GetWindowDrawList();float ty=trackPos.y+15*s,tx=trackPos.x+6*s+(trackWidth-12*s)*trackRatio;
        sd->AddLine({trackPos.x+6*s,ty},{trackPos.x+trackWidth-6*s,ty},Paint(Theme().field),3*s);
        sd->AddLine({trackPos.x+6*s,ty},{tx,ty},Paint(Theme().accent),3*s);
        sd->AddCircleFilled({tx,ty},6*s,Paint(Theme().accent),24);sd->AddCircleFilled({tx,ty},2.4f*s,Paint(Theme().onAccent),18);
        ImGui::SameLine(0,10*s);if(Pill("##larger","+",{28*s,30*s})){SetScale(s+.05f);state.scalePreview=state.scale*100;}
        ImGui::Spacing();
        float bw=(ImGui::GetContentRegionAvail().x-10*s)/2;
        if(Pill("##save","保存配置",{bw,38*s},true)){command(Save);if(!SavePreferences())Notice("界面设置保存失败，请检查目录写入权限");else Notice("配置已保存");}
        ImGui::SameLine(0,10*s);
        if(Pill("##load","读取配置",{bw,38*s})){command(Load);Notice(LoadPreferences()?"配置已读取":"功能配置已读取，暂无界面配置");}
        ImGui::Spacing();WindowFont(12*s);
        ImGui::TextDisabled("保存你的偏好，留给下次使用。");
        ImGui::EndChild();
        ImGui::SetCursorScreenPos({compact?start.x:start.x+left+16*s,start.y+interactionY*s});
        ImGui::PushStyleColor(ImGuiCol_ChildBg,{0,0,0,0});
        ImGui::BeginChild("##interaction",{right,240*s},ImGuiChildFlags_AlwaysUseWindowPadding);
        ImGui::PopStyleColor();ChildSurface();
        WindowFont(18*s);
        ImGui::TextUnformatted("设计与交互");ImGui::Spacing();
        ImGui::TextDisabled("远近距离");
        ImGui::PushItemWidth(-1);ImGui::SliderFloat("##distance",&state.distance,0,1.2f,"%.2f");
        if(ImGui::IsItemDeactivatedAfterEdit())SavePreferences();ImGui::PopItemWidth();
        Toggle("空间光效",&state.depth);Toggle("减少动态效果",&state.reduceMotion);
        WindowFont(12*s);
        ImGui::TextDisabled("长按横条旋转 · 双击复位");ImGui::EndChild();ImGui::PopStyleVar();
        ImGui::SetCursorScreenPos(cp(0,compact?998.f:502.f));
        if(ImGui::CollapsingHeader("运行控制与设备信息")&&!cinematic)page(0);
    } else if(!cinematic){
        if(tab==3){
            if(Pill("##ui-reset","姿态复位",{140*s,38*s}))state.pendingCenter=true;
            ImGui::SameLine();if(Pill("##ui-hide","收起窗口",{140*s,38*s}))open=false;
            ImGui::Spacing();ImGui::Separator();ImGui::Spacing();
        }
        page(tab);
    }
    ScrollPage();ImGui::EndChild();
    label(compact?24:286,H-34,compact?10:11,Muted(),compact?(tab?"上下滑动查看  /  点击数值输入":"上下滑动浏览  /  双指缩放"):(tab?"上下滑动查看全部功能  /  数值区域左右调节":"拖动横条移动  /  双指缩放  /  顶部 3D 开启空间交互"));
    char fps[40];std::snprintf(fps,sizeof(fps),"%.0f FPS",io.Framerate);label(W-94,H-34,11,Muted(),fps);
    // Four corner resize targets, four edge move targets and the bottom grab bar.
    float corner=38*s;float outerW=(W+2*PAD)*s,outerH=(H+2*PAD)*s;
    Gesture("##nw",{0,0},{corner,corner},0,origin);
    Gesture("##ne",{outerW-corner,0},{corner,corner},1,origin);
    Gesture("##sw",{0,outerH-corner},{corner,corner},2,origin);
    Gesture("##se",{outerW-corner,outerH-corner},{corner,corner},3,origin);
    Gesture("##top",{corner,0},{outerW-2*corner,20*s},4,origin);
    Gesture("##left",{0,corner},{20*s,outerH-2*corner},5,origin);
    Gesture("##right",{outerW-20*s,corner},{20*s,outerH-2*corner},6,origin);
    Gesture("##bottom-left",{corner,outerH-20*s},{outerW*.5f-corner-82*s,20*s},7,origin);
    Gesture("##bottom-right",{outerW*.5f+82*s,outerH-20*s},{outerW*.5f-corner-82*s,20*s},7,origin);
    // Grab bar is registered after the bottom edge but has its own non-overlapping region.
    Gesture("##grab",{(PAD+W*.5f-82)*s,(PAD+H-5)*s},{164*s,26*s},8,origin);
    ImVec2 bar=pt(W*.5f-56,H+6);
    d->AddRectFilled(Add(bar,{-10*s,-4*s}),Add(bar,{122*s,9*s}),Paint(Theme().accent,.08f),7*s);
    d->AddRectFilled(bar,Add(bar,{112*s,5*s}),Paint(Theme().accent,.85f),3*s);
    d->AddRectFilled(Add(bar,{14*s,s}),Add(bar,{98*s,2*s}),Paint(Theme().ink,.32f),s);
    BlankOrbit();state.overlayOpen=false;state.overlayBounds=ImRect();RenderThemePopup();RenderBackdropStudio();ImGui::End();
    RenderNumericEditor();
    if(NumericEditor().active){
        if(state.overlayOpen)state.overlayBounds.Add(NumericEditor().bounds);
        else state.overlayBounds=NumericEditor().bounds;
        state.overlayOpen=true;
    }
    if(state.overlayOpen)for(ImGuiWindow* popup:GImGui->Windows)
        if(popup->Active&&popup->LastFrameActive==GImGui->FrameCount&&popup->BeginOrderWithinContext>=firstOrder&&(popup->Flags&ImGuiWindowFlags_Popup))state.overlayBounds.Add(popup->Rect());
    // Both menus keep input in screen space while their page remains projected.
    spatial_transform::Get().screenOverlay=state.overlayOpen;
    ImGui::GetStyle()=savedStyle;
    for(ImGuiWindow* win:GImGui->Windows)
        if(win->Active&&win->LastFrameActive==GImGui->FrameCount&&win->BeginOrderWithinContext>=firstOrder&&!(win->Flags&ImGuiWindowFlags_Popup)&&!(win->RootWindow->Flags&ImGuiWindowFlags_Popup)&&!std::strstr(win->Name,"##spatial-number-editor")){
            spatial_transform::Get().selected.push_back(win->DrawList);
            if(win->Flags&ImGuiWindowFlags_ChildWindow){
                float depth=std::strstr(win->Name,"configuration")?130:std::strstr(win->Name,"interaction")?175:55;
                float dx=compact?0:depth==130?12:depth==175?42:20,dy=compact?0:depth==55?0:30;
                spatial_transform::ListDepth(win->DrawList,LayerDepth(depth),win->Rect(),LayerShift(dx),LayerShift(dy));
            }
        }
    if(state.messageUntil>ImGui::GetTime()) {
        ImDrawList* fg=ImGui::GetForegroundDrawList();float fs=17*s;
        ImVec2 ts=ImGui::GetFont()->CalcTextSizeA(fs,10000,0,state.message);
        ImVec2 a={(io.DisplaySize.x-ts.x)*.5f-22*s,io.DisplaySize.y-68*s};
        spatial_chrome::Panel(fg,a,{ts.x+44*s,44*s},22*s,Material(),s,state.alpha,.6f);
        Text(fg,{a.x+22*s,a.y+13*s},fs,Ink(),state.message);
    }
}
} // namespace spatial

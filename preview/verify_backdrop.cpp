#include "SpatialBackdrop.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <vector>
using namespace spatial_backdrop;
static void Check(bool value,const char* message){if(!value){std::fprintf(stderr,"FAIL %s\n",message);std::exit(2);}}
static bool Near(double one,double two){return std::fabs(one-two)<.0001;}
static void Advance(double& now,double seconds,bool visible=true,bool reduced=false){
    for(int i=0;i<(int)std::round(seconds*4);++i){now+=.25;Update(now,visible,reduced);}
}
static double InvalidDouble(std::uint64_t bits){double value;std::memcpy(&value,&bits,sizeof(value));return value;}
static void Fresh(){Get()=Settings{};ResetClock();}
struct DrawSnapshot {std::vector<ImDrawVert> vertices;bool hasImage=false;};
static DrawSnapshot RenderBackdrop(int scene,const spatial_theme::Palette& palette,bool thumbnail=false,ImTextureID image=nullptr){
    ImGui::NewFrame();auto* list=ImGui::GetForegroundDrawList();
    Draw(list,{40,40},{800,600},24,palette,1,image,{1600,900},scene,thumbnail);
    DrawSnapshot result;result.vertices.assign(list->VtxBuffer.begin(),list->VtxBuffer.end());
    if(image)for(const auto& command:list->CmdBuffer)if(command.TextureId==image&&command.ElemCount>0)result.hasImage=true;
    ImGui::Render();return result;
}
static bool SameVertices(const DrawSnapshot& one,const DrawSnapshot& two){
    if(one.vertices.size()!=two.vertices.size())return false;
    for(size_t i=0;i<one.vertices.size();++i){const auto& a=one.vertices[i];const auto& b=two.vertices[i];
        if(a.col!=b.col||a.pos.x!=b.pos.x||a.pos.y!=b.pos.y||a.uv.x!=b.uv.x||a.uv.y!=b.uv.y)return false;}
    return true;
}
int main(){
    Fresh();double now=0;Update(now,true,false);Advance(now,35);
    Check(CurrentScene()==Snow&&Near(Run().cycleClock,0),"fixed scene does not rotate");
    Check(Run().clock>30,"fixed scene still animates");
    std::puts("PASS fixed backgrounds animate without changing the selected scene");

    Fresh();Get().mode=1;Get().interval=5;Get().rotationMask=(1u<<Snow)|(1u<<Rain)|(1u<<Stars);now=0;Update(now,true,false);
    Advance(now,4.75);Check(CurrentScene()==Snow&&RemainingSeconds()<.3,"rotation waits for its full period");
    Advance(now,.25);Check(CurrentScene()==Rain&&Near(RemainingSeconds(),5),"rotation advances to enabled next scene");
    Advance(now,5);Check(CurrentScene()==Stars,"second enabled scene");Advance(now,5);Check(CurrentScene()==Snow,"wrap enabled pool");
    std::puts("PASS rotation follows the selected pool, exact interval and wrap boundary");

    Advance(now,2);const double heldClock=Run().clock,heldCycle=Run().cycleClock;
    Advance(now,80,false);Check(Near(Run().clock,heldClock)&&Near(Run().cycleClock,heldCycle),"hidden state pauses both clocks");
    now+=100;Update(now,true,false);Check(Near(Run().clock,heldClock),"first resumed frame cannot accumulate hidden gap");
    Advance(now,1);Check(Near(Run().cycleClock,heldCycle+1),"rotation resumes remaining period");
    std::puts("PASS hidden windows pause and resume without skipping weather or accumulating background time");

    const double beforeReduced=Run().clock;Advance(now,15,true,true);Check(Near(Run().clock,beforeReduced),"reduced motion freezes");
    now+=.25;Update(now,true,false);Check(Near(Run().clock,beforeReduced),"reduce motion resumes without old time");
    Get().motion=false;Advance(now,10);Check(Near(Run().clock,beforeReduced),"manual static freezes");
    Get().motion=true;now+=.25;Update(now,true,false);Advance(now,.5);Check(Run().clock>beforeReduced,"manual static resumes");
    std::puts("PASS reduced motion and the independent motion switch freeze and resume both animation and rotation");

    SelectScene(Rain);Check(Get().mode==1&&CurrentScene()==Rain&&Near(RemainingSeconds(),5),"manual choice keeps rotation mode and restarts cycle");
    now+=.25;Update(now,true,false);Advance(now,4.75);Check(CurrentScene()==Rain,"manual selection is not immediately rotated away");
    Advance(now,.25);Check(CurrentScene()==Stars,"manual choice completes a full fresh period");
    std::puts("PASS selecting a scene preserves the mode and starts a full rotation interval");

    Get().rotationMask=1u<<Rain;SelectScene(Rain);now+=.25;Update(now,true,false);Advance(now,30);
    Check(CurrentScene()==Rain,"one item pool remains stable");
    Get().rotationMask=0;Sanitize(Get());Check(Get().rotationMask==1,"empty pool falls back to snow");
    std::puts("PASS a single-scene pool stays fixed and an empty pool receives a valid fallback");

    Fresh();Get().scene=StarryEye;Get().mode=1;Get().interval=5;
    Get().rotationMask=(1u<<StarryEye)|(1u<<BubbleWitch)|(1u<<BlueButterfly);
    now=0;Update(now,true,false);Advance(now,5);Check(CurrentScene()==BubbleWitch,"rotate to second picture");
    Advance(now,5);Check(CurrentScene()==BlueButterfly,"rotate to third picture");
    Advance(now,5);Check(CurrentScene()==StarryEye,"picture pool wraps without procedural scenes");
    Check(FirstInPool((1u<<Custom)|(1u<<StarryEye))==StarryEye,"custom image excluded from pool");
    Check(NextInPool(Clear,(1u<<Custom)|(1u<<BlueButterfly))==BlueButterfly,"rotation skips custom ID");
    std::puts("PASS the three embedded pictures rotate, wrap and keep the custom image outside the built-in pool");

    const double finiteClock=Run().clock;
    Update(InvalidDouble(UINT64_C(0x7ff0000000000000)),true,false);
    Update(InvalidDouble(UINT64_C(0x7ff8000000000001)),true,false);
    Check(Near(Run().clock,finiteClock),"nonfinite timestamps ignored under fast-math");
    Update(now-100,true,false);Check(Near(Run().clock,finiteClock),"backward timestamp ignored");
    now+=10000;Update(now,true,false);Check(Near(Run().clock,finiteClock),"backwards recovery establishes baseline");
    now+=10000;Update(now,true,false);Check(Run().clock-finiteClock<=.251,"huge time gap bounded");
    std::puts("PASS invalid, backwards and suspended clocks cannot corrupt or fast-forward the animation");

    Fresh();auto& s=Get();s.scene=Custom;s.mode=1;s.followTheme=false;s.rotationMask=37;s.interval=47.5f;s.density=.77f;s.speed=1.4f;s.dim=.21f;s.wind=-.4f;s.customEffect=Rain;
    s.top={.12f,.23f,.34f,1};s.bottom={.03f,.04f,.07f,1};std::snprintf(s.imagePath,sizeof(s.imagePath),"%s","/sdcard/图片/雪 夜\\custom.jpg");
    const Settings saved=s;const char* path="verify-backdrop.cfg";Check(Save(path),"save config");Get()=Settings{};
    Check(Load(path),"load own config");
    Check(Get().scene==saved.scene&&Get().mode==saved.mode&&Get().rotationMask==saved.rotationMask&&Get().customEffect==saved.customEffect,"persist enums and pool");
    Check(Get().followTheme==saved.followTheme&&Near(Get().interval,saved.interval)&&Near(Get().density,saved.density)&&Near(Get().wind,saved.wind),"persist scalars");
    Check(Near(Get().top.y,saved.top.y)&&Near(Get().bottom.z,saved.bottom.z)&&std::strcmp(Get().imagePath,saved.imagePath)==0,"persist colors and UTF8 path including whitespace");
    std::puts("PASS settings, custom colors and UTF8 image paths round-trip through the independent text profile");

    const Settings good=Get();
    FILE* broken=std::fopen(path,"wb");Check(broken!=nullptr,"open corrupt fixture");std::fputs("SPATIAL_BACKDROP_1\n1 1 1 0 1 0 nan .5 1 .5 .1 0 0\n0 0 0 0 0 0\nPATH 0 -\n",broken);std::fclose(broken);
    Check(!Load(path),"reject malformed config");Check(std::memcmp(&good,&Get(),sizeof(good))==0,"corrupt load preserves prior settings");
    Check(!Load("missing-backdrop-profile.cfg"),"missing profile rejected");std::remove(path);
    std::puts("PASS corrupt or missing profiles leave the working background settings intact");

    for(int scene=StarryEye;scene<SceneCount;++scene){
        Fresh();Get().scene=scene;Get().rotationMask=RotationScenes;const auto preset=Get();
        Check(Save(path),"save preset profile");Fresh();Check(Load(path),"load preset profile");
        Check(Get().scene==preset.scene&&Get().rotationMask==RotationScenes&&!Get().imagePath[0],"preset persists without external path");
    }
    std::remove(path);
    std::puts("PASS each embedded-picture selection and the expanded rotation pool persist without an image path");

    Settings malformed;malformed.scene=999;malformed.mode=-4;malformed.rotationMask=0x40000000;malformed.interval=-1;malformed.density=3;malformed.opacity=-1;malformed.speed=0;malformed.customEffect=999;
    Sanitize(malformed);Check(malformed.scene==SceneCount-1&&malformed.mode==0&&malformed.rotationMask==1&&malformed.interval==5&&malformed.density==1&&malformed.opacity==0&&Near(malformed.speed,.1)&&malformed.customEffect==Clear,"bounded settings");
    Check(Footprint({50,50},2,2,{0,0},{100,100},20)&&!Footprint({1,1},2,2,{0,0},{100,100},20),"particles stay in rounded surface");
    std::puts("PASS sanitizing bounds every public control and rounded footprints reject corner leakage");

    ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={1000,800};io.DeltaTime=1.f/60.f;
    io.Fonts->AddFontDefault();unsigned char* pixels;int width,height;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
    Fresh();Get().density=0;Get().dim=0;
    for(int theme=0;theme<spatial_theme::Count;++theme){
        const auto& palette=spatial_theme::Get(theme);Get().followTheme=true;Get().customEffect=Snow;
        const auto themed=RenderBackdrop(Custom,palette);
        Check(SameVertices(themed,RenderBackdrop(Snow,palette)),"custom gradient honors the same global theme as built-in scenes");
        Get().followTheme=false;
        Check(!SameVertices(themed,RenderBackdrop(Custom,palette)),"custom gradient theme switch changes the actual rendered colors");
    }
    Get().followTheme=true;
    Check(RenderBackdrop(Custom,spatial_theme::Get(3),false,(ImTextureID)(intptr_t)123).hasImage,"themed custom backgrounds retain their image overlay");
    std::puts("PASS custom gradients follow all global palettes, respond to the toggle and retain image overlays");

    for(int scene=StarryEye;scene<SceneCount;++scene){
        Get().density=1;Get().customEffect=Rain;Run().clock=1;
        const auto picture=RenderBackdrop(scene,spatial_theme::Get(3),false,(ImTextureID)(intptr_t)234);
        Check(picture.hasImage,"embedded scene emits its image command");Run().clock=12;
        Check(SameVertices(picture,RenderBackdrop(scene,spatial_theme::Get(3),false,(ImTextureID)(intptr_t)234)),"preset is static independent of custom weather");
        Check(RenderBackdrop(scene,spatial_theme::Get(0),true,(ImTextureID)(intptr_t)234).hasImage,"light-theme thumbnail retains picture");
        Check(!RenderBackdrop(Snow,spatial_theme::Get(3),false,(ImTextureID)(intptr_t)234).hasImage,"weather scene cannot inherit preset image");
    }
    std::puts("PASS embedded scenes render real image commands and static thumbnails without leaking images into weather scenes");

    for(bool thumbnail:{false,true}){
        const auto& palette=spatial_theme::Get(3);Get().density=0;
        const auto empty=RenderBackdrop(Clear,palette,thumbnail);
        for(int scene=Snow;scene<=Fireflies;++scene){
            Check(RenderBackdrop(scene,palette,thumbnail).vertices.size()==empty.vertices.size(),"zero density emits no weather particle geometry");
            Get().density=1;
            Check(RenderBackdrop(scene,palette,thumbnail).vertices.size()>empty.vertices.size(),"nonzero density still emits weather geometry");
            Get().density=0;
        }
    }
    std::puts("PASS zero density removes every particle in full backgrounds and thumbnails while nonzero density still renders");
    ImGui::DestroyContext();
    return 0;
}

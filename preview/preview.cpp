// Native UI harness. It runs the same SpatialUI.h and legacy page widgets,
// with local demo state; no game, process, network or driver is accessed.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <array>
#include <cassert>
#include "imgui.h"
#include "imgui_internal.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#include "Main/Images/Files/stb_image.h"
#include "SpatialVideoTexture.h"
#include "SpatialUI.h"
#include "SpatialBackdropImage.h"
#include "Font.h"
#include "demo_bindings.h"
#include "draw_only_scene.inl"
struct ObservedWidget {
    std::string label;ImRect rect,clip;ImGuiWindow* window;
    ImRect minus,slider,number,plus,segments[8];int segmentCount=0;
};
static std::vector<ObservedWidget> observedWidgets;
static ObservedWidget& ObserveWidget(const char* label){
    auto* w=ImGui::GetCurrentWindow();observedWidgets.push_back({label,GImGui->LastItemData.Rect,w->ClipRect,w});
    return observedWidgets.back();
}
static void PreviewToggle(const char* label,bool* value){ModernStyleToggle(label,value);ObserveWidget(label);}
namespace ImGui {
static bool PreviewSliderFloat(const char* label,float* value,float low,float high,const char* format="%.3f",ImGuiSliderFlags flags=0){
    bool changed=SliderFloat(label,value,low,high,format,flags);ObserveWidget(label);return changed;
}
static bool PreviewCombo(const char* label,int* value,const char* const items[],int count,int height=-1){
    bool changed=Combo(label,value,items,count,height);ObserveWidget(label);return changed;
}
}
namespace spatial {
static bool PreviewNumberSlider(const char* label,float* value,float low,float high,const char* format){
    bool changed=NumberSlider(label,value,low,high,format);auto& item=ObserveWidget(label);const auto& layout=LastNumberLayout();
    item.minus=layout.minus;item.slider=layout.slider;item.number=layout.number;item.plus=layout.plus;return changed;
}
static bool PreviewChoice(const char* label,int* value,const char* const items[],int count,int height=-1){
    bool changed=Choice(label,value,items,count,height);auto& item=ObserveWidget(label);const auto& layout=LastChoiceLayout();
    item.segmentCount=layout.count;for(int i=0;i<layout.count&&i<8;++i)item.segments[i]=layout.segments[i];return changed;
}
}
// Observe the actual items emitted by the original pages without changing
// their layout, hit rectangles or application callbacks.
#define ModernStyleToggle PreviewToggle
#define SliderFloat PreviewSliderFloat
#define Combo PreviewCombo
#define NumberSlider PreviewNumberSlider
#define Choice PreviewChoice
#include "SpatialLegacyPages.inl"
#undef Choice
#undef NumberSlider
#undef Combo
#undef SliderFloat
#undef ModernStyleToggle

// Verification must also run in optimized builds; never let NDEBUG erase checks
// or the configuration operations exercised by them.
#undef assert
#define assert(condition) do { if(!(condition)){std::fprintf(stderr,"CHECK FAILED line %d: %s\n",__LINE__,#condition);std::fflush(stderr);std::exit(2);} } while(false)

static int width=1600,height=1000,tab=0;
static bool opened=true;
static GLuint atlas=0;
static HWND hwnd;
static HDC dc;
static HGLRC glContext;
static int pageCalls=0,commandCalls=0,exitCalls=0;
static bool testMode=false;
static bool sceneMode=false;
static LRESULT CALLBACK WindowProc(HWND h,UINT msg,WPARAM w,LPARAM l) {
    if(msg==WM_GETMINMAXINFO){auto* info=(MINMAXINFO*)l;info->ptMaxTrackSize={8192,8192};info->ptMaxSize={8192,8192};return 0;}
    if(ImGui::GetCurrentContext()){
        ImGuiIO& io=ImGui::GetIO();
        switch(msg){
        case WM_MOUSEMOVE:io.AddMousePosEvent((float)(short)LOWORD(l),(float)(short)HIWORD(l));return 0;
        case WM_LBUTTONDOWN:SetCapture(h);io.AddMouseButtonEvent(0,true);return 0;
        case WM_LBUTTONUP:ReleaseCapture();io.AddMouseButtonEvent(0,false);return 0;
        case WM_MOUSEWHEEL:io.AddMouseWheelEvent(0,(float)GET_WHEEL_DELTA_WPARAM(w)/WHEEL_DELTA);return 0;
        case WM_SIZE:width=LOWORD(l);height=HIWORD(l);return 0;
        case WM_KEYDOWN:if(w==VK_ESCAPE)opened=!opened;return 0;
        }
    }
    if(msg==WM_DESTROY){PostQuitMessage(0);return 0;}
    return DefWindowProcW(h,msg,w,l);
}
static void PreviewBackdrop(){
    // A host-only backdrop makes real UI translucency visible. None of these
    // vertices enter ImGui draw data or the Android overlay.
    glDisable(GL_TEXTURE_2D);glDisable(GL_BLEND);
    glBegin(GL_QUADS);
    glColor4f(.13f,.23f,.32f,1);glVertex2f(0,0);
    glColor4f(.18f,.36f,.43f,1);glVertex2f((float)width,0);
    glColor4f(.07f,.15f,.24f,1);glVertex2f((float)width,(float)height);
    glColor4f(.27f,.29f,.30f,1);glVertex2f(0,(float)height);glEnd();
    glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    auto glow=[](float x,float y,float rx,float ry,float r,float g,float b,float a){
        glBegin(GL_TRIANGLE_FAN);glColor4f(r,g,b,a);glVertex2f(x,y);
        glColor4f(r,g,b,0);
        for(int i=0;i<=80;++i){float angle=i*2*IM_PI/80;
            glVertex2f(x+std::cos(angle)*rx,y+std::sin(angle)*ry);}
        glEnd();};
    glow(width*.21f,height*.27f,width*.43f,height*.75f,.79f,.62f,.42f,.49f);
    glow(width*.79f,height*.58f,width*.56f,height*.73f,.18f,.68f,.83f,.43f);
}
static void DrawUiGL(ImDrawData* data){
    for(int n=0;n<data->CmdListsCount;++n){const ImDrawList* list=data->CmdLists[n];
        for(const ImDrawCmd& cmd:list->CmdBuffer){
            if(cmd.UserCallback){cmd.UserCallback(list,&cmd);continue;}
            glBindTexture(GL_TEXTURE_2D,(GLuint)(intptr_t)cmd.TextureId);
            glScissor((int)cmd.ClipRect.x,(int)(height-cmd.ClipRect.w),(int)(cmd.ClipRect.z-cmd.ClipRect.x),(int)(cmd.ClipRect.w-cmd.ClipRect.y));
            glBegin(GL_TRIANGLES);
            for(unsigned i=0;i<cmd.ElemCount;++i){const ImDrawVert& v=list->VtxBuffer[list->IdxBuffer[cmd.IdxOffset+i]+cmd.VtxOffset];
                unsigned c=v.col;glColor4ub(c&255,(c>>8)&255,(c>>16)&255,(c>>24)&255);
                glTexCoord2f(v.uv.x,v.uv.y);glVertex2f(v.pos.x,v.pos.y);}
            glEnd();
        }
    }
}
static void RenderGL(bool transparent=false) {
    ImDrawData* data=ImGui::GetDrawData();
    glViewport(0,0,width,height);glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_CULL_FACE);glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,width,height,0,-1,1);
    glMatrixMode(GL_MODELVIEW);glLoadIdentity();
    if(!transparent)PreviewBackdrop();
    glEnable(GL_BLEND);glEnable(GL_SCISSOR_TEST);glEnable(GL_TEXTURE_2D);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,transparent?GL_FALSE:GL_TRUE);
    DrawUiGL(data);
    if(transparent){
        // OpenGL 1.1 has no separate RGB/alpha blend factors. Accumulate alpha
        // in a second pass so overlapping glass uses A + dstA * (1 - A).
        glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_TRUE);
        glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);DrawUiGL(data);
        glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
        glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    }
    glDisable(GL_SCISSOR_TEST);glFlush();
}
static void Frame(bool render=false){
    ImGuiIO& io=ImGui::GetIO();io.DisplaySize={(float)width,(float)height};io.DeltaTime=1.f/60;
    observedWidgets.clear();
    spatial::BeginFrameInput(opened);ImGui::NewFrame();
    if(sceneMode){
        draw_only_scene::Render(spatial::Get().theme);ImGui::Render();
        if(render)RenderGL();return;
    }
    ImGuiStyle before=ImGui::GetStyle();
    spatial::Render(opened,tab,[](int page){++pageCalls;DrawSpatialPage(page);},[](spatial::Action a){
        ++commandCalls;
        if(a==spatial::Initialize)spatial::Notice("UI 交互预览");
        if(a==spatial::Exit){++exitCalls;if(!testMode)PostQuitMessage(0);}
    });
    assert(std::memcmp(&before,&ImGui::GetStyle(),sizeof(before))==0);
    assert(GImGui->ColorStack.Size==0);assert(GImGui->StyleVarStack.Size==0);
    ImGui::Render();spatial::FinishFrame();if(render)RenderGL();
}
static void Mouse(float x,float y,bool down){
    ImGuiIO& io=ImGui::GetIO();io.AddMousePosEvent(x,y);io.AddMouseButtonEvent(0,down);Frame();
}
static void Click(float x,float y){Mouse(x,y,false);Mouse(x,y,true);Mouse(x,y,false);Frame();}
static void Drag(float x,float y,float dx,float dy){Mouse(x,y,false);Mouse(x,y,true);
    for(int i=1;i<=12;++i)Mouse(x+dx*i/12,y+dy*i/12,true);
    Mouse(x+dx,y+dy,false);Frame();}
static void SavePixels(const char* file){
    std::vector<unsigned char> pixels(width*height*3);
    glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
    FILE* f=std::fopen(file,"wb");assert(f);std::fprintf(f,"P6\n%d %d\n255\n",width,height);
    for(int y=height-1;y>=0;--y)std::fwrite(pixels.data()+y*width*3,1,width*3,f);std::fclose(f);
}
static void ResizeDrawable(){
    // Resize the hidden drawable too, including portrait captures beyond desktop bounds.
    RECT r{0,0,width,height};AdjustWindowRect(&r,(DWORD)GetWindowLongPtrW(hwnd,GWL_STYLE),FALSE);
    SetWindowPos(hwnd,nullptr,0,0,r.right-r.left,r.bottom-r.top,SWP_NOACTIVATE|SWP_NOZORDER);
}
struct AlphaStats {size_t clear=0,visible=0,partial=0;};
static std::array<unsigned char,4> ReadPixel(ImVec2 point){
    std::array<unsigned char,4> pixel{};
    glReadPixels(std::max(0,std::min(width-1,(int)point.x)),
        std::max(0,std::min(height-1,height-1-(int)point.y)),1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel.data());
    return pixel;
}
static AlphaStats SaveTransparentPixels(const char* file){
    std::vector<unsigned char> pixels((size_t)width*height*4);
    glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    AlphaStats stats;
    for(size_t i=0;i<pixels.size();i+=4){unsigned a=pixels[i+3];
        if(a==0)++stats.clear;else ++stats.visible;
        if(a>0&&a<255)++stats.partial;
        // RGB accumulated against transparent black is premultiplied; TGA
        // stores straight alpha, so undo premultiplication before export.
        for(int c=0;c<3;++c)pixels[i+c]=a?(unsigned char)std::min(255u,(pixels[i+c]*255u+a/2)/a):0;
        std::swap(pixels[i],pixels[i+2]);
    }
    unsigned char header[18]{};header[2]=2;header[12]=(unsigned char)(width&255);header[13]=(unsigned char)(width>>8);
    header[14]=(unsigned char)(height&255);header[15]=(unsigned char)(height>>8);header[16]=32;header[17]=8;
    FILE* f=std::fopen(file,"wb");assert(f);assert(std::fwrite(header,1,sizeof(header),f)==sizeof(header));
    assert(std::fwrite(pixels.data(),1,pixels.size(),f)==pixels.size());assert(std::fclose(f)==0);return stats;
}
static AlphaStats CaptureTransparent(const char* file){
    ResizeDrawable();Mouse(-100,-100,false);Frame();Frame();RenderGL(true);return SaveTransparentPixels(file);
}
static void Capture(const char* file){
    ResizeDrawable();
    Mouse(-100,-100,false);Frame();
    Frame(true);SavePixels(file);
}
static ImVec2 Point(float x,float y){auto& s=spatial::Get();return spatial_transform::ProjectLocal({s.position.x+x*s.scale,s.position.y+y*s.scale});}
static void ClickAt(float x,float y){ImVec2 p=Point(x,y);Click(p.x,p.y);}
static void Settle(){for(int i=0;i<80;++i)Frame();}
static void LongRotate(float dx,float dy){
    ImVec2 p=Point(560,730);Mouse(p.x,p.y,false);Mouse(p.x,p.y,true);
    for(int i=0;i<35;++i)Frame();assert(spatial::Get().gesture==10);
    for(int i=1;i<=12;++i)Mouse(p.x+dx*i/12,p.y+dy*i/12,true);
    Mouse(p.x+dx,p.y+dy,false);Settle();
}
static void CheckCentered(){
    auto& s=spatial::Get();float split=std::max(s.layerBlend,s.exploded?1.f:0.f);
    ImRect bounds=spatial::ShellBounds(s,spatial::ProjectionFor(s),split);
    bounds.Add(spatial::ShellBounds(s,spatial::ProjectionFor(s,true),split));
    ImVec2 center=bounds.GetCenter();
    assert(std::fabs(center.x-width*.5f)<1.f&&std::fabs(center.y-height*.5f)<1.f);
}
static ObservedWidget FindWidget(const char* label){
    for(auto& widget:observedWidgets)if(widget.label==label)return widget;
    std::fprintf(stderr,"Widget missing: %s\n",label);assert(false);return {};
}
static ObservedWidget ShowWidget(const char* label){
    auto widget=FindWidget(label);float delta=widget.rect.GetCenter().y-widget.clip.GetCenter().y;
    if(!widget.clip.Contains(widget.rect.GetCenter())){
        ImGui::SetScrollY(widget.window,widget.window->Scroll.y+delta);Frame();Frame();widget=FindWidget(label);
    }
    assert(widget.clip.Contains(widget.rect.GetCenter()));return widget;
}
static void ClickWidget(const char* label,float fraction=.5f){
    auto widget=ShowWidget(label);ImVec2 p={widget.rect.Min.x+widget.rect.GetWidth()*fraction,widget.rect.GetCenter().y};
    p=spatial_transform::ProjectLocal(p);Click(p.x,p.y);
}
static void ClickRect(const ImRect& rect,bool project=true,float fraction=.5f){
    assert(rect.GetWidth()>0&&rect.GetHeight()>0);
    ImVec2 p={rect.Min.x+rect.GetWidth()*fraction,rect.GetCenter().y};
    if(project)p=spatial_transform::ProjectLocal(p);Click(p.x,p.y);
}
static void ClickNumberTrack(const char* label,float fraction){ClickRect(ShowWidget(label).slider,true,fraction);}
static void ClickNumberButton(const char* label,int button){
    auto widget=ShowWidget(label);ClickRect(button<0?widget.minus:button>0?widget.plus:widget.number);
}
static void TouchPoint(ImVec2 point,bool down){
    spatial_input::Sample sample;sample.count=down?1:0;
    sample.points[0]={point.x,point.y,21};spatial_input::Publish(sample);Frame();Frame();
}
static void TouchSwipe(ImVec2 local,float dx,float dy){
    ImVec2 start=spatial_transform::ProjectLocal(local);TouchPoint(start,false);TouchPoint(start,true);
    for(int i=1;i<=14;++i){
        ImVec2 target={local.x+dx*i/14,local.y+dy*i/14};
        TouchPoint(spatial_transform::ProjectLocal(target),true);
    }
    TouchPoint(spatial_transform::ProjectLocal({local.x+dx,local.y+dy}),false);Frame();Frame();
}
static ImGuiWindow* ThemePopup(){
    for(auto* w:GImGui->Windows)
        if(w->Active&&(w->Flags&ImGuiWindowFlags_Popup)&&!std::strstr(w->Name,"spatial-number-editor"))return w;
    return nullptr;
}
static void CloseThemePopup(){
    Mouse(8,8,false);Click(8,8);Frame();Frame();
}

#include "gyro_tests.inl"
static void QuoteTests(){
    auto& state=spatial::Get();auto& quotes=spatial::HomeQuotes();
    std::remove(state.path);state.reduceMotion=true;
    Frame();assert(spatial::IntroActive());
    while(spatial::IntroActive())Frame();Settle();CheckCentered();
    assert(opened&&tab==0&&!state.waitIntroRelease&&!state.overlayOpen);
    const auto settings=绘制.按钮;
    auto advance=[](double seconds){assert(seconds>=0);GImGui->Time+=seconds;Frame();};
    auto reset=[&](){quotes.Seed(UINT64_C(0x6251e08917b4a3cd));quotes.Update(ImGui::GetTime(),true);Frame();};
    auto clickOnce=[&](const ImRect& bounds){
        auto expected=quotes;expected.Refresh(ImGui::GetTime());
        const int pages=pageCalls,commands=commandCalls,exits=exitCalls;
        ClickRect(bounds);assert(quotes.Index()==expected.Index());
        assert(pageCalls==pages&&commandCalls==commands&&exitCalls==exits);
        assert(std::memcmp(&settings,&绘制.按钮,sizeof(settings))==0);
    };
    reset();const auto hero=spatial::HomeQuoteBounds();
    assert(hero.GetWidth()>600*state.scale&&std::fabs(hero.GetHeight()-122*state.scale)<.25f);
    float widestTitle=0,widestSubtitle=0;
    for(const auto& quote:spatial_quotes::Quotes){
        auto size=[&](const char* text,float fontSize){
            for(const char* next=text;*next;){unsigned int character;const int bytes=ImTextCharFromUtf8(&character,next,nullptr);
                assert(bytes>0&&ImGui::GetFont()->FindGlyphNoFallback((ImWchar)character));next+=bytes;}
            return ImGui::GetFont()->CalcTextSizeA(fontSize,FLT_MAX,0,text);
        };
        const auto title=size(quote.title,26*state.scale),subtitle=size(quote.subtitle,13*state.scale);
        assert(title.x<=hero.GetWidth()-48*state.scale&&subtitle.x<=hero.GetWidth()-48*state.scale);
        assert(45*state.scale+title.y<=91*state.scale&&91*state.scale+subtitle.y<=hero.GetHeight());
        widestTitle=std::max(widestTitle,title.x);widestSubtitle=std::max(widestSubtitle,subtitle.x);
    }
    std::printf("PASS all eighty title/subtitle pairs fit the real hero and font; widest %.1f/%.1f px in %.1f px\n",widestTitle,widestSubtitle,hero.GetWidth());
    clickOnce(hero);
    ImRect refreshLabel({hero.Max.x-76*state.scale,hero.Min.y+14*state.scale},{hero.Max.x-22*state.scale,hero.Min.y+34*state.scale});
    clickOnce(refreshLabel);
    std::puts("PASS the full home card and its refresh label advance exactly once without callbacks or setting changes");
    {
        auto expected=quotes;expected.Refresh(ImGui::GetTime());const auto point=spatial_transform::ProjectLocal(hero.GetCenter());
        TouchPoint(point,true);TouchPoint(point,false);assert(quotes.Index()==expected.Index());
        assert(!spatial::PageScroll().active&&!spatial::PageScroll().candidate);
        assert(std::memcmp(&settings,&绘制.按钮,sizeof(settings))==0);
    }
    std::puts("PASS a single-finger tap advances exactly once without scrolling or changing any setting");
    auto before=quotes.Index();ImVec2 center=spatial_transform::ProjectLocal(spatial::HomeQuoteBounds().GetCenter());
    Drag(center.x,center.y,28*state.scale,0);assert(quotes.Index()==before);
    Mouse(center.x,center.y,false);Frame();assert(quotes.Index()==before);
    std::puts("PASS dragging and hovering the quote card do not trigger an accidental refresh");
    reset();before=quotes.Index();advance(14.75);assert(quotes.Index()==before);
    advance(.3);assert(quotes.Index()!=before);
    std::puts("PASS an idle visible home card automatically changes after fifteen seconds");
    reset();advance(14.5);clickOnce(spatial::HomeQuoteBounds());before=quotes.Index();
    advance(.75);assert(quotes.Index()==before);advance(14.3);assert(quotes.Index()!=before);
    std::puts("PASS a manual click resets the UI countdown instead of being followed by an immediate automatic change");
    reset();advance(6);before=quotes.Index();opened=false;Frame();advance(80);assert(quotes.Index()==before);
    opened=true;Frame();advance(8.5);assert(quotes.Index()==before);advance(.6);assert(quotes.Index()!=before);Settle();
    std::puts("PASS hiding and restoring the panel excludes hidden time while preserving the remaining interval");
    reset();advance(6);before=quotes.Index();tab=1;Frame();advance(80);assert(quotes.Index()==before);
    tab=0;Frame();advance(8.5);assert(quotes.Index()==before);advance(.6);assert(quotes.Index()!=before);
    assert(std::memcmp(&settings,&绘制.按钮,sizeof(settings))==0);
    std::puts("PASS changing pages pauses the home countdown and preserves all drawing settings");
    reset();advance(6);ClickAt(1034,58);Frame();assert(ThemePopup()&&state.overlayOpen);before=quotes.Index();
    advance(80);assert(quotes.Index()==before);CloseThemePopup();assert(!ThemePopup()&&!state.overlayOpen);
    advance(8.3);assert(quotes.Index()==before);advance(.8);assert(quotes.Index()!=before);
    std::puts("PASS the global-color popup pauses automatic quote changes until the overlay closes");
    reset();advance(6);before=quotes.Index();center=spatial_transform::ProjectLocal(spatial::HomeQuoteBounds().GetCenter());
    Mouse(center.x,center.y,true);advance(80);assert(quotes.Index()==before);
    Mouse(center.x+30*state.scale,center.y,true);Mouse(center.x+30*state.scale,center.y,false);
    assert(quotes.Index()==before);advance(8.3);assert(quotes.Index()==before);advance(.8);assert(quotes.Index()!=before);
    std::puts("PASS holding the card pauses the timer and a released drag does not select another quote");
    for(int mode=0;mode<3;++mode){
        state.threeD=mode!=0;state.exploded=mode==2;
        spatial::Pose(mode?23.f:0,mode?-19.f:0,mode?6.f:0);Settle();reset();
        const float scale=state.scale,pitch=state.targetPitch,yaw=state.targetYaw,roll=state.targetRoll;
        clickOnce(spatial::HomeQuoteBounds());
        auto expected=quotes;expected.Refresh(ImGui::GetTime());
        const auto point=spatial_transform::ProjectLocal(spatial::HomeQuoteBounds().GetCenter());
        TouchPoint(point,true);TouchPoint(point,false);assert(quotes.Index()==expected.Index());
        assert(state.scale==scale&&state.targetPitch==pitch&&state.targetYaw==yaw&&state.targetRoll==roll);
    }
    std::puts("PASS mouse and finger hits follow the real quote layer in flat, tilted and exploded views without changing the pose");
    state.threeD=false;state.exploded=false;spatial::ResetPose();spatial::SetTheme(0,false);Settle();reset();
    for(int i=0;i<3;++i){
        if(i)clickOnce(spatial::HomeQuoteBounds());
        advance(.3);char filename[64];std::snprintf(filename,sizeof(filename),"draw-only-home-quotes-%d.ppm",i);Capture(filename);
        std::printf("CAPTURE %s | %s | %s\n",filename,quotes.Current().title,quotes.Current().subtitle);
    }
    assert(opened&&tab==0&&commandCalls==0&&exitCalls==0);
    assert(std::memcmp(&settings,&绘制.按钮,sizeof(settings))==0);
    std::remove(state.path);
    std::puts("PASS quote frames preserve balanced ImGui stacks, global styling and application callbacks");
}

#include "draw_only_tests.inl"
#include "background_tests.inl"
#include "responsive_tests.inl"
static void LiquidTests(){
    auto& state=spatial::Get();auto& backdrop=spatial_backdrop::Get();
    std::remove(state.path);std::remove(spatial::BackdropConfigPath().c_str());
    state.reduceMotion=true;backdrop=spatial_backdrop::Settings{};backdrop.motion=false;
    Frame();for(int i=0;i<180&&spatial::IntroActive();++i)Frame();
    assert(!spatial::IntroActive());Settle();
    spatial::SetTheme(3,false);spatial_backdrop::SelectScene(spatial_backdrop::Aurora);
    state.threeD=false;state.exploded=false;spatial::ResetPose();tab=0;
    width=720;height=1280;ResizeDrawable();Frame();Frame();
    const auto alpha=CaptureTransparent("draw-only-liquid-glass.tga");
    assert(alpha.clear>0&&alpha.partial>0&&alpha.visible>0);
    assert(!state.overlayOpen&&commandCalls==0&&exitCalls==0);
    std::printf("PASS liquid glass keeps transparency: clear=%zu partial=%zu visible=%zu at %dx%d\n",
        alpha.clear,alpha.partial,alpha.visible,width,height);
    std::puts("CAPTURE draw-only-liquid-glass.tga");
    width=1600;height=1000;ResizeDrawable();Frame();Frame();
    spatial::SetTheme(2,false);state.messageUntil=0;spatial::Recenter();Settle();
    spatial_backdrop::SelectScene(spatial_backdrop::BubbleWitch);
    tab=0;Capture("draw-only-liquid-match-home.ppm");
    tab=1;Capture("draw-only-liquid-match-drawing.ppm");
    auto sampledAlpha=[](ImVec2 p){
        unsigned char rgba[5*5*4]{};unsigned char values[25]{};
        glReadPixels((int)p.x-2,height-(int)p.y-2,5,5,GL_RGBA,GL_UNSIGNED_BYTE,rgba);
        for(int i=0;i<25;++i)values[i]=rgba[i*4+3];
        std::sort(values,values+25);return (int)values[12];
    };
    auto samplePoint=[](ImRect rect){return ImVec2(rect.Min.x+rect.GetWidth()*.65f,rect.GetCenter().y);};
    backdrop.enabled=false;
    for(int theme:{0,2}){
        spatial::SetTheme(theme,false);state.messageUntil=0;tab=0;Settle();
        char home[96],drawing[96];
        std::snprintf(home,sizeof(home),"draw-only-liquid-alpha-home-%d.tga",theme);
        std::snprintf(drawing,sizeof(drawing),"draw-only-liquid-alpha-drawing-%d.tga",theme);
        CaptureTransparent(home);
        const int quoteAlpha=sampledAlpha(samplePoint(spatial::HomeQuoteBounds()));
        const int navigationAlpha=sampledAlpha(samplePoint(spatial::ResponsiveLayout().nav[1]));
        const ImVec2 shell=spatial::ResponsiveLayout().shell.Min;
        const int brandingAlpha=sampledAlpha({shell.x+205*state.scale,shell.y+60*state.scale});
        const int emblemAlpha=sampledAlpha({shell.x+35*state.scale,shell.y+60*state.scale});
        tab=1;CaptureTransparent(drawing);
        const int drawingAlpha=sampledAlpha(samplePoint(FindWidget("绘制总开关").rect));
        std::printf("GLASS_ALPHA theme=%d quote=%d navigation=%d drawing=%d branding=%d emblem=%d / 255\n",theme,quoteAlpha,navigationAlpha,drawingAlpha,brandingAlpha,emblemAlpha);
        assert(std::abs(quoteAlpha-drawingAlpha)<=4&&std::abs(quoteAlpha-navigationAlpha)<=4);
        assert(std::abs(brandingAlpha-drawingAlpha)<=4&&std::abs(emblemAlpha-drawingAlpha)<=4);
    }
    std::puts("PASS composed quote, drawing row, navigation and upper-left branding transparency match within four alpha levels in light and dark themes");
    std::remove(state.path);
}
int main(int argc,char** argv){
    bool backgroundTest=argc>1&&std::strcmp(argv[1],"--background-test")==0;
    bool responsiveTest=argc>1&&std::strcmp(argv[1],"--responsive-test")==0;
    bool liquidTest=argc>1&&std::strcmp(argv[1],"--liquid-test")==0;
    bool test=argc>1&&std::strcmp(argv[1],"--test")==0;
    bool quotesTest=argc>1&&std::strcmp(argv[1],"--quotes-test")==0;
    bool gyroTest=argc>1&&std::strcmp(argv[1],"--gyro-test")==0;
    sceneMode=argc>1&&std::strcmp(argv[1],"--scene")==0;
    testMode=test||quotesTest||gyroTest||sceneMode||backgroundTest||responsiveTest||liquidTest;
    bool film=argc>1&&std::strcmp(argv[1],"--film")==0;
    bool transparent=argc>1&&std::strcmp(argv[1],"--transparent")==0;
    if(film){width=1200;height=750;}
    if(liquidTest){width=720;height=1280;}
    if(!test&&!quotesTest&&!gyroTest&&!film&&!transparent&&!sceneMode&&!backgroundTest&&!responsiveTest&&!liquidTest){RECT work{};SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);
        width=std::min(1440,(int)(work.right-work.left)-48);height=std::min(940,(int)(work.bottom-work.top)-72);}
    WNDCLASSW wc{};wc.style=CS_OWNDC;wc.lpfnWndProc=WindowProc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"SpatialPreview";
    wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassW(&wc);
    DWORD windowStyle=(test||quotesTest||gyroTest||film||transparent||sceneMode||backgroundTest||responsiveTest||liquidTest)?WS_POPUP:WS_OVERLAPPEDWINDOW;
    RECT rect{0,0,width,height};AdjustWindowRect(&rect,windowStyle,FALSE);
    hwnd=CreateWindowW(wc.lpszClassName,L"lsrael · Spatial 绘制专版 · 交互预览",windowStyle,0,0,rect.right-rect.left,rect.bottom-rect.top,nullptr,nullptr,wc.hInstance,nullptr);
    dc=GetDC(hwnd);PIXELFORMATDESCRIPTOR pf{};pf.nSize=sizeof(pf);pf.nVersion=1;pf.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;pf.iPixelType=PFD_TYPE_RGBA;pf.cColorBits=32;pf.cAlphaBits=8;
    SetPixelFormat(dc,ChoosePixelFormat(dc,&pf),&pf);glContext=wglCreateContext(dc);assert(glContext);wglMakeCurrent(dc,glContext);
    GLint alphaBits=0;glGetIntegerv(GL_ALPHA_BITS,&alphaBits);assert(alphaBits>=8);
    ImGui::CreateContext();ImGuiIO& io=ImGui::GetIO();io.IniFilename=nullptr;
    GetClientRect(hwnd,&rect);width=rect.right;height=rect.bottom;
    std::printf("UI renderer: %dx%d, OpenGL %s\n",width,height,(const char*)glGetString(GL_VERSION));
    ImFontConfig config;config.FontDataOwnedByAtlas=false;
    io.Fonts->AddFontFromMemoryTTF((void*)FontFile,Fontsize,28,&config,io.Fonts->GetGlyphRangesChineseFull());
    unsigned char* pixels;int aw,ah;io.Fonts->GetTexDataAsRGBA32(&pixels,&aw,&ah);
    glGenTextures(1,&atlas);glBindTexture(GL_TEXTURE_2D,atlas);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,aw,ah,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);io.Fonts->SetTexID((ImTextureID)(intptr_t)atlas);
    spatial_video_texture::Install();
    spatial::InstallBackdropImages(spatial_backdrop_image::Load,[]()->spatial::BackdropImageView{
        const auto& image=spatial_backdrop_image::Get();
        return {image.texture,{(float)image.width,(float)image.height},image.error};
    },spatial_backdrop_image::ReleaseCustom,[](int index)->spatial::BackdropImageView{
        const auto& image=spatial_backdrop_image::GetPreset(index);
        return {image.texture,{(float)image.width,(float)image.height},image.error};
    });
    spatial::Get().path=backgroundTest?"ui-background-test.cfg":responsiveTest?"ui-responsive-test.cfg":gyroTest?"ui-gyro-test.cfg":quotesTest?"ui-quotes-test.cfg":(test||film||transparent||liquidTest)?"ui-test.cfg":"draw-only-preview.cfg";
    if(film){std::remove(spatial::Get().path);CreateDirectoryW(L"film-v6",nullptr);
        const int frameCount=(int)std::ceil((spatial_cinema::Duration+.8f)*60.f);
        for(int i=0;i<frameCount;++i){Frame(i%4==0);if(i%4==0){char name[64];std::snprintf(name,sizeof(name),"film-v6/frame-%03d.ppm",i/4);SavePixels(name);}}
        assert(!spatial::IntroActive()&&!spatial_video::Get().visible&&!spatial_video_texture::Get().texture);
        std::printf("Exported %d startup frames at 15 fps through %.2f seconds\n",(frameCount+3)/4,frameCount/60.f);
    }else if(sceneMode){
        for(int theme=0;theme<spatial_theme::Count;++theme){
            spatial::SetTheme(theme,false);Frame();CheckFiniteDrawData();
            char filename[96];std::snprintf(filename,sizeof(filename),"draw-only-entities-theme-%d.ppm",theme);
            Capture(filename);std::printf("PASS shared entity renderer theme %d; CAPTURE %s\n",theme,filename);
        }
    }else if(test)DrawOnlyTests();else if(quotesTest)QuoteTests();else if(gyroTest)GyroTests();else if(backgroundTest)BackgroundTests();
    else if(responsiveTest)ResponsiveTests();else if(liquidTest)LiquidTests();else if(transparent){
        std::remove(spatial::Get().path);Frame();
        for(float time:{.9f,2.2f,3.8f,5.3f}){
            while(ImGui::GetTime()-spatial::Get().startTime<time)Frame();
            char name[64];std::snprintf(name,sizeof(name),"v6-intro-video-%02d.tga",(int)(time*10));CaptureTransparent(name);}
        while(spatial::IntroActive())Frame();Settle();DrawOnlyGallery();std::remove(spatial::Get().path);
    }else{ShowWindow(hwnd,SW_SHOW);MSG msg{};bool done=false;
        while(!done){while(PeekMessage(&msg,nullptr,0,0,PM_REMOVE)){if(msg.message==WM_QUIT)done=true;TranslateMessage(&msg);DispatchMessage(&msg);}if(done)break;
            if(width>0&&height>0){Frame(true);SwapBuffers(dc);}Sleep(16);}}
    spatial_video::Release();spatial_backdrop_image::Release();glDeleteTextures(1,&atlas);
    ImGui::DestroyContext();wglMakeCurrent(nullptr,nullptr);wglDeleteContext(glContext);ReleaseDC(hwnd,dc);DestroyWindow(hwnd);return 0;
}

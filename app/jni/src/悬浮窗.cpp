/* 
 * 由入梦 频道@lsraelnb开源
 * 注入时间: 2026-07-05 11:50:33
 * 联系方式: 交流群：1074049072
 */

#include "imgui.h"
#include "SpatialUI.h"
#include "SpatialGyroAndroid.h"
#include "SpatialConsole.h"
#include "图片调用.h"
#include "辅助类.h"
#include <linux/input.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <unistd.h> 
#include "imgui_settings.h"
#include "main.h"
#include "SpatialVideoTexture.h"
#include "SpatialBackdropImage.h"
#include <cstdio>
#include <string>
#include <unistd.h>
#include <fstream>
#include <sys/stat.h>
#include <imgui.h>
#include <cstdio>
#include <string>
#include <array>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <codecvt>
#include <dirent.h> 
#include <fcntl.h> 
#include <iostream>
#include <locale> 
#include <malloc.h>
#include <math.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <stdio.h>
#include <cstdlib>
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <linux/input.h> 
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <vector>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#define MAX_FILES 100
#define MAX_PATH_LEN 256
#include <cstdio>
#include <fstream>
#include <string>
#include <cstring>
#include <thread>
#include <chrono>
#include <dirent.h>
#include <pthread.h>
#include <fstream>
#include <string.h>
#include <time.h>
#include <malloc.h>
#include <iostream>
#include <fstream>
#include<iostream>
#include <string>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <signal.h>
#include <unistd.h>
#include<ctime>



#include "Font.h"
#include "main.h"
#include "RAGE.h"
#include "Main/Items.h"
#include <thread>
#include <chrono>
static bool 悬浮窗 = true;
static bool spatialExitRequested = false;
extern 绘制 绘制;
ImFont* Forward = nullptr;
#include <atomic>
#include <sys/utsname.h>
#include <string>
#include <fstream>
#include <cctype>
#include <sys/system_properties.h> // Android 获取系统属性
static bool 模型初始化 = false;
static bool 绘制模型 = false;
static std::string g_brand = "Unknown";
static std::string g_model = "Unknown";
static std::string g_kernel = "Unknown";
static bool g_info_loaded = false;
static std::string GetProp(const std::string& key) {
    char value[PROP_VALUE_MAX] = {0};
    __system_property_get(key.c_str(), value);
    return std::string(value);
}

// 简化内核版本显示
static std::string SimplifyKernelVersion(const std::string& kernel) {
    if (kernel.empty() || kernel == "Unknown") return kernel;

    std::string simplified;
    bool found_letter = false;

    for (char c : kernel) {
        if (std::isalpha(c) && c != '.') {
            found_letter = true;
            break;
        }
        if (std::isdigit(c) || c == '.') {
            simplified += c;
        } else if (!std::isspace(c)) {
            break;
        }
    }

    return simplified.empty() ? kernel : simplified;
}
static std::atomic<bool> mirrorDisplayRunning(false);
static std::chrono::steady_clock::time_point lastMirrorCallTime = 
    std::chrono::steady_clock::now() - std::chrono::seconds(6); //延迟6s 减少占用
static std::mutex mirrorMutex; //定义互斥锁
static void GetBase();
static void GetBase() {
    绘制.初始化绘制("com.tencent.tmgp.pubgmhd", abs_ScreenX, abs_ScreenY);
    ReadGame::Initialization();   // 内含 StartLoadMapModel() 加载 PhysX 场景
    模型初始化 = true;
    绘制模型 = true;              // 【自动】场景初始化完成，后续由"开启PhysX"控制是否画线
}
void AsyncProcessMirrorDisplay() {
    //互斥锁
    std::lock_guard<std::mutex> lock(mirrorMutex);
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastMirrorCallTime).count();
    
    if (mirrorDisplayRunning) {
        //跳过运行 防止内存泄露
        return;
    }

    //运行标志
    mirrorDisplayRunning = true;
/* 
 * 由入梦 频道@lsraelnb开源
 * 注入时间: 2026-07-05 11:50:33
 * 联系方式: 交流群：1074049072
 */    
    //创建线程处理
    std::thread mirrorThread([]() {
        SURFACE_LOG_INFO("Starting mirror display processing in thread %lu", 
                         std::this_thread::get_id());
        
        try {
            android::ANativeWindowCreator::ProcessMirrorDisplay();
            SURFACE_LOG_INFO("Mirror display processing completed successfully");
        } catch (const std::exception& e) {
            SURFACE_LOG_ERROR("Mirror display error: %s", e.what());
        } catch (...) {
            SURFACE_LOG_ERROR("Unknown error in mirror display");
        }
        
        //更新状态
        {
            std::lock_guard<std::mutex> lock(mirrorMutex);
            lastMirrorCallTime = std::chrono::steady_clock::now();
            mirrorDisplayRunning = false;
        }
        
        SURFACE_LOG_INFO("Mirror display processing finished");
    });
    
    //后台运行
    mirrorThread.detach();
}
EGLDisplay display = EGL_NO_DISPLAY;
EGLConfig config;
EGLSurface surface = EGL_NO_SURFACE;
EGLContext context = EGL_NO_CONTEXT;

ANativeWindow *native_window;
//
int native_window_screen_x = 0;
int native_window_screen_y = 0;
android::ANativeWindowCreator::DisplayInfo displayInfo{0};
uint32_t orientation = 0;
bool g_Initialized = false;
ImGuiWindow *g_window = nullptr;
int Orientation;

bool initGUI_draw(uint32_t _screen_x, uint32_t _screen_y, bool log) {
    orientation = displayInfo.orientation;
    if (!init_egl(_screen_x, _screen_y, log)) {
        return false;
    }
    if (!ImGui_init() || !ImGui::GetCurrentContext()) {
        spatial_console::GraphicsStep("界面上下文初始化失败", false);
        return false;
    }
    if (!ImGui::GetIO().BackendPlatformName || !ImGui::GetIO().BackendRendererUserData) {
        spatial_console::GraphicsStep("界面绘制后端初始化失败", false);
        return false;
    }
    spatial_video_texture::Install();
    return true;
}

void 过强制() {
    std::thread([]() {
        while (true) {
            android::ANativeWindowCreator::ProcessMirrorDisplay();
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    }).detach();
}

bool init_egl(uint32_t _screen_x, uint32_t _screen_y, bool log) {
    const int displayChoice=spatial_console::ChooseDisplayMode(_screen_x,_screen_y);
    if(displayChoice==0)return false;
    const bool protectedMode=displayChoice==1;
    spatial_console::BeginGraphics(_screen_x,_screen_y,protectedMode);
    if(protectedMode)
        ::native_window=android::ANativeWindowCreator::Create("阿念",_screen_x,_screen_y,true);
    else {
        ::native_window=android::ANativeWindowCreator::Create("阿念",_screen_x,_screen_y);
        过强制();
    }
    if(!native_window){spatial_console::GraphicsStep("窗口创建失败",false);return false;}
    ANativeWindow_acquire(native_window);
    spatial_console::GraphicsStep("显示窗口已创建");
    display=eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if(display==EGL_NO_DISPLAY||eglInitialize(display,0,0)!=EGL_TRUE){
        spatial_console::GraphicsStep("图形显示设备初始化失败",false);return false;
    }
    const EGLint attribList[]={
        EGL_SURFACE_TYPE,EGL_WINDOW_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,
        EGL_BLUE_SIZE,8,EGL_GREEN_SIZE,8,EGL_RED_SIZE,8,EGL_ALPHA_SIZE,8,
        EGL_BUFFER_SIZE,32,EGL_DEPTH_SIZE,16,EGL_STENCIL_SIZE,8,EGL_NONE
    };
    const EGLint contextAttributes[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
    EGLint count=0;
    if(eglChooseConfig(display,attribList,&config,1,&count)!=EGL_TRUE||count<1){
        spatial_console::GraphicsStep("图形配置不可用",false);return false;
    }
    EGLint format=0;
    eglGetConfigAttrib(display,config,EGL_NATIVE_VISUAL_ID,&format);
    ANativeWindow_setBuffersGeometry(native_window,0,0,format);
    spatial_console::GraphicsStep("透明画布已配置");
    context=eglCreateContext(display,config,EGL_NO_CONTEXT,contextAttributes);
    if(context==EGL_NO_CONTEXT){spatial_console::GraphicsStep("图形上下文创建失败",false);return false;}
    surface=eglCreateWindowSurface(display,config,native_window,nullptr);
    if(surface==EGL_NO_SURFACE){spatial_console::GraphicsStep("绘制表面创建失败",false);return false;}
    if(!eglMakeCurrent(display,surface,surface,context)){
        spatial_console::GraphicsStep("图形上下文激活失败",false);return false;
    }
    spatial_console::GraphicsStep("OpenGL ES 3.0 已就绪");
    // EGL readiness does not mean that the ImGui context/backends exist yet.
    // ImGui_init() owns g_Initialized and sets it only after full UI setup.
    return true;
}


void screen_config() {
    displayInfo = android::ANativeWindowCreator::GetDisplayInfo();
    Orientation = displayInfo.orientation;
}

bool loadSystemFont() {
    ImGuiIO &io = ImGui::GetIO();
    ImFontConfig embeddedFont;
    embeddedFont.FontDataOwnedByAtlas = false;
    font::opoo = io.Fonts->AddFontFromMemoryTTF((void *)FontFile, Fontsize, 26.0f, &embeddedFont, io.Fonts->GetGlyphRangesChineseFull());
    绘制.font_24 = io.Fonts->AddFontFromMemoryTTF((void *)FontFile, Fontsize, static_cast<float>(绘制.名称字体大小), &embeddedFont, io.Fonts->GetGlyphRangesChineseFull());

    return font::opoo != nullptr && 绘制.font_24 != nullptr;
}

bool ImGui_init()
{
    if (g_Initialized)
    {
        return ImGui::GetCurrentContext() != nullptr &&
               ImGui::GetIO().BackendPlatformName != nullptr &&
               ImGui::GetIO().BackendRendererUserData != nullptr;
    }
    IMGUI_CHECKVERSION();
    if (!ImGui::CreateContext()) return false;
    ImGuiStyle* style = &ImGui::GetStyle();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.IniFilename = NULL;
    ImGui::StyleColorsLight();
    if (!ImGui_ImplAndroid_Init(native_window) || !ImGui_ImplOpenGL3_Init("#version 300 es"))
        return false;
    io.IniFilename = NULL;
    bool success = loadSystemFont();
    if (!success)
    {
        return false;
    }
    style->WindowPadding = ImVec2(0, 0);
    style->WindowBorderSize = 0;
    style->ItemSpacing = ImVec2(20, 20);
    style->ScrollbarSize = 15.f;
    ImFontConfig cfg;
    memset(&Config, 0, sizeof(sConfig));

    cfg.FontDataOwnedByAtlas = false;
    Forward = io.Fonts->AddFontFromMemoryTTF((void *)RAGEdata, RAGEsize, 30.0f, &cfg, io.Fonts->GetGlyphRangesDefault());
    if (!Forward) return false;

    
    Config.Vehicle.Enable = true;

    ::g_Initialized = true;
  
    return true;
}

int 布局::初始化程序()
{
    screen_config();
    AsyncProcessMirrorDisplay(); // 根据force_mirror的值调
    ::abs_ScreenX = (displayInfo.height > displayInfo.width ? displayInfo.height : displayInfo.width);
    ::abs_ScreenY = (displayInfo.height < displayInfo.width ? displayInfo.height : displayInfo.width);

    /* 【修·录屏偏框】原来这两行用了【同一个条件】，导致 X 和 Y 都变成较大值。
       例：3040x1904 屏幕上会创建 3040x3040 窗口 → 绘制被拉伸 → 偏框。
       改为与上面 abs_ScreenX/Y 一致的正确配对。 */
    ::native_window_screen_x = (displayInfo.height > displayInfo.width ? displayInfo.height : displayInfo.width);
    ::native_window_screen_y = (displayInfo.height < displayInfo.width ? displayInfo.height : displayInfo.width);
    if (!initGUI_draw(native_window_screen_x, native_window_screen_y, true))
    {
        return -1;
    }

           Touch_Init(displayInfo.width, displayInfo.height, displayInfo.orientation, true);

//    模型状态 = 绘制.按钮.模型;
    return 0;
}

void drawBegin()
{
    screen_config();

    if (::orientation != displayInfo.orientation)
    {
        ::orientation = displayInfo.orientation;
        UpdateScreenData(displayInfo.width, displayInfo.height, displayInfo.orientation);
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame(native_window_screen_x, native_window_screen_y);
    spatial_gyro_android::Install(displayInfo.orientation);
    spatial::BeginFrameInput(悬浮窗);
    ImGui::NewFrame();
}

void drawEnd()
{
    ImGui::Render();
    spatial::FinishFrame();

    glClearColor(0.f,0.f,0.f,0.f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    eglSwapBuffers(display, surface);
}
int tabs = 0;
static void LoadDeviceInfo() {
    if (g_info_loaded) return;

    g_brand = GetProp("ro.product.brand");
    if (g_brand.empty()) {
        g_brand = GetProp("ro.product.manufacturer");
    }
    if (g_brand.empty()) g_brand = "Unknown";

    g_model = GetProp("ro.product.model");
    if (g_model.empty()) g_model = "Unknown";

    struct utsname buf;
    if (uname(&buf) == 0) {
        g_kernel = SimplifyKernelVersion(std::string(buf.release));
    } else {
        g_kernel = "Unknown";
    }
    g_info_loaded = true;
}

void ModernStyleToggle(const char* label,bool* value){spatial::Toggle(label,value);}
#include "SpatialLegacyPages.inl"

void 布局::绘制悬浮窗() {
    drawBegin();
    

    if (绘制.按钮.绘制) {
        绘制.运行绘制();
    }
    if (模型初始化){
        ReadGame::ReadGameData();
    }
    if(绘制.按钮.掩体识别||绘制.按钮.PhysX画线){
        ReadGame::EnsurePhysXStarted();
        if((模型初始化||绘制模型)&&绘制.按钮.PhysX画线)ReadGame::Raycast();
    }
    spatial::Get().path="/data/local/tmp/lsrael_draw_139_ui.cfg";
    spatial::InstallBackdropImages(spatial_backdrop_image::Load,[]()->spatial::BackdropImageView{
        const auto& image=spatial_backdrop_image::Get();
        return {image.texture,{(float)image.width,(float)image.height},image.error};
    },spatial_backdrop_image::ReleaseCustom,[](int index)->spatial::BackdropImageView{
        const auto& image=spatial_backdrop_image::GetPreset(index);
        return {image.texture,{(float)image.width,(float)image.height},image.error};
    });
    spatial::Render(悬浮窗, tabs, DrawSpatialPage, [](spatial::Action action) {
        switch (action) {
        case spatial::Save: 绘制.保存配置("lsrael_draw_139"); break;
        case spatial::Load: 绘制.读取配置("lsrael_draw_139"); break;
        case spatial::Initialize: GetBase(); 模型初始化=true; 绘制.按钮.绘制=true; break;
        case spatial::Exit: spatial::SavePreferences(); spatialExitRequested=true; break;
        }
    });
    // Keep the touch hit region synchronized with the visible spatial shell.
    const spatial::State& ui = spatial::Get();
    if (悬浮窗) {
        const ImRect bounds=spatial::WindowBounds();
        绘制.Pos.x=bounds.Min.x;
        绘制.Pos.y=bounds.Min.y;
        绘制.winWidth=(int)bounds.GetWidth();
        绘制.winHeith=(int)bounds.GetHeight();
    } else {
        绘制.Pos.x = ImGui::GetIO().DisplaySize.x - 160;
        绘制.Pos.y = ImGui::GetIO().DisplaySize.y - 82;
        绘制.winWidth = 140;
        绘制.winHeith = 54;
    }

    drawEnd();
}

                           


int 数据() {
    DIR *dir = opendir("/dev/input/");
    if (!dir) return -1;
    dirent *ptr = NULL;
    int count = 0;
    while ((ptr = readdir(dir)) != NULL) {
        if (strstr(ptr->d_name, "event"))
            count++;
    }
    closedir(dir);
    return count ? count : -1;
}


int 音量() {
    int EventCount = 数据();
    if (EventCount < 0) {
        printf("未找到输入设备\n");
        return -1;
    }

    int *fdArray = (int *)malloc(EventCount * sizeof(int));
    if (!fdArray) return -1;

    for (int i = 0; i < EventCount; i++) {
        char temp[128];
        sprintf(temp, "/dev/input/event%d", i);
        fdArray[i] = open(temp, O_RDWR | O_NONBLOCK);
    }


    input_event ev;
    while (1) {
        for (int i = 0; i < EventCount; i++) {
            if (fdArray[i] < 0) continue;
            memset(&ev, 0, sizeof(ev));
            if (read(fdArray[i], &ev, sizeof(ev)) != sizeof(ev)) continue;
            if (ev.type == EV_KEY && (ev.code == KEY_VOLUMEUP || ev.code == KEY_VOLUMEDOWN)) {
          
                 if (ev.code == 115&&ev.value==1) {//音量➕
                // printf("开启");
                 悬浮窗 = true;
                } else if (ev.code == 114&&ev.value==1) {
                悬浮窗 = false;
               // printf("关闭");
                }
            }
            usleep(1000);
        }
        usleep(500);
    }
       usleep(1500);
    return 0;
}

void 布局::开启悬浮窗() {
//    ImPlot::CreateContext(); // ✅ 新增：初始化 ImPlot
    timer WindowDrawing;
    WindowDrawing.SetFps(120);
    WindowDrawing.AotuFPS_init();
    WindowDrawing.setAffinity();
    std::thread volumeThread(音量);
    volumeThread.detach();
 while (true) {
        绘制悬浮窗();
        if(spatialExitRequested){
            // Finish the current ImGui/GL frame before stopping input and
            // exiting normally through the application's atexit handlers.
            spatial_video::Release();
            spatial_backdrop_image::Release();
            Touch_Close();
            std::exit(0);
        }
        WindowDrawing.SetFps(绘制.按钮.当前帧率);
        WindowDrawing.AotuFPS();
        std::this_thread::sleep_for(1ms);
    }
}   

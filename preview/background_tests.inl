// Included after draw_only_tests.inl. These tests drive the real background
// studio widgets; the popup controls deliberately receive raw screen positions.
static ImGuiWindow* BackgroundPopupWindow(){
    for(auto* window:GImGui->Windows)
        if(window->Active&&(window->Flags&ImGuiWindowFlags_Modal)&&
           !(window->Flags&ImGuiWindowFlags_ChildWindow)&&std::strstr(window->Name,"background-studio"))return window;
    return nullptr;
}
static void BackgroundFrames(int count=2){for(int i=0;i<count;++i){Frame();CheckFiniteDrawData();}}
static void BackgroundCheckBounds(){
    const auto rect=spatial::BackdropLayout().popup;
    assert(BackgroundPopupWindow()&&rect.GetWidth()>100&&rect.GetHeight()>100);
    assert(rect.Min.x>=7&&rect.Min.y>=7&&rect.Max.x<=width-7&&rect.Max.y<=height-7);
    assert(spatial::Get().overlayOpen&&spatial_transform::Get().screenOverlay);
}
static void BackgroundOpenStudio(){
    assert(!BackgroundPopupWindow());
    ClickRect(spatial::BackdropLayout().entry);BackgroundFrames();BackgroundCheckBounds();
}
static void BackgroundScrollTop(){
    auto* body=spatial::BackdropStudio().scroll.window;assert(body);
    ImGui::SetScrollY(body,0);BackgroundFrames();
}
template<class ReadRectangle> static ImRect BackgroundReveal(ReadRectangle read){
    for(int attempt=0;attempt<5;++attempt){
        const auto rect=read(spatial::BackdropLayout());
        assert(rect.GetWidth()>0&&rect.GetHeight()>0);
        auto* body=spatial::BackdropStudio().scroll.window;assert(body);
        const auto clip=spatial::BackdropStudio().scroll.clip;
        if(clip.Contains(rect.GetCenter()))return rect;
        ImGui::SetScrollY(body,body->Scroll.y+rect.GetCenter().y-clip.GetCenter().y);BackgroundFrames();
    }
    assert(false);return {};
}
template<class ReadRectangle> static void BackgroundClickControl(ReadRectangle read,float fraction=.5f){
    ClickRect(BackgroundReveal(read),false,fraction);BackgroundFrames();
}
static void BackgroundSelectTab(int index){
    ClickRect(spatial::BackdropLayout().tabs[index],false);BackgroundFrames();
    assert(spatial::BackdropStudio().tab==index);BackgroundScrollTop();
}
static void BackgroundTouchSwipe(ImVec2 start,float dy){
    TouchPoint(start,false);TouchPoint(start,true);
    for(int i=1;i<=14;++i)TouchPoint({start.x,start.y+dy*i/14},true);
    TouchPoint({start.x,start.y+dy},false);BackgroundFrames();
}
static bool BackgroundSettingsEqual(const spatial_backdrop::Settings& a,const spatial_backdrop::Settings& b){
    if(a.enabled!=b.enabled||a.motion!=b.motion||a.followTheme!=b.followTheme||a.scene!=b.scene||a.mode!=b.mode||
       a.rotationMask!=b.rotationMask||a.customEffect!=b.customEffect||std::strcmp(a.imagePath,b.imagePath))return false;
    const float left[]={a.interval,a.density,a.speed,a.opacity,a.dim,a.wind,a.top.x,a.top.y,a.top.z,a.bottom.x,a.bottom.y,a.bottom.z};
    const float right[]={b.interval,b.density,b.speed,b.opacity,b.dim,b.wind,b.top.x,b.top.y,b.top.z,b.bottom.x,b.bottom.y,b.bottom.z};
    for(int i=0;i<12;++i)if(std::fabs(left[i]-right[i])>.00001f)return false;
    return true;
}
static void BackgroundCloseStudio(bool done){
    ClickRect(done?spatial::BackdropLayout().done:spatial::BackdropLayout().close,false);BackgroundFrames();
    assert(!BackgroundPopupWindow()&&!spatial::Get().overlayOpen&&!spatial_transform::Get().screenOverlay);
}
static void BackgroundTests(){
    auto& state=spatial::Get();auto& config=spatial_backdrop::Get();
    const std::string profile=state.path,sidecar=spatial::BackdropConfigPath();
    std::remove(profile.c_str());std::remove(sidecar.c_str());
    config=spatial_backdrop::Settings{};spatial_backdrop::ResetClock();state.reduceMotion=true;
    Frame();for(int i=0;i<180&&spatial::IntroActive();++i)Frame();
    assert(!spatial::IntroActive());Settle();state.reduceMotion=false;
    state.threeD=false;state.exploded=false;spatial::ResetPose();spatial::SetTheme(2,false);BackgroundFrames();
    const auto drawingSettings=绘制.按钮;const int commands=commandCalls,exits=exitCalls;
    BackgroundOpenStudio();
    assert(spatial::BackdropStudio().tab==0);
    const auto centered=spatial::BackdropLayout().popup.GetCenter();
    assert(std::fabs(centered.x-width*.5f)<1&&std::fabs(centered.y-height*.5f)<1);
    assert(state.overlayBounds.Min.x==spatial::BackdropLayout().popup.Min.x&&state.overlayBounds.Min.y==spatial::BackdropLayout().popup.Min.y);
    assert(std::fabs(spatial::BackdropStudio().scroll.window->CalcFontSize()-16.f)<.1f);
    state.messageUntil=0;Capture("draw-only-background-studio.ppm");
    std::puts("PASS the sidebar entry opens the centered background studio with raw-screen popup geometry");

    BackgroundSelectTab(1);BackgroundSelectTab(2);BackgroundSelectTab(0);
    std::puts("PASS all three background tabs respond to real mouse clicks");
    for(int scene=0;scene<6;++scene){
        BackgroundClickControl([&](const auto& geometry){return geometry.scenes[scene];});
        assert(config.scene==scene&&config.mode==0&&spatial_backdrop::CurrentScene()==scene);
        BackgroundCloseStudio(true);state.messageUntil=0;
        char name[96];std::snprintf(name,sizeof(name),"draw-only-background-scene-%d.ppm",scene);Capture(name);
        BackgroundOpenStudio();
    }
    std::puts("PASS all six scene cards apply their live panel backgrounds and each exports a GL capture");

    GLuint presetTextures[spatial_backdrop_presets::Count]{};
    for(int index=0;index<spatial_backdrop_presets::Count;++index){
        const int scene=spatial_backdrop::StarryEye+index;
        BackgroundClickControl([&](const auto& geometry){return geometry.scenes[scene];});
        assert(config.scene==scene&&config.mode==0&&!config.imagePath[0]);
        const auto& image=spatial_backdrop_image::GetPreset(index);
        presetTextures[index]=image.textureName;
        assert(image.textureName&&glIsTexture(image.textureName)&&!*image.error);
        assert(spatial::BackdropImageForScene(scene).texture==image.texture);
        BackgroundCloseStudio(true);state.messageUntil=0;
        char name[96];std::snprintf(name,sizeof(name),"draw-only-background-preset-%d.ppm",index);Capture(name);
        assert(glGetError()==GL_NO_ERROR);
        const auto savedPreset=config;config=spatial_backdrop::Settings{};spatial::LoadBackdropPreferences();
        assert(BackgroundSettingsEqual(config,savedPreset));BackgroundFrames(12);
        assert(spatial_backdrop_image::GetPreset(index).textureName==presetTextures[index]);
        BackgroundOpenStudio();
    }
    BackgroundScrollTop();Capture("draw-only-background-preset-cards.ppm");
    std::puts("PASS all three embedded picture cards select, save, reload and render real cached GL textures without external paths");

    BackgroundClickControl([](const auto& geometry){return geometry.modes[1];});assert(config.mode==1);
    BackgroundClickControl([](const auto& geometry){return geometry.sliders[0];},.005f);
    assert(config.interval>=5&&config.interval<=8);
    const unsigned originalMask=config.rotationMask;
    BackgroundClickControl([](const auto& geometry){return geometry.pool[5];});
    assert(config.rotationMask==(originalMask^(1u<<5)));
    BackgroundClickControl([](const auto& geometry){return geometry.pool[2];});
    assert(config.rotationMask==((originalMask^(1u<<5))^(1u<<2)));
    const int initialScene=spatial_backdrop::CurrentScene();
    for(int i=0;i<38;++i){GImGui->Time+=.24;BackgroundFrames(1);}
    assert(spatial_backdrop::CurrentScene()!=initialScene);
    BackgroundClickControl([](const auto& geometry){return geometry.modes[0];});
    assert(config.mode==0&&spatial_backdrop::CurrentScene()==config.scene);
    std::puts("PASS fixed and rotating modes, interval control and rotation-pool checkboxes affect real playback");

    const auto beforePresetRotation=config;
    BackgroundClickControl([](const auto& geometry){return geometry.sliders[0];},.995f);
    for(int scene=spatial_backdrop::StarryEye;scene<spatial_backdrop::SceneCount;++scene)
        BackgroundClickControl([&](const auto& geometry){return geometry.pool[scene];});
    for(int scene=0;scene<6;++scene)if(config.rotationMask&(1u<<scene))
        BackgroundClickControl([&](const auto& geometry){return geometry.pool[scene];});
    assert(config.rotationMask==((1u<<7)|(1u<<8)|(1u<<9)));
    BackgroundClickControl([](const auto& geometry){return geometry.scenes[spatial_backdrop::StarryEye];});
    BackgroundClickControl([](const auto& geometry){return geometry.modes[1];});
    BackgroundClickControl([](const auto& geometry){return geometry.sliders[0];},.005f);
    const double cycleStart=spatial_backdrop::Run().cycleClock;
    for(int i=0;i<(int)std::ceil((config.interval-cycleStart)/.25);++i){GImGui->Time+=.24;BackgroundFrames(1);}
    assert(spatial_backdrop::CurrentScene()==spatial_backdrop::BubbleWitch);
    Frame(true);assert(glGetError()==GL_NO_ERROR);
    const auto rotatingSaved=config;BackgroundCloseStudio(true);config=spatial_backdrop::Settings{};
    spatial::LoadBackdropPreferences();assert(BackgroundSettingsEqual(config,rotatingSaved));
    config=beforePresetRotation;spatial_backdrop::ResetClock();BackgroundFrames();BackgroundOpenStudio();
    std::puts("PASS picture-only rotation can be configured with real checkboxes and its selected pool survives reload");

    BackgroundSelectTab(1);
    BackgroundClickControl([](const auto& geometry){return geometry.enabled;},.92f);assert(!config.enabled);
    const double disabledClock=spatial_backdrop::Run().clock;BackgroundFrames(8);
    assert(spatial_backdrop::Run().clock==disabledClock);
    BackgroundClickControl([](const auto& geometry){return geometry.enabled;},.92f);assert(config.enabled);
    BackgroundClickControl([](const auto& geometry){return geometry.motion;},.92f);assert(!config.motion);
    const double pausedClock=spatial_backdrop::Run().clock;BackgroundFrames(8);
    assert(spatial_backdrop::Run().clock==pausedClock);
    BackgroundClickControl([](const auto& geometry){return geometry.motion;},.92f);assert(config.motion);
    const double resumedClock=spatial_backdrop::Run().clock;BackgroundFrames(8);
    assert(spatial_backdrop::Run().clock>resumedClock);
    state.reduceMotion=true;BackgroundFrames();const double reducedClock=spatial_backdrop::Run().clock;
    BackgroundFrames(8);assert(spatial_backdrop::Run().clock==reducedClock);state.reduceMotion=false;BackgroundFrames();
    const float density=config.density,speed=config.speed,opacity=config.opacity,dim=config.dim,wind=config.wind;
    BackgroundClickControl([](const auto& geometry){return geometry.sliders[1];},.82f);
    BackgroundClickControl([](const auto& geometry){return geometry.sliders[2];},.62f);
    BackgroundClickControl([](const auto& geometry){return geometry.sliders[3];},.64f);
    BackgroundClickControl([](const auto& geometry){return geometry.sliders[4];},.31f);
    BackgroundClickControl([](const auto& geometry){return geometry.sliders[5];},.72f);
    assert(config.density!=density&&config.speed!=speed&&config.opacity!=opacity&&config.dim!=dim&&config.wind!=wind);
    BackgroundScrollTop();state.messageUntil=0;Capture("draw-only-background-motion.ppm");
    std::puts("PASS background enable, playback pause, reduced motion and all five appearance sliders operate independently");

    BackgroundSelectTab(0);BackgroundScrollTop();
    auto* sceneBody=spatial::BackdropStudio().scroll.window;assert(sceneBody&&sceneBody->ScrollMax.y>20);
    const auto beforeSwipe=config;const float scrollBefore=sceneBody->Scroll.y;
    const auto card=spatial::BackdropLayout().scenes[spatial_backdrop::StarryEye];
    BackgroundTouchSwipe({card.Min.x+18,card.GetCenter().y},-180);
    assert(sceneBody->Scroll.y>scrollBefore+10&&BackgroundSettingsEqual(config,beforeSwipe));
    assert(!spatial::BackdropStudio().scroll.active&&!spatial::BackdropStudio().scroll.candidate);
    std::puts("PASS a real finger swipe scrolls the scene body without selecting a card or changing settings");

    BackgroundSelectTab(2);
    BackgroundClickControl([](const auto& geometry){return geometry.follow;},.92f);assert(!config.followTheme);
    BackgroundClickControl([](const auto& geometry){return geometry.applyCustom;});
    assert(config.scene==spatial_backdrop::Custom&&config.mode==0);
    for(int effect=0;effect<6;++effect){
        BackgroundClickControl([&](const auto& geometry){return geometry.effects[effect];});assert(config.customEffect==effect);
    }
    BackgroundClickControl([](const auto& geometry){return geometry.effects[2];});
    BackgroundClickControl([](const auto& geometry){return geometry.browse;});
    assert(spatial::BackdropStudio().browsing&&spatial::BackdropStudio().listed);
    assert(spatial::BackdropStudio().listing.entries.size()<=512);
    const std::string folder=spatial::BackdropStudio().listing.directory;
    const auto parent=spatial_backdrop_files::Parent(folder);
    BackgroundClickControl([](const auto& geometry){return geometry.back;});
    assert(spatial::BackdropStudio().listing.directory==parent);
    BackgroundClickControl([](const auto& geometry){return geometry.browse;});assert(!spatial::BackdropStudio().browsing);
    BackgroundScrollTop();state.messageUntil=0;Capture("draw-only-background-custom.ppm");
    const auto saved=config;BackgroundCloseStudio(true);
    FILE* persisted=std::fopen(sidecar.c_str(),"rb");assert(persisted);assert(std::fclose(persisted)==0);
    config=spatial_backdrop::Settings{};spatial::LoadBackdropPreferences();
    assert(BackgroundSettingsEqual(config,saved));BackgroundFrames();
    std::puts("PASS custom gradients, all effect choices and local-directory navigation work; Done saves the separate profile");

    BackgroundOpenStudio();BackgroundSelectTab(1);
    BackgroundClickControl([](const auto& geometry){return geometry.motion;},.92f);
    const auto closedSettings=config;BackgroundCloseStudio(false);
    config=spatial_backdrop::Settings{};spatial::LoadBackdropPreferences();
    assert(BackgroundSettingsEqual(config,closedSettings));config.motion=true;BackgroundFrames();
    std::puts("PASS the close icon saves settings and dismisses both the popup and its screen-space input layer");

    // Decode and upload a real PNG through the browser and queued backend,
    // then render it before replacing/removing it. The unit harness separately
    // exercises allocation failures; this covers the real GL resource lifetime.
    const auto fixtureDirectory=std::filesystem::temp_directory_path()/
        ("lsrael-background-ui-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
    assert(std::filesystem::create_directory(fixtureDirectory));
    const auto fixture=fixtureDirectory/std::filesystem::u8path("雪 夜.png");
    const unsigned char png[]={137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,2,0,0,0,2,8,2,0,0,0,253,212,154,115,0,0,0,22,73,68,65,84,120,156,99,148,52,138,226,119,115,99,249,252,252,154,238,46,17,0,28,139,4,241,251,166,131,39,0,0,0,0,73,69,78,68,174,66,96,130};
    FILE* imageFile=_wfopen(fixture.c_str(),L"wb");assert(imageFile);
    assert(std::fwrite(png,1,sizeof(png),imageFile)==sizeof(png));assert(std::fclose(imageFile)==0);
    BackgroundOpenStudio();BackgroundSelectTab(2);
    spatial::StudioDirectory(fixtureDirectory.u8string());
    BackgroundClickControl([](const auto& geometry){return geometry.browse;});
    auto* fileBody=spatial::BackdropStudio().scroll.window;ImGui::SetScrollY(fileBody,fileBody->ScrollMax.y);BackgroundFrames(3);
    assert(spatial::BackdropLayout().files[0].GetWidth()>0);
    ClickRect(spatial::BackdropLayout().files[0],false);BackgroundFrames();
    const auto loaded=spatial_backdrop_image::Get().textureName;
    assert(loaded&&glIsTexture(loaded)&&spatial_backdrop_image::Get().width==2);
    assert(std::string(config.imagePath)==fixture.u8string()&&config.scene==spatial_backdrop::Custom);
    Frame(true);assert(glGetError()==GL_NO_ERROR);
    assert(spatial::ChooseBackdropImage("missing-background-fixture.png"));BackgroundFrames();
    assert(spatial_backdrop_image::Get().textureName==loaded&&glIsTexture(loaded)&&std::string(config.imagePath)==fixture.u8string());
    assert(spatial::ChooseBackdropImage(fixture.u8string().c_str()));BackgroundFrames();Frame(true);
    assert(glGetError()==GL_NO_ERROR&&spatial_backdrop_image::Get().textureName!=loaded&&!glIsTexture(loaded));
    BackgroundClickControl([](const auto& geometry){return geometry.remove;});
    assert(!*config.imagePath&&!spatial_backdrop_image::Get().texture);Frame(true);assert(glGetError()==GL_NO_ERROR);
    for(int index=0;index<spatial_backdrop_presets::Count;++index){
        assert(spatial_backdrop_image::GetPreset(index).textureName==presetTextures[index]);
        assert(glIsTexture(presetTextures[index]));
    }
    BackgroundCloseStudio(true);
    assert(std::filesystem::remove(fixture));assert(std::filesystem::remove(fixtureDirectory));
    std::puts("PASS the Chinese-named PNG is selected, rendered and replaced through real GL; failed loads preserve it and removal frees it");

    for(int mode=0;mode<3;++mode){
        state.threeD=mode!=1;state.exploded=mode!=0;spatial::Pose(23,-19,6);Settle();
        const float scale=state.scale,pitch=state.targetPitch,yaw=state.targetYaw,roll=state.targetRoll;
        BackgroundOpenStudio();BackgroundSelectTab(1);
        auto* popup=BackgroundPopupWindow();auto* body=spatial::BackdropStudio().scroll.window;
        assert(popup&&body&&spatial_transform::Get().frame.enabled);
        const auto& selected=spatial_transform::Get().selected;
        assert(std::find(selected.begin(),selected.end(),popup->DrawList)==selected.end());
        assert(std::find(selected.begin(),selected.end(),body->DrawList)==selected.end());
        assert(!spatial_transform::InputAt(spatial::BackdropLayout().popup.GetCenter()).enabled);
        bool foundRoot=false,foundBody=false;
        for(int i=0;i<ImGui::GetDrawData()->CmdListsCount;++i){
            foundRoot|=ImGui::GetDrawData()->CmdLists[i]==popup->DrawList;
            foundBody|=ImGui::GetDrawData()->CmdLists[i]==body->DrawList;
        }
        assert(foundRoot&&foundBody);
        const bool enabled=config.enabled;
        BackgroundClickControl([](const auto& geometry){return geometry.enabled;},.92f);assert(config.enabled!=enabled);
        BackgroundClickControl([](const auto& geometry){return geometry.enabled;},.92f);assert(config.enabled==enabled);
        assert(state.scale==scale&&state.targetPitch==pitch&&state.targetYaw==yaw&&state.targetRoll==roll);
        BackgroundCloseStudio(mode%2==0);
    }
    std::puts("PASS the popup and its child remain unprojected and clickable in 3D, exploded and combined views");

    state.threeD=false;state.exploded=false;spatial::ResetPose();width=720;height=1280;ResizeDrawable();
    BackgroundFrames();spatial::Recenter();Settle();BackgroundOpenStudio();BackgroundCheckBounds();
    for(int index=0;index<3;++index){BackgroundSelectTab(index);BackgroundCheckBounds();}
    BackgroundSelectTab(0);BackgroundClickControl([](const auto& geometry){return geometry.scenes[spatial_backdrop::BlueButterfly];});
    assert(config.scene==spatial_backdrop::BlueButterfly);BackgroundScrollTop();state.messageUntil=0;Capture("draw-only-background-portrait.ppm");
    BackgroundCloseStudio(true);width=1600;height=1000;ResizeDrawable();BackgroundFrames();spatial::Recenter();Settle();
    assert(std::memcmp(&drawingSettings,&绘制.按钮,sizeof(drawingSettings))==0&&commandCalls==commands&&exitCalls==exits);
    assert(!state.reduceMotion&&!state.overlayOpen&&!BackgroundPopupWindow());
    std::remove(profile.c_str());std::remove(sidecar.c_str());
    std::puts("PASS the 720x1280 studio remains within the display and preserves drawing controls, callbacks and balanced ImGui state");
}

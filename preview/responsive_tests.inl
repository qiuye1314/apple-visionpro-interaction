// Real UI interaction and projection coverage across phone and tablet sizes.
// Coordinates are obtained from emitted items; no desktop-only hit positions.
static void ResponsiveFrames(int count=3){
    for(int i=0;i<count;++i){Frame();CheckFiniteDrawData();}
}
static void ResponsiveCheckBounds(){
    const auto bounds=spatial::WindowBounds();
    assert(bounds.Min.x>=7.f&&bounds.Min.y>=7.f);
    assert(bounds.Max.x<=width-7.f&&bounds.Max.y<=height-7.f);
    CheckFiniteDrawData();
}
static void ResponsiveScrollTop(){
    auto* body=spatial::PageScroll().window;assert(body);
    ImGui::SetScrollY(body,0.f);ResponsiveFrames();
}
static void ResponsiveSelectPage(int page){
    ClickRect(spatial::ResponsiveLayout().nav[page]);ResponsiveFrames();
    assert(tab==page);ResponsiveScrollTop();
}
static void ResponsiveCapture(const char* name){
    spatial::Get().messageUntil=0;Capture(name);CheckFiniteDrawData();
    std::printf("CAPTURE %s\n",name);std::fflush(stdout);
}
static void ResponsiveTests(){
    auto& state=spatial::Get();auto& backdrop=spatial_backdrop::Get();
    const std::string profile=state.path,sidecar=spatial::BackdropConfigPath();
    std::remove(profile.c_str());std::remove(sidecar.c_str());
    state.reduceMotion=true;backdrop=spatial_backdrop::Settings{};backdrop.motion=false;
    Frame();for(int i=0;i<180&&spatial::IntroActive();++i)Frame();
    assert(!spatial::IntroActive());Settle();
    spatial::SetTheme(2,false);spatial_backdrop::SelectScene(spatial_backdrop::Snow);
    state.threeD=false;state.exploded=false;spatial::ResetPose();ResponsiveFrames();
    assert(std::fabs(spatial::Material().surface.w-.12f)<.001f);
    assert(std::fabs(spatial::Material().sidebar.w-.12f)<.001f);
    spatial::SetTheme(0,false);ResponsiveFrames();
    assert(std::fabs(spatial::Material().surface.w-.16f)<.001f);
    spatial::SetTheme(2,false);ResponsiveFrames();
    const ImVec2 sizes[]={
        {360,800},{720,1280},{1080,2400},{1440,3200},{1904,3040},
        {960,540},{800,360},{640,360},{1920,1080},{2400,1080},{1920,1200},
        {2560,1600},{3200,2000},{3840,2160},{1600,1000},
        {1024,768},{2048,1536},{768,1024},{1536,2048},{800,800}
    };
    int resolutions=0;std::vector<ImVec2> testedResolutions;
    for(const auto requested:sizes){
        const int targetWidth=(int)requested.x,targetHeight=(int)requested.y;
        std::printf("RESOLUTION %dx%d\n",targetWidth,targetHeight);std::fflush(stdout);
        state.threeD=false;state.exploded=false;state.layerBlend=0;
        state.pitch=state.yaw=state.roll=0;state.targetPitch=state.targetYaw=state.targetRoll=0;
        state.distance=0;state.gyroEnabled=false;state.gyroOffset={};
        width=targetWidth;height=targetHeight;ResizeDrawable();ResponsiveFrames();
        spatial::SetScale(spatial::DefaultScale());spatial::Recenter();ResponsiveFrames();
        CheckCentered();ResponsiveCheckBounds();
        const bool expectedCompact=width<height*1.12f;
        assert(spatial::CompactLayout()==expectedCompact);
        assert(std::fabs(state.scale-spatial::DefaultScale())<.005f);
        assert(state.scale<=spatial::MaximumScale()*.901f);
        if(width==3840&&height==2160)assert(state.scale>1.6f);
        const auto& layout=spatial::ResponsiveLayout();
        assert(layout.logicalWidth==(expectedCompact?384.f:1120.f));
        assert(layout.content.GetWidth()>200.f*state.scale);
        if(expectedCompact){
            assert(layout.nav[0].Min.y<layout.header.Min.y&&layout.header.Max.y<layout.content.Min.y);
            for(int i=1;i<4;++i)assert(layout.nav[i].Min.x>layout.nav[i-1].Max.x);
        }else{
            assert(layout.nav[0].Max.x<layout.content.Min.x);
            assert(layout.header.Max.y<layout.content.Min.y);
        }

        ResponsiveSelectPage(0);
        auto* home=spatial::PageScroll().window;assert(home);
        assert(std::fabs(home->CalcFontSize()-21.f*state.scale)<.25f);
        if(expectedCompact){
            assert(home->ScrollMax.y>100.f*state.scale);
            const auto quick0=spatial::ResponsiveLayout().quick[0];
            for(int i=1;i<3;++i){const auto quick=spatial::ResponsiveLayout().quick[i];
                assert(std::fabs(quick.Min.x-quick0.Min.x)<.5f);
                assert(quick.Min.y>quick0.Max.y);
            }
        }
        char name[112];
        std::snprintf(name,sizeof(name),"draw-only-responsive-%dx%d-home.ppm",width,height);
        ResponsiveCapture(name);

        ResponsiveSelectPage(1);
        auto* drawing=spatial::PageScroll().window;assert(drawing&&drawing->ScrollMax.y>0);
        assert(std::fabs(drawing->CalcFontSize()-21.f*state.scale)<.25f);
        const bool oldBox=绘制.按钮.方框;const auto beforeToggle=绘制.按钮;
        ClickWidget("方框绘制",.91f);assert(绘制.按钮.方框!=oldBox);
        auto afterToggle=绘制.按钮;afterToggle.方框=oldBox;
        assert(std::memcmp(&afterToggle,&beforeToggle,sizeof(beforeToggle))==0);
        ClickWidget("方框绘制",.91f);assert(绘制.按钮.方框==oldBox);
        ResponsiveScrollTop();
        std::snprintf(name,sizeof(name),"draw-only-responsive-%dx%d-drawing.ppm",width,height);
        ResponsiveCapture(name);
        const auto beforeSwipe=绘制.按钮;
        const float scrollBefore=drawing->Scroll.y;
        const auto swipeRow=FindWidget("方框绘制");
        const float swipeDistance=std::min(230.f*state.scale,drawing->InnerRect.GetHeight()*.65f);
        TouchSwipe({swipeRow.rect.Min.x+20.f*state.scale,swipeRow.rect.GetCenter().y},0,-swipeDistance);
        ResponsiveFrames(6);
        assert(drawing->Scroll.y>scrollBefore+5.f);
        assert(std::memcmp(&beforeSwipe,&绘制.按钮,sizeof(beforeSwipe))==0);
        assert(!spatial::PageScroll().active&&!spatial::PageScroll().candidate);

        const auto beforeMortar=绘制.按钮;
        const int beforeMortarSave=绘制.saveCalls;
        ClickWidget("迫击炮落点",.91f);
        assert(绘制.按钮.迫击炮落点!=beforeMortar.迫击炮落点);
        assert(绘制.saveCalls==beforeMortarSave+1);
        auto afterMortar=绘制.按钮;afterMortar.迫击炮落点=beforeMortar.迫击炮落点;
        assert(std::memcmp(&afterMortar,&beforeMortar,sizeof(beforeMortar))==0);
        ClickWidget("迫击炮落点",.91f);
        assert(绘制.按钮.迫击炮落点==beforeMortar.迫击炮落点);

        const auto last=ShowWidget("##骨骼距离");
        assert(last.clip.Contains(last.rect.GetCenter()));
        const float originalWidth=绘制.按钮.骨骼粗细;
        ClickNumberTrack("##骨骼粗细",originalWidth>1.5f?.15f:.85f);
        assert(std::fabs(绘制.按钮.骨骼粗细-originalWidth)>.01f);
        auto number=ShowWidget("##骨骼粗细");
        assert(number.slider.GetWidth()>20.f*state.scale&&number.number.GetWidth()>20.f*state.scale);
        assert(number.rect.Min.x>=number.clip.Min.x-2.f);
        assert(number.rect.Max.x<=number.clip.Max.x+2.f);
        ClickNumberButton("##骨骼粗细",0);ResponsiveFrames();
        assert(spatial::NumericEditor().active);
        const auto editor=spatial::NumericEditor().bounds;
        assert(editor.Min.x>=7&&editor.Min.y>=7&&editor.Max.x<=width-7&&editor.Max.y<=height-7);
        if(width==360||width==720||width==800||width==3840){
            std::snprintf(name,sizeof(name),"draw-only-responsive-%dx%d-number.ppm",width,height);ResponsiveCapture(name);
        }
        ClickRect(spatial::NumericEditor().cancelBounds,false);ResponsiveFrames();
        assert(!spatial::NumericEditor().active);
        绘制.按钮.骨骼粗细=originalWidth;

        for(int page=2;page<4;++page){
            ResponsiveSelectPage(page);auto* body=spatial::PageScroll().window;assert(body);
            assert(body->ScrollMax.x<1.f);
            ImGui::SetScrollY(body,body->ScrollMax.y);ResponsiveFrames();
            assert(body->Scroll.y<=body->ScrollMax.y+.5f);
            ResponsiveCheckBounds();
            std::snprintf(name,sizeof(name),"draw-only-responsive-%dx%d-%s.ppm",width,height,page==2?"items":"settings");
            ResponsiveCapture(name);
        }

        // Popups remain in screen space and fit even the 360 px viewport.
        ResponsiveSelectPage(1);ClickRect(spatial::ResponsiveLayout().theme);ResponsiveFrames();
        auto* popup=ThemePopup();assert(popup&&state.overlayOpen);
        assert(popup->Pos.x>=7&&popup->Pos.y>=7&&popup->Pos.x+popup->Size.x<=width-7&&popup->Pos.y+popup->Size.y<=height-7);
        CloseThemePopup();assert(!ThemePopup()&&!state.overlayOpen);
        BackgroundOpenStudio();BackgroundCheckBounds();
        for(int studioTab=0;studioTab<3;++studioTab){BackgroundSelectTab(studioTab);BackgroundCheckBounds();}
        BackgroundSelectTab(0);
        const auto beforePreset=backdrop;
        for(int scene=spatial_backdrop::StarryEye;scene<spatial_backdrop::SceneCount;++scene){
            BackgroundClickControl([&](const auto& geometry){return geometry.scenes[scene];});
            assert(backdrop.scene==scene&&backdrop.mode==0);
            assert(spatial::BackdropImageForScene(scene).texture);
            assert(spatial::BackdropStudio().scroll.window->ScrollMax.x<1.f);
            BackgroundCheckBounds();
        }
        BackgroundScrollTop();
        if(width==360||width==720||width==1904||width==3040||width==3840){
            std::snprintf(name,sizeof(name),"draw-only-responsive-%dx%d-presets.ppm",width,height);ResponsiveCapture(name);
        }
        backdrop=beforePreset;spatial_backdrop::ResetClock();BackgroundFrames();
        BackgroundSelectTab(1);
        const bool wasEnabled=backdrop.enabled;
        BackgroundClickControl([](const auto& geometry){return geometry.enabled;},.92f);assert(backdrop.enabled!=wasEnabled);
        BackgroundClickControl([](const auto& geometry){return geometry.enabled;},.92f);assert(backdrop.enabled==wasEnabled);
        if(width==360||width==720||width==800||width==3840){
            std::snprintf(name,sizeof(name),"draw-only-responsive-%dx%d-studio.ppm",width,height);ResponsiveCapture(name);
        }
        BackgroundCloseStudio(true);

        // Keep the user's relative zoom level when changing orientation.
        const float relative=.85f;
        spatial::SetScale(spatial::DefaultScale()*relative);spatial::Recenter();ResponsiveFrames();
        const float actualRelative=state.scale/state.layoutBaseScale;
        width=targetHeight;height=targetWidth;ResizeDrawable();ResponsiveFrames();
        assert(std::fabs(state.scale/state.layoutBaseScale-actualRelative)<.015f);
        CheckCentered();ResponsiveCheckBounds();
        width=targetWidth;height=targetHeight;ResizeDrawable();ResponsiveFrames();
        assert(std::fabs(state.scale/state.layoutBaseScale-actualRelative)<.015f);
        spatial::SetScale(spatial::DefaultScale());spatial::Recenter();ResponsiveFrames();

        // Buttons, projected widgets and the visible union work in both modes.
        for(int mode=0;mode<3;++mode){
            state.threeD=mode!=1;state.exploded=mode!=0;
            spatial::Pose(20.f,-15.f,5.f);ResponsiveFrames(5);spatial::Recenter();ResponsiveFrames(5);
            CheckCentered();ResponsiveCheckBounds();ResponsiveScrollTop();
            const bool value=绘制.按钮.方框;
            ClickWidget("方框绘制",.91f);assert(绘制.按钮.方框!=value);
            ClickWidget("方框绘制",.91f);assert(绘制.按钮.方框==value);
        }
        if(width==360||width==720||width==1920||width==3840){
            ResponsiveScrollTop();std::snprintf(name,sizeof(name),"draw-only-responsive-%dx%d-exploded.ppm",width,height);
            ResponsiveCapture(name);
        }
        state.threeD=false;state.exploded=false;state.layerBlend=0;
        spatial::ResetPose();spatial::SetScale(spatial::DefaultScale());spatial::Recenter();ResponsiveFrames();

        // Every page retains accessible exit/restore targets after reflow.
        for(int page=0;page<4;++page){
            ResponsiveSelectPage(page);const int previous=exitCalls;
            ClickRect(spatial::SidebarActionsLayout().exit);assert(exitCalls==previous+1);
        }
        ClickRect(spatial::SidebarActionsLayout().hide);ResponsiveFrames();assert(!opened);
        const int calls=pageCalls;ResponsiveFrames();assert(pageCalls==calls);
        opened=true;ResponsiveFrames();CheckCentered();ResponsiveCheckBounds();
        std::printf("PASS responsive layout %dx%d: %s reflow, four pages, click/scroll, numeric editor, theme, rotation, 3D/explode, centered restore\n",
            targetWidth,targetHeight,expectedCompact?"compact":"wide");std::fflush(stdout);
        testedResolutions.push_back(requested);++resolutions;
    }
    width=1600;height=1000;ResizeDrawable();ResponsiveFrames();
    tab=0;spatial::SetScale(spatial::DefaultScale());spatial::Recenter();ResponsiveFrames();
    std::remove(profile.c_str());std::remove(sidecar.c_str());
    FILE* matrix=std::fopen("responsive-matrix.json","wb");assert(matrix);
    std::fprintf(matrix,"{\"tested_resolutions_px\":[");
    for(size_t i=0;i<testedResolutions.size();++i){
        if(i)std::fprintf(matrix,",");
        std::fprintf(matrix,"[%d,%d]",(int)testedResolutions[i].x,(int)testedResolutions[i].y);
    }
    std::fprintf(matrix,"]}\n");assert(std::fclose(matrix)==0);
    std::printf("PASS responsive matrix completed: %d resolutions, liquid material alpha, parent-corrected nested fonts\n",resolutions);
}

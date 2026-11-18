static void DrawOnlyGallery(){
    auto& s=spatial::Get();const int previousTab=tab,previousTheme=s.theme;
    const auto previousSettings=绘制.按钮;绘制.按钮=DemoSettings{};
    s.threeD=false;s.exploded=false;spatial::ResetPose();spatial::SetScale(1);spatial::Recenter();Settle();
    const char* pages[]={"home","drawing","items","settings"};
    for(int theme=0;theme<spatial_theme::Count;++theme)for(int page=0;page<4;++page){
        spatial::SetTheme(theme,false);tab=page;Frame();Frame();
        if(spatial::PageScroll().window){ImGui::SetScrollY(spatial::PageScroll().window,0);Frame();Frame();}
        s.messageUntil=0;char filename[100];
        std::snprintf(filename,sizeof(filename),"draw-only-%s-theme-%d.ppm",pages[page],theme);Capture(filename);
    }
    tab=1;spatial::SetTheme(3,false);Frame();Frame();
    const auto occlusion=FindWidget("人物掩体漏打");
    ImGui::SetScrollY(occlusion.window,occlusion.window->Scroll.y+occlusion.rect.GetCenter().y-occlusion.clip.GetCenter().y);
    Frame();Frame();s.messageUntil=0;Capture("draw-only-person-occlusion.ppm");
    ImGui::SetScrollY(occlusion.window,0);Frame();Frame();
    tab=1;spatial::SetTheme(3,false);s.threeD=true;s.exploded=true;spatial::Pose(19,-16,3);Settle();
    s.messageUntil=0;Capture("draw-only-exploded.ppm");
    s.threeD=false;s.exploded=false;spatial::ResetPose();spatial::SetTheme(previousTheme,false);tab=previousTab;
    绘制.按钮=previousSettings;Settle();
}

static void CheckFiniteDrawData(){
    const auto* data=ImGui::GetDrawData();
    for(int n=0;n<data->CmdListsCount;++n)for(const auto& vertex:data->CmdLists[n]->VtxBuffer){
        assert(spatial_motion::Finite(vertex.pos.x)&&spatial_motion::Finite(vertex.pos.y));
        assert(std::fabs(vertex.pos.x)<1e6f&&std::fabs(vertex.pos.y)<1e6f);
    }
}

static void DrawOnlyTests(){
    auto& s=spatial::Get();std::remove(s.path);s.reduceMotion=true;
    Frame();assert(spatial::IntroActive());while(spatial::IntroActive())Frame();Settle();
    assert(opened&&tab==0&&!s.gyroEnabled);CheckCentered();
    std::puts("PASS startup completes with centered UI and physical motion off by default");

    tab=1;Frame();Frame();
    const bool box=绘制.按钮.方框,bone=绘制.按钮.骨骼,ray=绘制.按钮.射线;
    ClickWidget("方框绘制",.91f);assert(绘制.按钮.方框!=box&&绘制.按钮.骨骼==bone&&绘制.按钮.射线==ray);
    ClickWidget("骨骼绘制",.91f);assert(绘制.按钮.骨骼!=bone&&绘制.按钮.射线==ray);
    ClickWidget("射线绘制",.91f);assert(绘制.按钮.射线!=ray);
    std::puts("PASS person box, skeleton and ray controls respond independently");
    for(const auto& widget:observedWidgets)
        assert(widget.label.find("追踪")==std::string::npos&&widget.label.find("自瞄")==std::string::npos);
    std::puts("PASS drawing page exposes display controls without removed feature widgets");

    assert(!绘制.按钮.掩体识别);
    const auto displaySettings=绘制.按钮;
    ClickWidget("人物掩体漏打",.91f);assert(绘制.按钮.掩体识别);
    auto changedSettings=绘制.按钮;changedSettings.掩体识别=displaySettings.掩体识别;
    assert(std::memcmp(&displaySettings,&changedSettings,sizeof(displaySettings))==0);
    ClickWidget("人物掩体漏打",.91f);assert(!绘制.按钮.掩体识别);
    std::puts("PASS person occlusion remains available as an independent switch, off by default");

    auto* content=FindWidget("方框绘制").window;ImGui::SetScrollY(content,0);Frame();Frame();
    const auto before=绘制.按钮;const float initialScroll=content->Scroll.y;
    auto start=FindWidget("方框绘制");
    TouchSwipe({start.rect.Min.x+25*s.scale,start.rect.GetCenter().y},0,-260*s.scale);
    Settle();assert(content->Scroll.y>initialScroll+10);
    assert(std::memcmp(&before,&绘制.按钮,sizeof(before))==0);
    std::puts("PASS a vertical finger swipe reaches lower rows without toggling settings");

    const auto last=ShowWidget("##骨骼距离");assert(last.clip.Contains(last.rect.GetCenter()));
    std::puts("PASS lower drawing settings remain reachable through scrolling");
    auto oldWidth=绘制.按钮.骨骼粗细;
    ClickNumberTrack("##骨骼粗细",.7f);assert(绘制.按钮.骨骼粗细!=oldWidth);
    auto slider=ShowWidget("##骨骼粗细");assert(slider.slider.GetWidth()>0&&slider.number.GetWidth()>0);
    std::puts("PASS line width uses a working slider and separate number entry target");

    ClickNumberButton("##骨骼粗细",0);Frame();Frame();assert(spatial::NumericEditor().active);
    const auto unchanged=绘制.按钮.骨骼粗细;
    ClickRect(spatial::NumericEditor().cancelBounds,false);Frame();Frame();
    assert(!spatial::NumericEditor().active&&绘制.按钮.骨骼粗细==unchanged);
    std::puts("PASS cancelling numeric entry preserves the existing width");

    for(int page=2;page<4;++page){
        tab=page;Frame();Frame();auto* w=spatial::PageScroll().window;assert(w);
        ImGui::SetScrollY(w,w->ScrollMax.y);Frame();Frame();assert(w->Scroll.y<=w->ScrollMax.y+.5f);
    }
    std::puts("PASS item and settings pages fit or scroll to their final rows");
    for(int theme=0;theme<spatial_theme::Count;++theme){
        spatial::SetTheme(theme,false);
        for(int page=0;page<4;++page){tab=page;Frame();Frame();CheckFiniteDrawData();}
    }
    std::puts("PASS all seven themes render four pages with balanced style stacks and finite geometry");

    tab=0;Frame();ClickAt(1034,58);Frame();assert(ThemePopup()&&s.overlayOpen);
    CloseThemePopup();assert(!ThemePopup()&&!s.overlayOpen);
    std::puts("PASS the global theme picker opens and dismisses cleanly");

    s.threeD=true;s.exploded=true;spatial::Pose(25,-21,6);Settle();
    tab=1;Frame();const auto tiltedValue=绘制.按钮.方框;ClickWidget("方框绘制",.91f);
    assert(绘制.按钮.方框!=tiltedValue);
    std::puts("PASS projected drawing controls remain clickable in combined 3D and exploded mode");

    for(const auto size:{ImVec2(960,540),ImVec2(720,1280),ImVec2(1600,1000)}){
        width=(int)size.x;height=(int)size.y;Frame();spatial::Recenter();Settle();
        const auto bounds=spatial::WindowBounds();
        assert(bounds.Min.x>=7&&bounds.Min.y>=7&&bounds.Max.x<=width-7&&bounds.Max.y<=height-7);
        CheckFiniteDrawData();
    }
    std::puts("PASS landscape, compact and portrait layouts keep the projected panel within screen bounds");

    s.threeD=false;s.exploded=false;spatial::ResetPose();Settle();
    const int calls=pageCalls;opened=false;Frame();Frame();assert(pageCalls==calls);
    opened=true;Frame();Settle();CheckCentered();
    std::puts("PASS hiding pauses page rendering and restoring recenters the panel");

    for(int page=0;page<4;++page){
        tab=page;Frame();const int count=exitCalls;
        ClickRect(spatial::SidebarActionsLayout().exit);assert(exitCalls==count+1);
    }
    std::puts("PASS exit is reachable from all four pages");
    DrawOnlyGallery();std::remove(s.path);
}

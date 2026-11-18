// Included inside namespace spatial, after common controls.
struct BackdropStudioGeometry {
    ImRect entry,popup,close,done,reset,tabs[3],scenes[spatial_backdrop::SceneCount],modes[2],pool[spatial_backdrop::SceneCount];
    ImRect enabled,motion,follow,sliders[6],browse,remove,back,files[512],applyCustom,effects[6];
    int fileCount=0;
};
inline BackdropStudioGeometry& BackdropLayout(){static BackdropStudioGeometry value;return value;}
struct BackdropStudioState {
    int tab=0;
    bool browsing=false,listed=false;
    spatial_backdrop_files::Listing listing;
    PageScrollState scroll;
};
inline BackdropStudioState& BackdropStudio(){static BackdropStudioState value;return value;}
inline ImRect StudioItem(){return {ImGui::GetItemRectMin(),ImGui::GetItemRectMax()};}
inline void StudioFont(float size){
    auto* window=ImGui::GetCurrentWindow();
    const float parent=window->ParentWindow?window->ParentWindow->FontWindowScale:1.f;
    ImGui::SetWindowFontScale(size/(GImGui->FontBaseSize*parent));
}
inline void StudioDirectory(const std::string& path){auto& ui=BackdropStudio();ui.listing=spatial_backdrop_files::List(path);ui.listed=true;}
inline void StudioBeginScroll(){
    auto& scroll=BackdropStudio().scroll;auto* w=ImGui::GetCurrentWindow();auto& io=ImGui::GetIO();
    const auto id=w->GetID("##background-pan");
    if(scroll.window!=w||scroll.page!=BackdropStudio().tab){scroll=PageScrollState{};}
    scroll.window=w;scroll.page=BackdropStudio().tab;scroll.id=id;
    scroll.clip=w->InnerRect;scroll.clip.ClipWith(w->ClipRect);
    if(ImGui::IsMouseClicked(0)&&w->ScrollMax.y>0&&scroll.clip.Contains(io.MousePos)&&ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)){
        bool blocked=io.MousePos.x>w->InnerRect.Max.x-14*Get().scale;
        for(const auto& rect:scroll.exclusions)if(rect.Contains(io.MousePos))blocked=true;
        scroll.candidate=!blocked;scroll.active=false;scroll.start=io.MousePos;scroll.startScroll=w->Scroll.y;
    }
    if(scroll.candidate){
        const float dx=io.MousePos.x-scroll.start.x,dy=io.MousePos.y-scroll.start.y;
        if(!scroll.active&&std::fabs(dy)>8*Get().scale&&std::fabs(dy)>.85f*std::fabs(dx)){
            scroll.active=true;ImGui::SetActiveID(id,w);ImGui::SetKeyOwner(ImGuiKey_MouseLeft,id);
        }
        if(scroll.active){ImGui::KeepAliveID(id);ImGui::SetScrollY(w,Clamp(scroll.startScroll-dy,0,w->ScrollMax.y));}
        else if(std::fabs(dx)>10*Get().scale&&std::fabs(dx)>1.35f*std::fabs(dy))scroll.candidate=false;
    }
    scroll.exclusions.clear();
}
inline void StudioEndScroll(){
    auto& scroll=BackdropStudio().scroll;
    if(!ImGui::GetIO().MouseDown[0]){
        if(scroll.active&&GImGui->ActiveId==scroll.id)ImGui::ClearActiveID();
        scroll.candidate=scroll.active=false;
    }
}
inline void StudioHint(const char* text){ImGui::PushStyleColor(ImGuiCol_Text,Theme().muted);ImGui::TextWrapped("%s",text);ImGui::PopStyleColor();}
inline void StudioSlider(const char* label,float* value,float lo,float hi,const char* format,int index){
    const float s=Get().scale;ImGui::TextUnformatted(label);ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);ImGui::PushID(label);
    ImGui::SliderFloat("##value",value,lo,hi,format,ImGuiSliderFlags_NoInput);
    BackdropLayout().sliders[index]=StudioItem();BackdropStudio().scroll.exclusions.push_back(StudioItem());
    ImGui::PopID();ImGui::Dummy({0,7*s});
}
inline void CloseBackdropStudio(){
    if(!SaveBackdropPreferences())Notice("背景设置保存失败，请检查目录写入权限");
    BackdropStudio().browsing=false;BackdropStudio().scroll=PageScrollState{};
    ImGui::CloseCurrentPopup();
}
inline void RenderBackdropStudio(){
    auto& state=Get();auto& ui=BackdropStudio();auto& layout=BackdropLayout();
    const ImRect entry=layout.entry;layout=BackdropStudioGeometry{};layout.entry=entry;
    if(!ImGui::IsPopupOpen("##background-studio"))return;
    const float priorScale=state.scale;const ImVec2 display=ImGui::GetIO().DisplaySize;
    const bool landscape=display.x>display.y*1.4f&&display.y<650.f;
    const bool compact=!landscape&&(display.x<900.f||display.x<display.y*.82f);
    const float logicalW=compact?384.f:760.f,logicalH=landscape?420.f:700.f,bodyW=logicalW-48.f;
    const float previewY=landscape?72.f:84.f,previewH=landscape?58.f:136.f;
    const float tabsY=landscape?142.f:234.f,bodyY=landscape?190.f:288.f,bodyH=landscape?158.f:342.f;
    const float footerLineY=landscape?358.f:644.f,footerY=landscape?368.f:654.f;
    const float fit=std::min((display.x-24.f)/logicalW,(display.y-24.f)/logicalH);
    const float preferred=(compact||landscape)?fit:std::min(display.x/1600.f,display.y/1000.f);
    const float s=std::max(.1f,std::min(fit,preferred));
    state.scale=s;
    const ImVec2 size{logicalW*s,logicalH*s};
    ImGui::SetNextWindowSize(size,ImGuiCond_Always);
    const bool resized=BackdropStudio().scroll.window&&
        (std::fabs(BackdropStudio().scroll.window->RootWindow->Size.x-size.x)>1.f||
         std::fabs(BackdropStudio().scroll.window->RootWindow->Size.y-size.y)>1.f);
    ImGui::SetNextWindowPos({display.x*.5f,display.y*.5f},resized?ImGuiCond_Always:ImGuiCond_Appearing,{.5f,.5f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,24*s);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{12*s,8*s});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{10*s,10*s});
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize,12*s);
    ImGui::PushStyleColor(ImGuiCol_PopupBg,{0,0,0,0});
    ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg,{0,0,0,0});
    ImVec4 scrollbar=Theme().accent;scrollbar.w=.4f;
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab,scrollbar);
    if(ImGui::BeginPopupModal("##background-studio",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoMove)){
        GImGui->DimBgRatio=0.f;
        auto* win=ImGui::GetCurrentWindow();auto* d=win->DrawList;ImVec2 p=win->Pos;
        p.x=Clamp(p.x,8.f,std::max(8.f,display.x-size.x-8));p.y=Clamp(p.y,8.f,std::max(8.f,display.y-size.y-8));
        if(p.x!=win->Pos.x||p.y!=win->Pos.y)ImGui::SetWindowPos(p);
        layout.popup=ImRect(p,Add(p,size));
        if(state.overlayOpen)state.overlayBounds.Add(layout.popup);else state.overlayBounds=layout.popup;
        state.overlayOpen=true;
        StudioFont(16*s);
        spatial_chrome::Panel(d,p,size,24*s,Material(),s,state.alpha,.16f);
        auto at=[&](float x,float y){ImGui::SetCursorScreenPos(Add(p,{x*s,y*s}));};
        Text(d,Add(p,{26*s,18*s}),10*s,Paint(Theme().accent),"AMBIENT / BACKGROUND");
        Text(d,Add(p,{26*s,37*s}),25*s,Ink(),"背景空间");
        at(20,10);ImGui::InvisibleButton("##studio-drag",{(logicalW-110)*s,62*s});
        if(ImGui::IsItemActive()&&ImGui::IsMouseDragging(0,3*s))ImGui::SetWindowPos(Add(p,ImGui::GetIO().MouseDelta));
        at(logicalW-66,23);if(Pill("##studio-close","×",{40*s,36*s}))CloseBackdropStudio();layout.close=StudioItem();
        const ImVec2 preview=Add(p,{24*s,previewY*s});
        BackdropSurface(d,preview,{bodyW*s,previewH*s},18*s,state.alpha);
        spatial_chrome::Panel(d,preview,{bodyW*s,previewH*s},18*s,Material(),s,state.alpha,.16f);
        Text(d,Add(preview,{28*s,(landscape?12.f:28.f)*s}),landscape?18*s:24*s,Ink(),spatial_backdrop::SceneName(spatial_backdrop::CurrentScene()));
        Text(d,Add(preview,{29*s,(landscape?36.f:66.f)*s}),10*s,Muted(),spatial_backdrop::Get().mode?"自动轮换 · 实时预览":"固定场景 · 实时预览");
        const char* tabs[]={"场景","动效","自定义"};
        const float tabW=(bodyW-16.f)/3.f;
        for(int i=0;i<3;++i){at(24+i*(tabW+8),tabsY);ImGui::PushID(i);
            if(Pill("##studio-tab",tabs[i],{tabW*s,38*s},ui.tab==i)){ui.tab=i;ui.scroll=PageScrollState{};}
            layout.tabs[i]=StudioItem();ImGui::PopID();}
        at(24,bodyY);ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{4*s,6*s});
        ImGui::PushStyleColor(ImGuiCol_ChildBg,{0,0,0,0});
        ImGui::BeginChild("##background-studio-body",{bodyW*s,bodyH*s},false,ImGuiWindowFlags_AlwaysVerticalScrollbar);
        ImGui::PopStyleColor();ImGui::PopStyleVar();StudioFont(16*s);StudioBeginScroll();
        auto& cfg=spatial_backdrop::Get();
        if(ui.tab==0){
            const int columns=compact?2:3;
            const float cardW=(ImGui::GetContentRegionAvail().x-(columns-1)*10*s)/columns;
            const char* subtitles[]={"缓慢飘落，轻盈有层次","斜向细雨，细小涟漪","疏朗星点，缓缓闪烁","温暖光点，轻柔游动","柔和光带，安静流转","无粒子，简洁渐变"};
            auto sceneCard=[&](int i,const char* subtitle,int column,bool last){ImGui::PushID(i);const ImVec2 q=ImGui::GetCursorScreenPos();
                if(ImGui::InvisibleButton("##scene",{cardW,112*s})){cfg.mode=0;spatial_backdrop::SelectScene(i);}
                layout.scenes[i]=StudioItem();auto* cd=ImGui::GetWindowDrawList();
                BackdropSurface(cd,q,{cardW,112*s},13*s,state.alpha,i,true);
                spatial_chrome::Panel(cd,q,{cardW,112*s},13*s,Material(),s,state.alpha,.16f);
                Text(cd,Add(q,{12*s,70*s}),17*s,Ink(),spatial_backdrop::SceneName(i));
                Text(cd,Add(q,{12*s,94*s}),10*s,Muted(),subtitle);
                if(cfg.scene==i)cd->AddRect(q,Add(q,{cardW,112*s}),Paint(Theme().accent),13*s,0,1.8f*s);
                ImGui::PopID();if(column!=columns-1&&!last)ImGui::SameLine(0,10*s);
            };
            ImGui::TextUnformatted("内置图片");StudioHint("已内置，可直接使用，也可加入下方轮换。");
            const char* pictureSubtitles[]={"蓝色星河，静谧光影","水色泡影，轻透梦境","幽蓝蝶翼，细腻层次"};
            for(int i=0;i<spatial_backdrop_presets::Count;++i){
                sceneCard(spatial_backdrop::StarryEye+i,pictureSubtitles[i],i%columns,i==spatial_backdrop_presets::Count-1);
            }
            ImGui::Dummy({0,8*s});ImGui::TextUnformatted("动态场景");
            for(int i=0;i<6;++i)sceneCard(i,subtitles[i],i%columns,i==5);
            ImGui::Dummy({0,8*s});
            const float half=(ImGui::GetContentRegionAvail().x-10*s)*.5f;
            if(Pill("##fixed","固定场景",{half,38*s},cfg.mode==0))cfg.mode=0;layout.modes[0]=StudioItem();
            ImGui::SameLine(0,10*s);
            if(Pill("##rotate","自动轮换",{half,38*s},cfg.mode==1)){cfg.mode=1;if(!(cfg.rotationMask&(1u<<cfg.scene)))spatial_backdrop::SelectScene(spatial_backdrop::FirstInPool(cfg.rotationMask));}layout.modes[1]=StudioItem();
            ImGui::Spacing();StudioHint("天气可播放动画，图片保持静态。轮换仅使用下面勾选的场景，暂停请到「动效」。");
            StudioSlider("轮换间隔",&cfg.interval,5,180,"%.0f 秒",0);
            const float poolStart=ImGui::GetCursorPosX(),poolStep=ImGui::GetContentRegionAvail().x/columns;
            for(int entry=0;entry<spatial_backdrop::SceneCount-1;++entry){
                const int i=entry<spatial_backdrop_presets::Count?spatial_backdrop::StarryEye+entry:entry-spatial_backdrop_presets::Count;
                ImGui::PushID(i);bool checked=(cfg.rotationMask&(1u<<i))!=0;
                if(ImGui::Checkbox(spatial_backdrop::SceneName(i),&checked)){
                    if(checked)cfg.rotationMask|=1u<<i;
                    else if((cfg.rotationMask&~(1u<<i))!=0)cfg.rotationMask&=~(1u<<i);
                }layout.pool[i]=StudioItem();ImGui::PopID();if(entry%columns!=columns-1&&entry<spatial_backdrop::SceneCount-2)ImGui::SameLine(poolStart+(entry%columns+1)*poolStep);}
            ImGui::Spacing();if(Pill("##custom-scene","使用自己的图片或渐变",{ImGui::GetContentRegionAvail().x,40*s})){ui.tab=2;ui.scroll=PageScrollState{};}layout.scenes[6]=StudioItem();
        }else if(ui.tab==1){
            Toggle("显示背景",&cfg.enabled);layout.enabled=StudioItem();
            Toggle("播放动态效果",&cfg.motion);layout.motion=StudioItem();
            ImGui::Spacing();StudioHint(state.reduceMotion?"全局「减少动态效果」已开启，当前保持静止。关闭后恢复播放。":"收起悬浮窗时暂停播放，重新展开会从原进度继续。");
            ImGui::Dummy({0,10*s});
            StudioSlider("粒子密度",&cfg.density,0,1,"%.2f",1);
            StudioSlider("运动速度",&cfg.speed,.1f,3,"%.1f 倍",2);
            StudioSlider("背景强度",&cfg.opacity,0,1,"%.2f",3);
            StudioSlider("玻璃雾度",&cfg.dim,0,.85f,"%.2f",4);
            StudioSlider("风向偏移",&cfg.wind,-1,1,"%.2f",5);
        }else{
            Toggle("渐变跟随全局配色",&cfg.followTheme);layout.follow=StudioItem();
            if(!cfg.followTheme){
                ImGui::ColorEdit3("顶部色",&cfg.top.x,ImGuiColorEditFlags_NoInputs);ui.scroll.exclusions.push_back(StudioItem());
                ImGui::ColorEdit3("底部色",&cfg.bottom.x,ImGuiColorEditFlags_NoInputs);ui.scroll.exclusions.push_back(StudioItem());
            }
            if(Pill("##apply-custom","启用自定义背景",{ImGui::GetContentRegionAvail().x,38*s},cfg.scene==spatial_backdrop::Custom)){
                cfg.mode=0;spatial_backdrop::SelectScene(spatial_backdrop::Custom);
            }layout.applyCustom=StudioItem();
            ImGui::Spacing();ImGui::TextUnformatted("叠加动效");
            const float effectW=(ImGui::GetContentRegionAvail().x-20*s)/3;
            for(int i=0;i<6;++i){ImGui::PushID(i);
                if(Pill("##effect",i==5?"不叠加":spatial_backdrop::SceneName(i),{effectW,34*s},cfg.customEffect==i))cfg.customEffect=i;
                layout.effects[i]=StudioItem();
                ImGui::PopID();if(i%3!=2)ImGui::SameLine(0,10*s);}
            ImGui::Dummy({0,8*s});
            if(Pill("##browse-image",ui.browsing?"收起文件列表":"选择本地图片",{ImGui::GetContentRegionAvail().x,40*s},ui.browsing)){
                ui.browsing=!ui.browsing;if(ui.browsing&&!ui.listed)StudioDirectory(*cfg.imagePath?spatial_backdrop_files::Parent(cfg.imagePath):spatial_backdrop_files::DefaultDirectory());
            }layout.browse=StudioItem();
            if(*cfg.imagePath){
                StudioHint(cfg.imagePath);
                if(Pill("##remove-image","移除图片，使用渐变",{ImGui::GetContentRegionAvail().x,36*s})){cfg.imagePath[0]=0;}
                layout.remove=StudioItem();
            }
            auto image=BackdropImage();if(image.error&&*image.error)StudioHint(image.error);
            if(ui.browsing){
                StudioHint("PNG / JPEG · 最长边 8192 像素 · 最大 32 MB");
                if(Pill("##parent-dir","返回上一级",{ImGui::GetContentRegionAvail().x,36*s}))StudioDirectory(spatial_backdrop_files::Parent(ui.listing.directory));layout.back=StudioItem();
#if defined(__ANDROID__)
                const float shortcutW=(ImGui::GetContentRegionAvail().x-20*s)/3;
                if(Pill("##pictures","图片",{shortcutW,34*s}))StudioDirectory("/sdcard/Pictures");ImGui::SameLine();
                if(Pill("##download","下载",{shortcutW,34*s}))StudioDirectory("/sdcard/Download");ImGui::SameLine();
                if(Pill("##storage","手机存储",{shortcutW,34*s}))StudioDirectory("/sdcard");
#endif
                StudioHint(ui.listing.directory.c_str());if(!ui.listing.error.empty())StudioHint(ui.listing.error.c_str());
                if(ui.listing.entries.empty())StudioHint("此目录没有可显示的图片或文件夹。");
                std::string enter,choose;
                ImGuiListClipper clipper;clipper.Begin((int)ui.listing.entries.size());
                while(clipper.Step())for(int i=clipper.DisplayStart;i<clipper.DisplayEnd;++i){const auto& item=ui.listing.entries[i];ImGui::PushID(i);
                    const ImVec2 q=ImGui::GetCursorScreenPos();const float rowW=ImGui::GetContentRegionAvail().x;
                    if(ImGui::InvisibleButton("##file",{rowW,42*s})){if(item.directory)enter=item.path;else choose=item.path;}
                    layout.files[i]=StudioItem();++layout.fileCount;auto* cd=ImGui::GetWindowDrawList();
                    cd->AddRectFilled(q,Add(q,{rowW,42*s}),Paint(ImGui::IsItemHovered()?Material().hover:Material().field),8*s);
                    Icon(cd,item.directory?Box:Display,Add(q,{12*s,11*s}),20*s,Paint(Theme().accent));
                    cd->PushClipRect(Add(q,{42*s,0}),Add(q,{rowW-10*s,42*s}),true);
                    Text(cd,Add(q,{42*s,13*s}),15*s,Ink(),item.name.c_str());cd->PopClipRect();ImGui::PopID();
                }
                if(!enter.empty()){StudioDirectory(enter);ImGui::SetScrollY(0);}
                if(!choose.empty())ChooseBackdropImage(choose.c_str());
            }
        }
        StudioEndScroll();ImGui::EndChild();
        d->AddLine(Add(p,{24*s,footerLineY*s}),Add(p,{(logicalW-24)*s,footerLineY*s}),Paint(Theme().border,.7f),s);
        at(24,footerY);if(Pill("##background-reset","恢复默认",{126*s,32*s})){spatial_backdrop::Get()=spatial_backdrop::Settings{};spatial_backdrop::ResetClock();}layout.reset=StudioItem();
        if(!compact)Text(d,Add(p,{170*s,(footerY+10)*s}),12*s,Muted(),"实时生效 · 关闭时保存");
        at(logicalW-150,footerY-2);if(Pill("##background-done","完成",{126*s,36*s},true))CloseBackdropStudio();layout.done=StudioItem();
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor(3);ImGui::PopStyleVar(6);state.scale=priorScale;
}

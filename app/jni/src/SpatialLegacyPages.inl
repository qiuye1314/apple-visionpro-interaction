// Drawing-only controls. All settings belong to the standalone drawing profile.
namespace draw_only_controls {
inline void Save(){绘制.保存配置("lsrael_draw_139");}
inline void Toggle(const char* label,bool* value){
    const bool previous=*value;ModernStyleToggle(label,value);
    if(previous!=*value)Save();
}
inline void BeginRow(const char* label){
    spatial::BeginDrawingRow(label);
    const float controlX=ImGui::GetCursorPosX()+ImGui::GetContentRegionAvail().x*.36f;
    ImGui::AlignTextToFramePadding();ImGui::TextUnformatted(label);
    ImGui::SameLine();ImGui::SetCursorPosX(controlX);
    ImGui::PushItemWidth(-12.f*spatial::Get().scale);
}
inline void EndRow(){ImGui::PopItemWidth();spatial::EndDrawingRow();}
inline void Slider(const char* label,const char* id,float* value,float minimum,float maximum,const char* format="%.1f"){
    BeginRow(label);
    if(spatial::NumberSlider(id,value,minimum,maximum,format))Save();
    EndRow();
}
inline void Note(const char* id,const char* text){
    spatial::BeginDrawingRow(id);
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+ImGui::GetContentRegionAvail().x-12.f*spatial::Get().scale);
    ImGui::TextDisabled("%s",text);ImGui::PopTextWrapPos();
    spatial::EndDrawingRow();
}
}

static void DrawSpatialPage(int currentTab){
    using namespace draw_only_controls;
    const float s=spatial::Get().scale;
    if(currentTab==0){
        if(spatial::DrawingSection("运行控制")){
            spatial::BeginDrawingRow("initialize-drawing");
            if(ImGui::Button("初始化绘制",{-12*s,42*s})){
                GetBase();模型初始化=true;绘制.按钮.绘制=true;
                spatial::Notice("绘制已初始化");
            }
            spatial::EndDrawingRow();
            Toggle("绘制总开关",&绘制.按钮.绘制);
            Note("edition-note","1.39 绘制专版 · 人物、物资与载具信息使用独立配置。");
        }
    }else if(currentTab==1){
        if(spatial::DrawingSection("人物轮廓")){
            Toggle("绘制总开关",&绘制.按钮.绘制);
            Toggle("方框绘制",&绘制.按钮.方框);
            Toggle("骨骼绘制",&绘制.按钮.骨骼);
            Toggle("显示头部节点",&绘制.按钮.显示头部骨骼点);
            Note("visual-language","细线角框、清晰骨骼与轻量描边。标记颜色跟随顶部主题，减少对画面的遮挡。");
        }
        if(spatial::DrawingSection("信息标记")){
            Toggle("名字绘制",&绘制.按钮.名字);
            Toggle("血量绘制",&绘制.按钮.血量);
            Toggle("距离绘制",&绘制.按钮.距离);
            Toggle("手持绘制",&绘制.按钮.手持);
            Toggle("射线绘制",&绘制.按钮.射线);
            Toggle("忽略人机",&绘制.按钮.忽略人机);
            Toggle("忽略倒地",&绘制.按钮.忽略倒地);
            Toggle("人物掩体漏打",&绘制.按钮.掩体识别);
            Note("occlusion-note","区分可见与遮挡骨骼，标记颜色随全局主题。");
        }
        if(spatial::DrawingSection("辅助视图")){
            Toggle("人数统计",&绘制.按钮.人数);
            Toggle("全图统计",&绘制.按钮.绘制全图);
            Toggle("绘制雷达",&绘制.按钮.雷达);
            Toggle("背敌预警",&绘制.按钮.背敌预警);
            Toggle("手雷预警",&绘制.按钮.手雷预警);
            Toggle("迫击炮落点",&绘制.按钮.迫击炮落点);
            Note("mortar-note","显示落点位置、距离及世界坐标，标记颜色跟随全局主题。");
        }
        if(spatial::DrawingSection("布局与线条")){
            Slider("方框粗细","##方框粗细",&绘制.按钮.方框粗细,.6f,3.f);
            Slider("骨骼粗细","##骨骼粗细",&绘制.按钮.骨骼粗细,.6f,3.f);
            Slider("射线粗细","##射线粗细",&绘制.按钮.射线粗细,.6f,2.4f);
            Slider("骨骼距离","##骨骼距离",&绘制.骨骼距离限制,30.f,500.f,"%.0f m");
            if(绘制.按钮.雷达){
                Slider("雷达X","##雷达X",&绘制.按钮.雷达X,0.f,(float)displayInfo.width,"%.0f");
                Slider("雷达Y","##雷达Y",&绘制.按钮.雷达Y,0.f,(float)displayInfo.height,"%.0f");
                Slider("雷达缩放","##雷达缩放",&绘制.按钮.雷达缩放,.5f,2.f,"%.2f");
            }
        }
    }else if(currentTab==2){
        if(spatial::DrawingSection("载具信息")){
            Toggle("载具绘制",&绘制.按钮.车辆);
            Toggle("载具血量",&绘制.按钮.载具血量);
            Toggle("载具油量",&绘制.按钮.载具油量);
        }
        if(spatial::DrawingSection("地面物资")){
            Toggle("步枪",&绘制.按钮.显示步枪);
            Toggle("冲锋枪",&绘制.按钮.冲锋枪械);
            Toggle("狙击枪",&绘制.按钮.狙击枪械);
            Toggle("霰弹枪",&绘制.按钮.散弹枪械);
            Toggle("子弹",&绘制.按钮.显示子弹);
            Toggle("倍镜",&绘制.按钮.显示倍镜);
            Toggle("药品",&绘制.按钮.显示药品);
            Toggle("防具",&绘制.按钮.显示防具);
            Toggle("投掷物",&绘制.按钮.投掷物品);
            Toggle("信号枪",&绘制.按钮.绘制信号枪);
            Toggle("其他物资",&绘制.按钮.其他物资);
            Toggle("配件",&绘制.按钮.显示配件);
        }
        if(spatial::DrawingSection("箱体标记")){
            Toggle("绘制宝箱",&绘制.按钮.绘制宝箱);
            Toggle("绘制药箱",&绘制.按钮.绘制药箱);
            Toggle("武器箱",&绘制.按钮.绘制武器箱);
            Toggle("超级物资箱",&绘制.按钮.超级物资箱);
            Toggle("死亡盒子",&绘制.按钮.盒子);
            Toggle("隐藏已开启",&绘制.按钮.隐藏已开启);
            Note("items-note","只显示需要的信息。关闭不常用类别，可减少标记重叠。");
        }
    }else if(currentTab==3){
        if(spatial::DrawingSection("运行参数")){
            Slider("绘制帧率","##绘制帧率",&绘制.按钮.当前帧率,30.f,165.f,"%.0f FPS");
            Toggle("场景线框",&绘制.按钮.PhysX画线);
            Note("scene-note","场景线框按需加载，用于观察场景轮廓，默认关闭。");
        }
        if(spatial::DrawingSection("配置管理")){
            spatial::BeginDrawingRow("save-drawing-config");
            if(ImGui::Button("保存当前配置",{-12*s,42*s})){
                Save();spatial::SavePreferences();spatial::Notice("绘制配置已保存");
            }
            spatial::EndDrawingRow();
            spatial::BeginDrawingRow("load-drawing-config");
            if(ImGui::Button("读取绘制配置",{-12*s,42*s})){
                绘制.读取配置("lsrael_draw_139");spatial::LoadPreferences();spatial::Notice("绘制配置已读取");
            }
            spatial::EndDrawingRow();
        }
        if(spatial::DrawingSection("设备信息")){
            LoadDeviceInfo();
            BeginRow("设备型号");ImGui::TextWrapped("%s %s",g_brand.c_str(),g_model.c_str());EndRow();
            BeginRow("系统内核");ImGui::TextWrapped("%s",g_kernel.c_str());EndRow();
            BeginRow("画布尺寸");ImGui::Text("%d × %d",displayInfo.width,displayInfo.height);EndRow();
            Note("drawing-version","LSRAEL DRAW / 1.39 · 玻璃界面 · 独立绘制配置");
        }
    }
}

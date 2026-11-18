#pragma once
// Included by SpatialUI.h inside namespace spatial, after the shared helpers.
// These widgets own presentation and temporary editing state only.

struct NumericEditorState {
    bool active=false, requestOpen=false, replaceText=false, valid=true, changed=false;
    float* target=nullptr;
    float* changedTarget=nullptr;
    float pending=0, minimum=0, maximum=1, step=1;
    int decimals=0;
    char label[160]{}, format[24]{}, text[64]{};
    ImRect bounds, displayBounds, keyBounds[12], clearBounds, backspaceBounds, cancelBounds, confirmBounds;
};
inline NumericEditorState& NumericEditor(){static NumericEditorState value;return value;}
struct NumberControlGeometry {ImRect minus,slider,number,plus;};
struct ChoiceControlGeometry {ImRect segments[8];int count=0;};
inline NumberControlGeometry& LastNumberLayout(){static NumberControlGeometry value;return value;}
inline ChoiceControlGeometry& LastChoiceLayout(){static ChoiceControlGeometry value;return value;}

inline int NumberDecimals(const char* format){
    if(!format)return 0;
    const char* dot=std::strchr(format,'.');if(!dot)return 0;
    int count=0;
    for(++dot;*dot>='0'&&*dot<='9';++dot)count=std::min(6,count*10+*dot-'0');
    return count;
}
inline float NumberStep(const char* format){
    float step=1;
    for(int n=0;n<NumberDecimals(format);++n)step*=.1f;
    return step;
}
inline bool ParseNumber(const char* text,float& value){
    if(!text||!*text)return false;
    char extra=0;float parsed=0;
    if(std::sscanf(text,"%f%c",&parsed,&extra)!=1||!std::isfinite(parsed))return false;
    value=parsed;return true;
}
inline float ConstrainNumber(float value,float minimum,float maximum,float step){
    if(!std::isfinite(value))value=minimum;
    const double rounded=std::round((double)value/(double)step)*(double)step;
    return Clamp((float)rounded,minimum,maximum);
}
inline void OpenNumericEditor(const char* label,float* target,float minimum,float maximum,const char* format){
    auto& edit=NumericEditor();
    edit.active=true;edit.requestOpen=true;edit.replaceText=true;edit.target=target;
    edit.minimum=minimum;edit.maximum=maximum;edit.decimals=NumberDecimals(format);edit.step=NumberStep(format);
    edit.pending=*target;
    const char* visible=label?label:"数值";
    if(visible[0]=='#'&&visible[1]=='#')visible+=2;
    std::snprintf(edit.label,sizeof(edit.label),"%s",visible);
    std::snprintf(edit.format,sizeof(edit.format),"%s",format?format:"%.0f");
    std::snprintf(edit.text,sizeof(edit.text),edit.format,*target);
    edit.valid=ParseNumber(edit.text,edit.pending);
}
inline void NumberEditorAppend(char character){
    auto& edit=NumericEditor();
    if(character=='.'&&edit.decimals==0)return;
    if(edit.replaceText){edit.text[0]=0;edit.replaceText=false;}
    size_t n=std::strlen(edit.text);
    if(n+2>=sizeof(edit.text))return;
    if(character=='.'){
        if(std::strchr(edit.text,'.'))return;
        if(n==0||(n==1&&edit.text[0]=='-')){edit.text[n++]='0';edit.text[n]=0;}
    }else if(character<'0'||character>'9')return;
    if(n==1&&edit.text[0]=='0'&&character!='.')n=0;
    edit.text[n++]=character;edit.text[n]=0;
    edit.valid=ParseNumber(edit.text,edit.pending);
}
inline void NumberEditorBackspace(){
    auto& edit=NumericEditor();edit.replaceText=false;
    size_t n=std::strlen(edit.text);if(n)edit.text[n-1]=0;
    edit.valid=ParseNumber(edit.text,edit.pending);
}
inline void NumberEditorSign(){
    auto& edit=NumericEditor();edit.replaceText=false;
    size_t n=std::strlen(edit.text);
    if(n&&edit.text[0]=='-')std::memmove(edit.text,edit.text+1,n);
    else if(n+2<sizeof(edit.text)){std::memmove(edit.text+1,edit.text,n+1);edit.text[0]='-';}
    edit.valid=ParseNumber(edit.text,edit.pending);
}
inline void CloseNumericEditor(bool accept){
    auto& edit=NumericEditor();
    if(accept&&edit.target&&edit.valid){
        const float next=ConstrainNumber(edit.pending,edit.minimum,edit.maximum,edit.step);
        if(*edit.target!=next){*edit.target=next;edit.changedTarget=edit.target;edit.changed=true;}
        edit.pending=next;
    }
    edit.active=false;edit.requestOpen=false;edit.target=nullptr;
    ImGui::CloseCurrentPopup();
}

inline bool NumberControlButton(const char* id,const char* text,ImVec2 size,bool selected=false,int symbol=0,float controlScale=0){
    const ImVec2 p=ImGui::GetCursorScreenPos();const float s=controlScale>0?controlScale:Get().scale;
    bool pressed=ImGui::InvisibleButton(id,size);
    auto* draw=ImGui::GetWindowDrawList();
    spatial_chrome::Panel(draw,p,size,9*s,Material(),s,ImGui::GetStyle().Alpha,ImGui::IsItemHovered()?.31f:.16f);
    if(selected)draw->AddRect(p,Add(p,size),ImGui::GetColorU32(ImGuiCol_CheckMark),9*s,0,1.2f*s);
    ImU32 ink=ImGui::GetColorU32(selected?ImGuiCol_CheckMark:ImGuiCol_Text);
    if(symbol){
        ImVec2 center(p.x+size.x*.5f,p.y+size.y*.5f);float arm=5*s;
        draw->AddLine({center.x-arm,center.y},{center.x+arm,center.y},ink,1.6f*s);
        if(symbol>0)draw->AddLine({center.x,center.y-arm},{center.x,center.y+arm},ink,1.6f*s);
    }else{
        const float naturalWidth=ImGui::GetFont()->CalcTextSizeA(17*s,10000,0,text).x;
        const float font=17*s*std::min(1.f,std::max(.60f,(size.x-6*s)/std::max(1.f,naturalWidth)));
        ImVec2 textSize=ImGui::GetFont()->CalcTextSizeA(font,10000,0,text);
        draw->PushClipRect(Add(p,{3*s,0}),Add(p,{size.x-3*s,size.y}),true);
        Text(draw,{p.x+(size.x-textSize.x)*.5f,p.y+(size.y-textSize.y)*.5f},font,ink,text);
        draw->PopClipRect();
    }
    return pressed;
}

inline bool NumberSlider(const char* label,float* value,float minimum,float maximum,const char* format){
    auto& edit=NumericEditor();bool changed=false;
    // A keypad commit is consumed by its original widget, keeping the caller's
    // existing save/notification callback in the same place as a slider edit.
    if(edit.changed&&edit.changedTarget==value){changed=true;edit.changed=false;edit.changedTarget=nullptr;}
    if(ImGui::GetCurrentWindow()->SkipItems)return changed;
    const float s=Get().scale,step=NumberStep(format),width=std::max(1.f,ImGui::CalcItemWidth());
    const float height=40*s,gap=std::min(6*s,width*.025f),button=std::min(36*s,width*.16f),number=std::min(84*s,width*.33f);
    const float track=std::max(20*s,width-2*button-number-3*gap);
    auto& layout=LastNumberLayout();layout=NumberControlGeometry{};
    const ImGuiID fullId=ImGui::GetID(label);
    const ImVec2 groupStart=ImGui::GetCursorScreenPos();
    ImGui::PushID(label);ImGui::BeginGroup();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{gap,0});
    auto assign=[&](float next){
        next=ConstrainNumber(next,minimum,maximum,step);
        if(next!=*value){*value=next;changed=true;}
    };
    if(NumberControlButton("##minus","",{button,height},false,-1))assign(*value-step);
    layout.minus=ImRect(ImGui::GetItemRectMin(),ImGui::GetItemRectMax());
    ImGui::SameLine();const ImVec2 p=ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##track",{track,height});
    layout.slider=ImRect(ImGui::GetItemRectMin(),ImGui::GetItemRectMax());
    if(ImGui::IsItemActive()&&ImGui::GetIO().MouseDown[0]){
        const float ratio=Clamp((ImGui::GetIO().MousePos.x-p.x-7*s)/std::max(1.f,track-14*s),0,1);
        assign(minimum+(maximum-minimum)*ratio);
    }
    // Clamp the painted position only. Initial application values are left
    // untouched until the user drags, steps or confirms a keypad edit.
    const float ratio=Clamp((*value-minimum)/std::max(.000001f,maximum-minimum),0,1);
    auto* draw=ImGui::GetWindowDrawList();float left=p.x+7*s,right=p.x+track-7*s,y=p.y+height*.5f,x=left+(right-left)*ratio;
    draw->AddLine({left,y},{right,y},ImGui::GetColorU32(ImGuiCol_FrameBgActive),4*s);
    draw->AddLine({left,y},{x,y},ImGui::GetColorU32(ImGuiCol_SliderGrab),4*s);
    if(ImGui::IsItemActive()||ImGui::IsItemHovered())draw->AddCircleFilled({x,y},10*s,ImGui::GetColorU32(ImGuiCol_FrameBgHovered),24);
    draw->AddCircleFilled({x,y},6*s,ImGui::GetColorU32(ImGuiCol_SliderGrab),24);
    draw->AddCircleFilled({x,y},2*s,ImGui::GetColorU32(ImGuiCol_FrameBg),18);
    ImGui::SameLine();char text[64];std::snprintf(text,sizeof(text),format?format:"%.0f",*value);
    if(NumberControlButton("##value",text,{number,height}))OpenNumericEditor(label,value,minimum,maximum,format);
    layout.number=ImRect(ImGui::GetItemRectMin(),ImGui::GetItemRectMax());
    ImGui::SameLine();if(NumberControlButton("##plus","",{button,height},false,1))assign(*value+step);
    layout.plus=ImRect(ImGui::GetItemRectMin(),ImGui::GetItemRectMax());
    ImGui::PopStyleVar();ImGui::EndGroup();
    ImGuiItemStatusFlags status=changed?ImGuiItemStatusFlags_Edited:ImGuiItemStatusFlags_None;
    ImGui::SetLastItemData(fullId,ImGuiItemFlags_None,status,ImRect(groupStart,ImGui::GetItemRectMax()));
    ImGui::PopID();return changed;
}

inline bool Choice(const char* label,int* value,const char* const items[],int count,int height=-1){
    if(count<=0||ImGui::GetCurrentWindow()->SkipItems)return false;
    const float s=Get().scale,gap=4*s,width=std::max(1.f,ImGui::CalcItemWidth());
    const float itemWidth=std::max(1.f,(width-gap*(count-1))/count),itemHeight=height<0?40*s:(float)height;
    bool changed=false;
    auto& layout=LastChoiceLayout();layout=ChoiceControlGeometry{};layout.count=std::min(8,count);
    ImGui::PushID(label);ImGui::BeginGroup();ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{gap,0});
    for(int n=0;n<count;++n){
        if(n)ImGui::SameLine();ImGui::PushID(n);
        if(NumberControlButton("##choice",items[n],{itemWidth,itemHeight},*value==n)&&*value!=n){*value=n;changed=true;}
        if(n<8)layout.segments[n]=ImRect(ImGui::GetItemRectMin(),ImGui::GetItemRectMax());
        ImGui::PopID();
    }
    ImGui::PopStyleVar();ImGui::EndGroup();
    ImGui::SetLastItemData(ImGui::GetID(label),ImGuiItemFlags_None,
        changed?ImGuiItemStatusFlags_Edited:ImGuiItemStatusFlags_None,
        ImRect(ImGui::GetItemRectMin(),ImGui::GetItemRectMax()));
    ImGui::PopID();return changed;
}

// Call once in a stable central scope, after the main window's End() and before
// restoring its style. Opening the popup here avoids page-row PushID scopes.
inline void RenderNumericEditor(){
    auto& edit=NumericEditor();if(!edit.active)return;
    if(edit.requestOpen){ImGui::OpenPopup("##spatial-number-editor");edit.requestOpen=false;}
    ImGuiIO& io=ImGui::GetIO();
    const bool wide=io.DisplaySize.x>io.DisplaySize.y*1.4f&&io.DisplaySize.y<800.f;
    const float logicalW=wide?600.f:400.f,logicalH=wide?460.f:580.f;
    const float fit=std::min((io.DisplaySize.x-24)/logicalW,(io.DisplaySize.y-24)/logicalH);
    const float s=std::max(.1f,std::min(wide?fit:std::max(.55f,Get().scale),fit));
    ImGui::SetNextWindowPos({io.DisplaySize.x*.5f,io.DisplaySize.y*.5f},ImGuiCond_Always,{.5f,.5f});
    ImGui::SetNextWindowSize({logicalW*s,0},ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{18*s,18*s});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,22*s);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,10*s);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{10*s,8*s});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{7*s,7*s});
    // Keep the host surface transparent while the modal captures interaction.
    ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg,{0,0,0,0});
    ImGui::PushStyleColor(ImGuiCol_PopupBg,{0,0,0,0});
    const ImGuiWindowFlags flags=ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|
        ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse|ImGuiWindowFlags_AlwaysAutoResize;
    if(ImGui::BeginPopupModal("##spatial-number-editor",nullptr,flags)){
        // ImGui draws modal dimming later in Render(), after our shared style
        // has been restored. Suppress that deferred fullscreen wash as well.
        GImGui->DimBgRatio=0.f;
        auto setFont=[&](float pixels){const auto* w=ImGui::GetCurrentWindow();
            const float parent=w->ParentWindow?w->ParentWindow->FontWindowScale:1.f;
            ImGui::SetWindowFontScale(pixels/std::max(.01f,GImGui->FontBaseSize*parent));};
        setFont(20*s);
        edit.bounds=ImRect(ImGui::GetWindowPos(),Add(ImGui::GetWindowPos(),ImGui::GetWindowSize()));
        spatial_chrome::Panel(ImGui::GetWindowDrawList(),edit.bounds.Min,edit.bounds.GetSize(),22*s,Material(),s,Get().alpha,.16f);
        ImGui::TextUnformatted(edit.label);
        setFont(13*s);
        ImGui::TextDisabled("输入数值后点击确定");
        const ImVec2 display=ImGui::GetCursorScreenPos();float available=ImGui::GetContentRegionAvail().x;
        edit.displayBounds=ImRect(display,Add(display,{available,56*s}));
        auto* draw=ImGui::GetWindowDrawList();
        spatial_chrome::Panel(draw,edit.displayBounds.Min,edit.displayBounds.GetSize(),12*s,Material(),s,Get().alpha,.16f);
        const char* shown=*edit.text?edit.text:"0";const float font=29*s;
        ImVec2 size=ImGui::GetFont()->CalcTextSizeA(font,10000,0,shown);
        draw->PushClipRect(Add(display,{12*s,0}),Add(display,{available-12*s,56*s}),true);
        Text(draw,{display.x+available-size.x-14*s,display.y+(56*s-size.y)*.5f},font,Ink(),shown);
        draw->PopClipRect();ImGui::Dummy({available,56*s});
        char low[48],high[48],step[48];
        std::snprintf(low,sizeof(low),edit.format,edit.minimum);std::snprintf(high,sizeof(high),edit.format,edit.maximum);
        std::snprintf(step,sizeof(step),edit.format,edit.step);
        ImGui::TextDisabled("范围 %s - %s  ·  步长 %s",low,high,step);
        setFont(20*s);
        // Desktop keyboards and the on-screen keypad share the same buffer.
        for(int n=0;n<io.InputQueueCharacters.Size;++n){
            unsigned int c=io.InputQueueCharacters[n];
            if((c>='0'&&c<='9')||c=='.')NumberEditorAppend((char)c);
            else if(c=='-')NumberEditorSign();
        }
        if(ImGui::IsKeyPressed(ImGuiKey_Backspace))NumberEditorBackspace();
        const char* keys[]={"7","8","9","4","5","6","1","2","3",".","0","+/-"};
        const int columns=wide?6:3;
        const float keyWidth=(available-7*s*(columns-1))/columns;
        for(int n=0;n<12;++n){
            if(n%columns)ImGui::SameLine();
            bool disabled=n==9&&edit.decimals==0;if(disabled)ImGui::BeginDisabled();
            ImGui::PushID(n);bool pressed=NumberControlButton("##key",keys[n],{keyWidth,44*s},false,0,s);ImGui::PopID();
            edit.keyBounds[n]=ImRect(ImGui::GetItemRectMin(),ImGui::GetItemRectMax());
            if(disabled)ImGui::EndDisabled();
            if(pressed){if(n==11)NumberEditorSign();else NumberEditorAppend(keys[n][0]);}
        }
        const float half=(available-7*s)*.5f;
        if(NumberControlButton("##clear","清空",{half,36*s},false,0,s)){edit.text[0]=0;edit.replaceText=false;edit.valid=false;}
        edit.clearBounds=ImRect(ImGui::GetItemRectMin(),ImGui::GetItemRectMax());ImGui::SameLine();
        if(NumberControlButton("##backspace","退格",{half,36*s},false,0,s))NumberEditorBackspace();
        edit.backspaceBounds=ImRect(ImGui::GetItemRectMin(),ImGui::GetItemRectMax());
        const bool escape=ImGui::IsKeyPressed(ImGuiKey_Escape,false);
        const bool enter=ImGui::IsKeyPressed(ImGuiKey_Enter,false)||ImGui::IsKeyPressed(ImGuiKey_KeypadEnter,false);
        bool cancel=NumberControlButton("##cancel","取消",{half,42*s},false,0,s)||escape;
        edit.cancelBounds=ImRect(ImGui::GetItemRectMin(),ImGui::GetItemRectMax());ImGui::SameLine();
        ImGui::BeginDisabled(!edit.valid);bool accept=NumberControlButton("##confirm","确定",{half,42*s},true,0,s)||(edit.valid&&enter);ImGui::EndDisabled();
        edit.confirmBounds=ImRect(ImGui::GetItemRectMin(),ImGui::GetItemRectMax());
        if(cancel)CloseNumericEditor(false);else if(accept)CloseNumericEditor(true);
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor(2);ImGui::PopStyleVar(5);
}

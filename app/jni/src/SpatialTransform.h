#pragma once
#include "imgui.h"
#include "imgui_internal.h"
#include <cmath>
#include <vector>
#include <memory>
#include <algorithm>

namespace spatial_transform {
struct Projection {
    ImVec2 center{0,0};
    float a=1,b=0,c=0,d=1,e=0,f=0,focal=1800,zoom=1,offsetY=0,offsetX=0;
    float nx=0,ny=0,nz=1,tx=0,ty=0,tz=0,depth=0;
    bool enabled=false;
    static Projection Make(ImVec2 center,float pitch,float yaw,float roll,float zoom=1,float offset=0) {
        Projection p;p.center=center;p.zoom=zoom;p.offsetY=offset;p.enabled=true;
        float x=-pitch*IM_PI/180,y=yaw*IM_PI/180,z=roll*IM_PI/180;
        float cx=std::cos(x),sx=std::sin(x),cy=std::cos(y),sy=std::sin(y),cz=std::cos(z),sz=std::sin(z);
        p.a=cz*cy;p.b=cz*sy*sx-sz*cx;p.c=sz*cy;p.d=sz*sy*sx+cz*cx;
        p.e=-sy;p.f=cy*sx;p.nx=cz*sy*cx+sz*sx;p.ny=sz*sy*cx-cz*sx;p.nz=cy*cx;return p;
    }
    Projection AtLayer(float z,float x=0,float y=0) const {Projection p=*this;p.depth=z;p.tx=a*x+b*y-nx*z;p.ty=c*x+d*y-ny*z;p.tz=e*x+f*y-nz*z;return p;}
    Projection AtDepth(float z) const {return AtLayer(z);}
    ImVec2 Project(ImVec2 q) const {
        if(!enabled||q.x<=-1e20f||q.y<=-1e20f)return q;
        float x=q.x-center.x,y=q.y-center.y;
        float k=zoom*focal/(focal+e*x+f*y+tz);
        return {center.x+offsetX+(a*x+b*y+tx)*k,center.y+offsetY+(c*x+d*y+ty)*k};
    }
    ImVec2 Inverse(ImVec2 q) const {
        if(!enabled||q.x<=-1e20f||q.y<=-1e20f)return q;
        float u=(q.x-center.x-offsetX)/zoom,v=(q.y-center.y-offsetY)/zoom;
        float aa=a-u*e/focal,bb=b-u*f/focal,cc=c-v*e/focal,dd=d-v*f/focal;
        float det=aa*dd-bb*cc;if(std::fabs(det)<.0001f)return {-FLT_MAX,-FLT_MAX};
        u=u*(1+tz/focal)-tx;v=v*(1+tz/focal)-ty;
        return {center.x+(u*dd-bb*v)/det,center.y+(aa*v-u*cc)/det};
    }
    ImRect Bounds(ImVec2 min,ImVec2 max) const {
        ImRect r(Project(min),Project(min));r.Add(Project({max.x,min.y}));r.Add(Project(max));r.Add(Project({min.x,max.y}));return r;
    }
};
struct Layer {ImDrawList* list;int first,last;float depth;ImRect rect;bool hit;float x=0,y=0;};
struct HitLayer {Projection projection;ImRect rect;};
struct Context {
    Projection previous,frame;
    Projection input;
    bool inputLocked=false;
    bool screenOverlay=false;
    std::vector<Layer> layers,defaults;
    std::vector<HitLayer> previousHits;
    ImVec2 raw{-FLT_MAX,-FLT_MAX},lastRaw{-FLT_MAX,-FLT_MAX};
    bool prepared=false;
    std::vector<ImDrawList*> selected;
    std::vector<std::unique_ptr<ImDrawList>> pool;
    std::vector<ImDrawList*> output;
    size_t used=0;
};
inline Context& Get(){static Context c;return c;}
inline void LayerRange(ImDrawList* list,int first,float depth,ImRect rect,bool hit=true,float x=0,float y=0){
    Get().layers.push_back({list,first,list->VtxBuffer.Size,depth,rect,hit,x,y});
}
inline void ListDepth(ImDrawList* list,float depth,ImRect rect,float x=0,float y=0){Get().defaults.push_back({list,0,INT_MAX,depth,rect,true,x,y});}
inline Projection InputAt(ImVec2 raw){
    auto& c=Get();if(c.screenOverlay)return Projection{};Projection result=c.previous;float best=-FLT_MAX;
    for(auto& layer:c.previousHits){ImVec2 q=layer.projection.Inverse(raw);
        if(layer.rect.Contains(q)&&layer.projection.depth>=best){result=layer.projection;best=result.depth;}}
    return result;
}
inline ImVec2 ProjectLocal(ImVec2 local){
    auto& c=Get();Projection result=c.previous;float best=-FLT_MAX;
    for(auto& layer:c.previousHits)if(layer.rect.Contains(local)&&layer.projection.depth>=best){result=layer.projection;best=result.depth;}
    return result.Project(local);
}
inline void PrepareInput() {
    auto& c=Get();auto& io=ImGui::GetIO();auto& g=*GImGui;
    c.selected.clear();c.layers.clear();c.defaults.clear();c.raw=io.MousePos;
    for(auto& event:g.InputEventsQueue)if(event.Type==ImGuiInputEventType_MousePos)c.raw={event.MousePos.PosX,event.MousePos.PosY};
    bool down=io.MouseDown[0];for(auto& event:g.InputEventsQueue)if(event.Type==ImGuiInputEventType_MouseButton&&event.MouseButton.Button==0)down=event.MouseButton.Down;
    if(!c.inputLocked)c.input=InputAt(c.raw);
    c.inputLocked=down;
    for(auto& event:g.InputEventsQueue)if(event.Type==ImGuiInputEventType_MousePos){
        ImVec2 raw(event.MousePos.PosX,event.MousePos.PosY);c.raw=raw;
        ImVec2 q=c.input.Inverse(raw);event.MousePos.PosX=q.x;event.MousePos.PosY=q.y;
    }
    io.MousePos=c.input.Inverse(io.MousePos);
    // Undo the previous frame's mapping before recomputing deltas in the new pose.
    io.MousePosPrev=c.input.Inverse(c.lastRaw);
    c.prepared=true;
}
inline ImVec2 RawPointer(){return Get().prepared?Get().input.Project(ImGui::GetIO().MousePos):ImGui::GetIO().MousePos;}
inline void RestoreInput() {
    auto& c=Get();auto& io=ImGui::GetIO();
    if(!c.prepared)return;
    ImVec2 raw=c.input.Project(io.MousePos);
    for(auto& event:GImGui->InputEventsQueue)if(event.Type==ImGuiInputEventType_MousePos){
        ImVec2 q=c.input.Project({event.MousePos.PosX,event.MousePos.PosY});event.MousePos.PosX=q.x;event.MousePos.PosY=q.y;
    }
    io.MousePos=raw;c.lastRaw=raw;c.prepared=false;
}
inline ImDrawVert Mix(const ImDrawVert& a,const ImDrawVert& b,float t) {
    ImDrawVert v;v.pos={a.pos.x+(b.pos.x-a.pos.x)*t,a.pos.y+(b.pos.y-a.pos.y)*t};
    v.uv={a.uv.x+(b.uv.x-a.uv.x)*t,a.uv.y+(b.uv.y-a.uv.y)*t};v.col=0;
    for(int sh=0;sh<32;sh+=8){int x=(a.col>>sh)&255,y=(b.col>>sh)&255;v.col|=(ImU32)(x+(y-x)*t+.5f)<<sh;}return v;
}
// Clip in the original UI plane BEFORE projection. This keeps scrolled content inside
// its rotated card, including combinations of pitch, yaw and roll.
inline int ClipTriangle(const ImDrawVert* triangle,ImVec4 rect,ImDrawVert* result) {
    ImDrawVert a[12],b[12];int count=3;for(int i=0;i<3;++i)a[i]=triangle[i];
    for(int edge=0;edge<4;++edge){int out=0;
        auto distance=[&](const ImDrawVert& v){return edge==0?v.pos.x-rect.x:edge==1?rect.z-v.pos.x:edge==2?v.pos.y-rect.y:rect.w-v.pos.y;};
        for(int i=0;i<count;++i){const auto& s=a[i];const auto& t=a[(i+1)%count];float ds=distance(s),dt=distance(t);
            if(ds>=0)b[out++]=s;
            if((ds>=0)!=(dt>=0))b[out++]=Mix(s,t,ds/(ds-dt));}
        count=out;if(!count)return 0;for(int i=0;i<count;++i)a[i]=b[i];
    }
    for(int i=0;i<count;++i)result[i]=a[i];return count;
}
inline ImDrawList* NextList(ImTextureID texture,ImVec2 display) {
    auto& c=Get();if(c.used==c.pool.size())c.pool.emplace_back(new ImDrawList(ImGui::GetDrawListSharedData()));
    ImDrawList* d=c.pool[c.used++].get();d->_ResetForNewFrame();d->Flags&=~ImDrawListFlags_AllowVtxOffset;
    d->PushClipRect({0,0},display);d->PushTextureID(texture);c.output.push_back(d);return d;
}
inline void ApplyDrawData(ImDrawData* data) {
    auto& c=Get();RestoreInput();if(!data)return;
    c.used=0;c.output.clear();
    for(ImDrawList* list:data->CmdLists){
        bool selected=std::find(c.selected.begin(),c.selected.end(),list)!=c.selected.end();
        if(!selected||!c.frame.enabled){c.output.push_back(list);continue;}
        ImDrawList* out=nullptr;
        for(const ImDrawCmd& cmd:list->CmdBuffer){
            if(cmd.UserCallback){if(!out)out=NextList(cmd.TextureId,data->DisplaySize);out->AddCallback(cmd.UserCallback,cmd.UserCallbackData);continue;}
            for(unsigned i=0;i+2<cmd.ElemCount;i+=3){
                ImDrawVert tri[3],poly[12];for(int n=0;n<3;++n)tri[n]=list->VtxBuffer[list->IdxBuffer[cmd.IdxOffset+i+n]+cmd.VtxOffset];
                unsigned vertex=list->IdxBuffer[cmd.IdxOffset+i]+cmd.VtxOffset;Projection projection=c.frame;
                for(auto& layer:c.defaults)if(layer.list==list)projection=c.frame.AtLayer(layer.depth,layer.x,layer.y);
                for(auto& layer:c.layers)if(layer.list==list&&vertex>=(unsigned)layer.first&&vertex<(unsigned)layer.last)projection=c.frame.AtLayer(layer.depth,layer.x,layer.y);
                int count=ClipTriangle(tri,cmd.ClipRect,poly);if(count<3)continue;
                if(!out||out->VtxBuffer.Size+(count-2)*3>60000)out=NextList(cmd.TextureId,data->DisplaySize);
                else if(out->_CmdHeader.TextureId!=cmd.TextureId)out->PushTextureID(cmd.TextureId);
                for(int j=1;j+1<count;++j){ImDrawVert verts[3]={poly[0],poly[j],poly[j+1]};
                    out->PrimReserve(3,3);unsigned index=out->_VtxCurrentIdx;
                    for(int k=0;k<3;++k){out->PrimWriteIdx((ImDrawIdx)(index+k));out->PrimWriteVtx(projection.Project(verts[k].pos),verts[k].uv,verts[k].col);}}
            }
        }
    }
    data->CmdLists.resize((int)c.output.size());data->TotalIdxCount=0;data->TotalVtxCount=0;
    for(int i=0;i<(int)c.output.size();++i){data->CmdLists[i]=c.output[i];data->TotalIdxCount+=c.output[i]->IdxBuffer.Size;data->TotalVtxCount+=c.output[i]->VtxBuffer.Size;}
    data->CmdListsCount=data->CmdLists.Size;c.previous=c.frame;c.previousHits.clear();
    if(c.frame.enabled){
        for(auto& layer:c.defaults)c.previousHits.push_back({c.frame.AtLayer(layer.depth,layer.x,layer.y),layer.rect});
        for(auto& layer:c.layers)if(layer.hit)c.previousHits.push_back({c.frame.AtLayer(layer.depth,layer.x,layer.y),layer.rect});
    }
}
} // namespace spatial_transform

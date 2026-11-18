#include "图片调用.h"
#include "物资ID.h"
#include "辅助类.h"
#include "DrawOnlyVisuals.h"

// Original rendering base: 入梦 / lsraelnb. Drawing-only visual redesign.
extern 绘制 绘制;

std::map<int, TextureInfo> 手持图片;

static int getLength(const char* str) {
    int len = 0;
    while (*str) {
        if ((*str & 0xC0) != 0x80) {
            len++;
        }
        ++str;
    }
    return len;
}

static std::array<D2DVector*,15> ScreenJointRefs(骨骼数据& 骨骼) {
    return {{&骨骼.Head,&骨骼.Chest,&骨骼.Pelvis,&骨骼.Left_Shoulder,&骨骼.Right_Shoulder,
        &骨骼.Left_Elbow,&骨骼.Right_Elbow,&骨骼.Left_Wrist,&骨骼.Right_Wrist,&骨骼.Left_Thigh,
        &骨骼.Right_Thigh,&骨骼.Left_Knee,&骨骼.Right_Knee,&骨骼.Left_Ankle,&骨骼.Right_Ankle}};
}
static std::array<ImVec2,15> ScreenJoints(骨骼数据& 骨骼) {
    const auto source=ScreenJointRefs(骨骼);std::array<ImVec2,15> points{};
    for(std::size_t i=0;i<points.size();++i)points[i]={source[i]->X,source[i]->Y};
    return points;
}

void 绘图::初始化绘图(int X, int Y) {
    if (Y < X) {
        this->PX = X / 2;
        this->PY = Y / 2;
    } else {
        this->PX = Y / 2;
        this->PY = X / 2;
    }
}

void 绘图::绘制头部骨骼点(D2DVector& 头部坐标,bool isboot,int 队伍,float 距离,bool isVisible) {
    if(!绘制.按钮.显示头部骨骼点||距离<0||距离>绘制.骨骼距离限制)return;
    draw_only_visuals::HeadMarker(ImGui::GetForegroundDrawList(),{头部坐标.X,头部坐标.Y},isboot,isVisible);
}

void 绘图::绘制距离(int 距离,int 队伍) {
    draw_only_visuals::Distance(ImGui::GetForegroundDrawList(),{left,top,right,bottom},距离);
}
/*
void 绘图::RenderRadarScan(ImDrawList* draw_list, ImVec2 center, float radius, int numSegments, float& rotationAngle, float lineLength) {
    ImVec2 outerCirclePos = ImVec2(center.x + radius * cos(rotationAngle), center.y + radius * sin(rotationAngle));
    draw_list->AddCircleFilled(center, radius, IM_COL32(255, 255, 255, 55), numSegments);
    draw_list->AddCircle(center, radius * 0.75f, IM_COL32_WHITE, numSegments);
    draw_list->AddCircle(center, radius * 0.5f, IM_COL32_WHITE, numSegments);
    draw_list->AddCircle(center, radius, IM_COL32_WHITE, numSegments);
    draw_list->AddLine(ImVec2(center.x - lineLength, center.y), ImVec2(center.x + lineLength, center.y), IM_COL32_WHITE);
    draw_list->AddLine(ImVec2(center.x, center.y - lineLength), ImVec2(center.x, center.y + lineLength), IM_COL32_WHITE);
}*/

void 绘图::初始化坐标(D4DVector& 屏幕坐标, 骨骼数据& 骨骼) {
    const draw_only_visuals::Box root{屏幕坐标.X,屏幕坐标.Y-屏幕坐标.W,屏幕坐标.X+屏幕坐标.Z,屏幕坐标.Y};
    const auto view=ImGui::GetIO().DisplaySize;
    const auto points=draw_only_visuals::FilterJoints(ScreenJoints(骨骼),root,view);
    const auto destination=ScreenJointRefs(骨骼);
    for(std::size_t i=0;i<points.size();++i){destination[i]->X=points[i].x;destination[i]->Y=points[i].y;}
    const auto box=draw_only_visuals::BoxFromJoints(points,root,view);
    left=box.left;right=box.right;top=box.top;bottom=box.bottom;
    MIDDLE=box.Center();BOTTOM=box.bottom;TOP=box.top;
}

void 绘图::绘制方框(bool isboot,bool isVisible) {
    draw_only_visuals::Corners(ImGui::GetForegroundDrawList(),{left,top,right,bottom},
                              isVisible,isboot,绘制.按钮.方框粗细);
}

void 绘图::绘制人数(int 人机,int 真人,uintptr_t 自身) {
    draw_only_visuals::Summary(ImGui::GetForegroundDrawList(),真人,人机,自身!=0);
}
// 注意：这里只有一个结束花括号

void 绘图::RenderRadarScan(ImDrawList* draw_list, ImVec2 center, float radius, int numSegments, float& rotationAngle, float lineLength, float yawRotation) {
    using namespace draw_only_visuals;
    const ImVec2 view=ImGui::GetIO().DisplaySize;
    if(!draw_list||!ViewportValid(view)||!PointValid(center)||!Finite(radius)||!Finite(yawRotation)||radius<12||radius>view.y*.45f)return;
    const auto palette=PaletteColors();const float s=Scale(view);
    const int segments=std::max(24,std::min(96,numSegments));
    draw_list->PushClipRect({0,0},view,true);
    draw_list->AddCircleFilled(center,radius,WithAlpha(palette.glass,.75f),segments);
    draw_list->AddCircle(center,radius,palette.border,segments,1*s);
    draw_list->AddCircle(center,radius*.5f,WithAlpha(palette.border,.65f),segments,.8f*s);
    Line(draw_list,{center.x-radius+5*s,center.y},{center.x+radius-5*s,center.y},WithAlpha(palette.border,.4f),.6f,false);
    Line(draw_list,{center.x,center.y-radius+5*s},{center.x,center.y+radius-5*s},WithAlpha(palette.border,.4f),.6f,false);
    draw_list->AddCircleFilled(center,2.5f*s,palette.visible,16);
    rotationAngle=yawRotation*.01745329252f;
    // A single heading mark replaces the animated sweep and eight floating labels.
    const ImVec2 heading(center.x+std::sin(rotationAngle)*radius*.84f,center.y-std::cos(rotationAngle)*radius*.84f);
    draw_list->AddCircleFilled(heading,2*s,palette.visible,12);
    draw_list->PopClipRect();
}
void 绘图::绘制射线(bool 是否可见,骨骼数据& 骨骼) {
    draw_only_visuals::Ray(ImGui::GetForegroundDrawList(),{left,top,right,bottom},是否可见,
                          绘制.对象信息.敌人信息.isboot,绘制.按钮.射线粗细);
}

void 绘图::绘制骨骼(骨骼数据& 骨骼,D4DVector& 屏幕坐标,bool LineOfSightTo[15],int 距离,int Bonecount) {
    if(距离<0||距离>绘制.骨骼距离限制)return;
    const auto points=ScreenJoints(骨骼);
    std::array<bool,15> visible{};
    for(int i=0;i<15;++i)visible[i]=LineOfSightTo&&LineOfSightTo[i];
    draw_only_visuals::Skeleton(ImGui::GetForegroundDrawList(),{left,top,right,bottom},points,visible,
                               绘制.对象信息.敌人信息.isboot,绘制.按钮.骨骼粗细);
}
void 绘图::绘制名字(string 敌人UID,string 名字,bool isboot,float 计时,bool 是否掐雷,int 阵营,int Bonecount,bool isVisible) {
    draw_only_visuals::Name(ImGui::GetForegroundDrawList(),{left,top,right,bottom},名字,阵营,isVisible,isboot,是否掐雷,计时);
}

void 绘图::绘制血量(float 最大血量,float 当前血量,bool isbot,bool isVisible) {
    draw_only_visuals::Health(ImGui::GetForegroundDrawList(),{left,top,right,bottom},
                             当前血量,最大血量,isVisible,isbot);
}
void 绘图::绘制手持(int 手持,int 状态,int 子弹,int 最大子弹) {
    const std::string weapon=GetHolGunItem(heldconversion(手持));
    draw_only_visuals::Weapon(ImGui::GetForegroundDrawList(),{left,top,right,bottom},weapon,子弹,最大子弹);
}

void 绘图::绘制字体描边(float size, int x, int y, ImVec4 color, const char* str) {
    ImGui::GetBackgroundDrawList()->AddText(nullptr, size, ImVec2(x + 1, y), ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 1.0f)), str);
    ImGui::GetBackgroundDrawList()->AddText(nullptr, size, ImVec2(x - 0.1, y), ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 1.0f)), str);
    ImGui::GetBackgroundDrawList()->AddText(nullptr, size, ImVec2(x, y + 1), ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 1.0f)), str);
    ImGui::GetBackgroundDrawList()->AddText(nullptr, size, ImVec2(x, y - 1), ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 1.0f)), str);
    ImGui::GetBackgroundDrawList()->AddText(nullptr, size, ImVec2(x, y), ImGui::ColorConvertFloat4ToU32(color), str);
}

void 绘图::绘制加粗文本(float size, float x, float y, ImColor color, ImColor color1, const char* str) {
    // ImGui::GetBackgroundDrawList()->AddText(nullptr, size, ImVec2(x - 0.1, y - 0.1), color1, str);
    // ImGui::GetBackgroundDrawList()->AddText(nullptr, size, ImVec2(x + 0.1, y + 0.1), color1, str);
    ImGui::GetBackgroundDrawList()->AddText(nullptr, size, ImVec2(x, y), color, str);
}

bool 绘图::WorldTurnScreen(VecTor2& Screen, VecTor3 World, float Matrix[]) {
    float Camera = Matrix[3] * World.x + Matrix[7] * World.y + Matrix[11] * World.z + Matrix[15];
    if (Camera < 0.03) {
        return false;
    }
    Screen.x = PX + (Matrix[0] * World.x + Matrix[4] * World.y + Matrix[8] * World.z + Matrix[12]) / Camera * PX;
    Screen.y = PY - (Matrix[1] * World.x + Matrix[5] * World.y + Matrix[9] * World.z + Matrix[13]) / Camera * PY;
    return true;
}

// 优化后的爆炸范围 - 纯色半圆球体
// 修正后的爆炸范围 - 东半球球体（优化版）
void 绘图::Draw3DExplosionCircle(VecTor3 center, ImColor color, float radius, float thickness, float matrix[])
{
    const int segments = 18;  // 减少水平分段数
    const int layers = 8;     // 减少垂直分层数
    const float angleStep = M_PI / segments; // 只绘制从0到π的范围（东半球）
    
    // 设置统一颜色（红色，透明度0.2）
    ImColor lineColor = ImColor(255, 0, 0, 91); // RGBA: 255,0,0,51 (透明度0.2)
    
    // 预计算sin/cos值
    static float sinCache[segments + 1];
    static float cosCache[segments + 1];
    static bool cacheInitialized = false;
    
    if (!cacheInitialized) {
        for (int i = 0; i <= segments; i++) {
            float angle = i * angleStep; // 0到π，对应东半球
            sinCache[i] = sinf(angle);
            cosCache[i] = cosf(angle);
        }
        cacheInitialized = true;
    }
    
    // 生成东半球球体的顶点
    std::vector<VecTor3> worldPoints;
    worldPoints.reserve((segments + 1) * (layers + 1));
    
    for (int layer = 0; layer <= layers; layer++) {
        // 垂直角度从0到π，覆盖整个球体的垂直方向
        float theta = (float)layer * (M_PI / layers);
        float currentRadius = radius * sinf(theta); // 当前层的半径
        float yOffset = center.y + radius * cosf(theta); // y坐标偏移
        
        for (int i = 0; i <= segments; i++) {
            VecTor3 point;
            point.x = center.x + currentRadius * cosCache[i];
            point.y = yOffset; // 垂直方向
            point.z = center.z + currentRadius * sinCache[i];
            worldPoints.push_back(point);
        }
    }
    
    // 批量转换坐标
    std::vector<VecTor2> screenPoints;
    screenPoints.reserve(worldPoints.size());
    
    for (const auto& worldPoint : worldPoints) {
        VecTor2 screenPoint;
        if (WorldTurnScreen(screenPoint, worldPoint, matrix)) {
            screenPoints.push_back(screenPoint);
        } else {
            // 标记无效点
            screenPoints.push_back(VecTor2{-FLT_MAX, -FLT_MAX});
        }
    }
    
    // 获取drawList
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    
    // 绘制东半球球体的网格
    if (screenPoints.size() >= (segments + 1) * 2) {
        // 绘制水平圆环（纬线）- 只绘制东半球，间隔绘制以减少线段
        for (int layer = 0; layer <= layers; layer += 2) { // 隔一层绘制
            for (int i = 0; i < segments; i += 2) { // 隔一点绘制
                int idx1 = layer * (segments + 1) + i;
                int idx2 = layer * (segments + 1) + i + 1;
                
                if (screenPoints[idx1].x > -FLT_MAX/2 && screenPoints[idx2].x > -FLT_MAX/2) {
                    drawList->AddLine(
                        ImVec2(screenPoints[idx1].x, screenPoints[idx1].y),
                        ImVec2(screenPoints[idx2].x, screenPoints[idx2].y),
                        lineColor, thickness
                    );
                }
            }
        }
        
        // 绘制垂直线（经线）- 只绘制东半球，间隔绘制以减少线段
        for (int i = 0; i <= segments; i += 2) { // 隔一条经线绘制
            for (int layer = 0; layer < layers; layer += 1) { // 垂直方向不间隔
                int idx1 = layer * (segments + 1) + i;
                int idx2 = (layer + 1) * (segments + 1) + i;
                
                if (screenPoints[idx1].x > -FLT_MAX/2 && screenPoints[idx2].x > -FLT_MAX/2) {
                    drawList->AddLine(
                        ImVec2(screenPoints[idx1].x, screenPoints[idx1].y),
                        ImVec2(screenPoints[idx2].x, screenPoints[idx2].y),
                        lineColor, thickness
                    );
                }
            }
        }
        
        // 绘制赤道圆环（最大圆）- 只绘制一条
        int equatorLayerIdx = (layers / 2) * (segments + 1);
        for (int i = 0; i < segments; i += 1) { // 赤道连续绘制
            int idx1 = equatorLayerIdx + i;
            int idx2 = equatorLayerIdx + i + 1;
            
            if (screenPoints[idx1].x > -FLT_MAX/2 && screenPoints[idx2].x > -FLT_MAX/2) {
                drawList->AddLine(
                    ImVec2(screenPoints[idx1].x, screenPoints[idx1].y),
                    ImVec2(screenPoints[idx2].x, screenPoints[idx2].y),
                    lineColor, thickness
                );
            }
        }
    }
}

void 绘图::Parabola(VecTor3 obj, float Matrix[]) {
    static std::vector<VecTor3> grenadePath;
    static const size_t MAX_POINTS = 80; // 进一步减少存储点数以优化CPU
    
    // 添加新位置
    grenadePath.push_back(obj);
    
    // 控制路径点数量
    if (grenadePath.size() > MAX_POINTS) {
        grenadePath.erase(grenadePath.begin(), grenadePath.begin() + (grenadePath.size() - MAX_POINTS));
    }
    
    // 批量转换坐标
    std::vector<ImVec2> screenPath;
    screenPath.reserve(grenadePath.size());
    
    for (const VecTor3& pos : grenadePath) {
        VecTor2 screenPos;
        if (WorldTurnScreen(screenPos, pos, Matrix)) {
            screenPath.push_back(ImVec2(screenPos.x, screenPos.y));
        } else {
            screenPath.push_back(ImVec2(-1, -1));
        }
    }
    
    // 绘制轨迹线
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    ImColor trajectoryColor = ImColor(0, 255, 0, 191);
    
    for (size_t i = 1; i < screenPath.size(); ++i) {
        if (screenPath[i-1].x >= 0 && screenPath[i].x >= 0) {
            drawList->AddLine(
                screenPath[i-1],
                screenPath[i],
                trajectoryColor, 
                2.0f
            );
        }
    }
    
    // 绘制末端标记
    if (!screenPath.empty() && screenPath.back().x >= 0) {
        drawList->AddCircleFilled(
            screenPath.back(),
            3.0f,
            trajectoryColor,
            8
        );
    }
}


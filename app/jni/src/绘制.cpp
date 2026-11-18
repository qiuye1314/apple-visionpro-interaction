// lsrael 1.39 drawing edition. Read-only object and scene visualization.
#include "结构体.h"
#include "PhysX.h"
#include "辅助类.h"
#include "图片调用.h"
#include "物资ID.h"
#include "DrawOnlyVisuals.h"
#include "DrawOnlyProjection.h"
#include "DrawOnlyMortarVisuals.h"
#include "DrawOnlyActorRead.h"
#include "DrawOnlyFrameStability.h"
#include <android/log.h>
#include <chrono>
#include <sstream>
#include <unordered_map>

extern 绘制 绘制;
D3DVector PovLocation;
FRotator PovRotator;
std::unordered_map<std::string, std::string> customMaterialMap;
int configType = 1;
bool g_materialEnabled = true;
static std::map<std::string, int> Antitankgrenade;

namespace {
constexpr const char* kDrawConfig = "/data/local/tmp/lsrael_draw_139";
constexpr const char* kDrawConfigVersion = "lsrael_draw_139_v1";
uintptr_t g_mortarBss = 0;
double g_nextMortarBssProbe = 0;
bool g_drawFrameReady = false;
bool g_selfTeamKnown = false;
bool g_cameraReady = false;
struct ViewSnapshot {
    struct 地址 addresses{};
    struct 自身数据 self{};
    D3DVector camera{};
    FRotator rotation{};
    bool teamKnown=false;
    bool cameraReady=false;
};
draw_only_stability::Recent<ViewSnapshot> g_viewHistory;
draw_only_stability::ActorList g_actorList;
std::unordered_map<uintptr_t,draw_only_stability::CharacterHistory<D3DVector>> g_characterHistory;
uintptr_t g_contextWorld=0,g_contextLevel=0,g_contextSelf=0;
struct ReadDiagnostics {
    unsigned frames=0,failedReads=0,reusedFrames=0,reusedLists=0,reusedPositions=0;
    unsigned candidates=0,characters=0,positionDrops=0,unknownTypes=0,filtered=0,drawn=0;
    double maxViewMs=0,maxObjectsMs=0;
    double nextLog=0;
} g_readDiagnostics;

void ClearReadHistory() {
    g_viewHistory.Clear(); g_actorList.Clear(); g_characterHistory.clear();
    g_contextWorld=g_contextLevel=g_contextSelf=0;
    g_selfTeamKnown=false;
    g_cameraReady=false;
}

void LogReadDiagnostics(double now) {
    if (now<g_readDiagnostics.nextLog) return;
    const auto& d=g_readDiagnostics;
    __android_log_print(ANDROID_LOG_INFO,"lsrael-draw",
        "frames=%u read_fail=%u frame_reuse=%u list_reuse=%u position_reuse=%u candidates=%u characters=%u position_drop=%u unknown_type=%u filtered=%u drawn=%u view_max_ms=%.2f objects_max_ms=%.2f",
        d.frames,d.failedReads,d.reusedFrames,d.reusedLists,d.reusedPositions,
        d.candidates,d.characters,d.positionDrops,d.unknownTypes,d.filtered,d.drawn,d.maxViewMs,d.maxObjectsMs);
    g_readDiagnostics={}; g_readDiagnostics.nextLog=now+2.0;
}

bool ValidCoordinate(const D3DVector& p) {
    return draw_only_visuals::Finite(p.X) && draw_only_visuals::Finite(p.Y) && draw_only_visuals::Finite(p.Z) &&
           std::fabs(p.X) <= 1.0e8f && std::fabs(p.Y) <= 1.0e8f && std::fabs(p.Z) <= 1.0e8f;
}


}

void loadCustomMaterials() {
    customMaterialMap.clear();
    std::ifstream file("/data/自定义物资.txt");
    std::string line;
    while (std::getline(file, line)) {
        const auto split = line.find('@');
        if (split == std::string::npos || split == 0 || split + 1 >= line.size()) continue;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        customMaterialMap[line.substr(0, split)] = line.substr(split + 1);
    }
}

void 绘制::OffScreen(ImDrawList *ImDraw, D4DVector Obj, float camear, ImU32 color, float Radius, float 距离)
{
  ImRect screen_rect = {0.0f, 0.0f, 真实PX, 真实PY};
  if (Obj.Z > 0 && screen_rect.Contains({Obj.X, Obj.Y}))
  return;
  auto screen_center = screen_rect.GetCenter();
  auto angle = atan2(screen_center.y - Obj.Y, screen_center.x - Obj.X);
  angle += camear > 0 ? M_PI : 0.0f;
  D2DVector arrow_center {
    screen_center.x + Radius * cosf(angle),
    screen_center.y + Radius * sinf(angle)
  };
  /*
  std::array<ImVec2, 4> points {
  ImVec2(-30.8f, -12.04f),
  ImVec2(0.0f, 0.0f),
  ImVec2(-38.8f, 12.4f),
  ImVec2(-25.2f, 0.0f)
  };
  */
  std::array<ImVec2, 4>points {
    ImVec2(-22.0f, -8.6f),
    ImVec2(0.0f, 0.0f),
    ImVec2(-22.0f, 8.6f),
    ImVec2(-18.0f, 0.0f)
  };
  D2DVector tpoint;
  for (auto& point : points)
  {
    auto x = point.x * 1.155f;
    auto y = point.y * 1.155f;
    point.x = arrow_center.X + x * cosf(angle) - y * sinf(angle);
    point.y = arrow_center.Y + x * sinf(angle) + y * cosf(angle);
    tpoint.X=point.x;
    tpoint.Y=point.y;
  }
  float alpha = 1.0f;
  if (camear > 0)
  {
    constexpr float nearThreshold = 200 * 200;
    ImVec2 screen_outer_diff = {
      Obj.X < 0 ? abs(Obj.X) : (Obj.X > screen_rect.Max.x ? Obj.X - screen_rect.Max.x : 0.0f),
      Obj.Y < 0 ? abs(Obj.Y) : (Obj.Y > screen_rect.Max.y ? Obj.Y - screen_rect.Max.y : 0.0f),
    };
    float distance = static_cast<float>(pow(screen_outer_diff.x, 2) + pow(screen_outer_diff.y, 2));
    alpha = camear < 0 ? 1.0f : (distance / nearThreshold);
  }
  ImColor arrowColor = color;
  //arrowColor.Value.w = std::min(alpha, 1.0f);
  arrowColor.Value.w = (alpha < 1.0f) ? alpha : 1.0f;
  ImDraw->AddTriangleFilled(points[0], points[1], points[3], arrowColor);
  ImDraw->AddTriangleFilled(points[2], points[1], points[3], arrowColor);
  ImDraw->AddQuad(points[0], points[1], points[2], points[3], ImColor(0.0f, 0.0f, 0.0f, alpha), 1.335f);
  
  float radius = 30.0f;
  ImColor textColor = arrowColor;
//  ImDraw->AddCircle({ tpoint.X, tpoint.Y }, radius, textColor, 12, 2.0f);
  
  string tmp= to_string((int)距离) + "M";
  auto textSize = ImGui::CalcTextSize(tmp.c_str(), 0, 28);
  ImDraw->AddText(NULL, 28, {tpoint.X-(textSize.x/2), tpoint.Y}, ImColor(draw_only_visuals::PaletteColors().text), tmp.c_str());
}
void 绘制::ReadsSaveData(ifstream& file) {
    string version;
    if (!(file >> version) || version != kDrawConfigVersion) return;
    string key; float value;
    while (file >> key >> value) {
        if (!draw_only_visuals::Finite(value)) continue;
        if (key == "绘制") { 按钮.绘制 = value != 0.0f; continue; }
        if (key == "方框") { 按钮.方框 = value != 0.0f; continue; }
        if (key == "骨骼") { 按钮.骨骼 = value != 0.0f; continue; }
        if (key == "射线") { 按钮.射线 = value != 0.0f; continue; }
        if (key == "名字") { 按钮.名字 = value != 0.0f; continue; }
        if (key == "距离") { 按钮.距离 = value != 0.0f; continue; }
        if (key == "血量") { 按钮.血量 = value != 0.0f; continue; }
        if (key == "人数") { 按钮.人数 = value != 0.0f; continue; }
        if (key == "手持") { 按钮.手持 = value != 0.0f; continue; }
        if (key == "显示头部骨骼点") { 按钮.显示头部骨骼点 = value != 0.0f; continue; }
        if (key == "忽略人机") { 按钮.忽略人机 = value != 0.0f; continue; }
        if (key == "忽略倒地") { 按钮.忽略倒地 = value != 0.0f; continue; }
        if (key == "掩体识别") { 按钮.掩体识别 = value != 0.0f; continue; }
        if (key == "绘制全图") { 按钮.绘制全图 = value != 0.0f; continue; }
        if (key == "雷达") { 按钮.雷达 = value != 0.0f; continue; }
        if (key == "背敌预警") { 按钮.背敌预警 = value != 0.0f; continue; }
        if (key == "手雷预警") { 按钮.手雷预警 = value != 0.0f; continue; }
        if (key == "迫击炮落点") { 按钮.迫击炮落点 = value != 0.0f; continue; }
        if (key == "PhysX画线") { 按钮.PhysX画线 = value != 0.0f; continue; }
        if (key == "车辆") { 按钮.车辆 = value != 0.0f; continue; }
        if (key == "载具血量") { 按钮.载具血量 = value != 0.0f; continue; }
        if (key == "载具油量") { 按钮.载具油量 = value != 0.0f; continue; }
        if (key == "盒子") { 按钮.盒子 = value != 0.0f; continue; }
        if (key == "盒子物资") { 按钮.盒子物资 = value != 0.0f; continue; }
        if (key == "空投") { 按钮.空投 = value != 0.0f; continue; }
        if (key == "绘制宝箱") { 按钮.绘制宝箱 = value != 0.0f; continue; }
        if (key == "绘制药箱") { 按钮.绘制药箱 = value != 0.0f; continue; }
        if (key == "绘制武器箱") { 按钮.绘制武器箱 = value != 0.0f; continue; }
        if (key == "隐藏已开启") { 按钮.隐藏已开启 = value != 0.0f; continue; }
        if (key == "超级物资箱") { 按钮.超级物资箱 = value != 0.0f; continue; }
        if (key == "海岛宝箱") { 按钮.海岛宝箱 = value != 0.0f; continue; }
        if (key == "绘制信号枪") { 按钮.绘制信号枪 = value != 0.0f; continue; }
        if (key == "其他物资") { 按钮.其他物资 = value != 0.0f; continue; }
        if (key == "冲锋枪械") { 按钮.冲锋枪械 = value != 0.0f; continue; }
        if (key == "投掷物品") { 按钮.投掷物品 = value != 0.0f; continue; }
        if (key == "散弹枪械") { 按钮.散弹枪械 = value != 0.0f; continue; }
        if (key == "显示倍镜") { 按钮.显示倍镜 = value != 0.0f; continue; }
        if (key == "显示子弹") { 按钮.显示子弹 = value != 0.0f; continue; }
        if (key == "显示步枪") { 按钮.显示步枪 = value != 0.0f; continue; }
        if (key == "显示药品") { 按钮.显示药品 = value != 0.0f; continue; }
        if (key == "显示配件") { 按钮.显示配件 = value != 0.0f; continue; }
        if (key == "显示防具") { 按钮.显示防具 = value != 0.0f; continue; }
        if (key == "狙击枪械") { 按钮.狙击枪械 = value != 0.0f; continue; }
        if (key == "当前帧率") { 按钮.当前帧率 = std::clamp(value, 30.00f, 165.00f); continue; }
        if (key == "方框粗细") { 按钮.方框粗细 = std::clamp(value, 0.60f, 3.00f); continue; }
        if (key == "骨骼粗细") { 按钮.骨骼粗细 = std::clamp(value, 0.60f, 3.00f); continue; }
        if (key == "射线粗细") { 按钮.射线粗细 = std::clamp(value, 0.60f, 2.40f); continue; }
        if (key == "雷达X") { 按钮.雷达X = std::clamp(value, 0.00f, 8192.00f); continue; }
        if (key == "雷达Y") { 按钮.雷达Y = std::clamp(value, 0.00f, 8192.00f); continue; }
        if (key == "雷达缩放") { 按钮.雷达缩放 = std::clamp(value, 0.50f, 2.00f); continue; }
        if (key == "骨骼距离限制") 骨骼距离限制 = std::clamp(value, 30.0f, 500.0f);
        else if (key == "物资显示") g_materialEnabled = value != 0.0f;
        else if (key == "物资配置") configType = value == 2.0f ? 2 : 1;
    }
}

void 绘制::WriteSaveData(ofstream& file) {
    file << kDrawConfigVersion << '\n';
    file << "绘制 " << 按钮.绘制 << '\n';
    file << "方框 " << 按钮.方框 << '\n';
    file << "骨骼 " << 按钮.骨骼 << '\n';
    file << "射线 " << 按钮.射线 << '\n';
    file << "名字 " << 按钮.名字 << '\n';
    file << "距离 " << 按钮.距离 << '\n';
    file << "血量 " << 按钮.血量 << '\n';
    file << "人数 " << 按钮.人数 << '\n';
    file << "手持 " << 按钮.手持 << '\n';
    file << "显示头部骨骼点 " << 按钮.显示头部骨骼点 << '\n';
    file << "忽略人机 " << 按钮.忽略人机 << '\n';
    file << "忽略倒地 " << 按钮.忽略倒地 << '\n';
    file << "掩体识别 " << 按钮.掩体识别 << '\n';
    file << "绘制全图 " << 按钮.绘制全图 << '\n';
    file << "雷达 " << 按钮.雷达 << '\n';
    file << "背敌预警 " << 按钮.背敌预警 << '\n';
    file << "手雷预警 " << 按钮.手雷预警 << '\n';
    file << "迫击炮落点 " << 按钮.迫击炮落点 << '\n';
    file << "PhysX画线 " << 按钮.PhysX画线 << '\n';
    file << "车辆 " << 按钮.车辆 << '\n';
    file << "载具血量 " << 按钮.载具血量 << '\n';
    file << "载具油量 " << 按钮.载具油量 << '\n';
    file << "盒子 " << 按钮.盒子 << '\n';
    file << "盒子物资 " << 按钮.盒子物资 << '\n';
    file << "空投 " << 按钮.空投 << '\n';
    file << "绘制宝箱 " << 按钮.绘制宝箱 << '\n';
    file << "绘制药箱 " << 按钮.绘制药箱 << '\n';
    file << "绘制武器箱 " << 按钮.绘制武器箱 << '\n';
    file << "隐藏已开启 " << 按钮.隐藏已开启 << '\n';
    file << "超级物资箱 " << 按钮.超级物资箱 << '\n';
    file << "海岛宝箱 " << 按钮.海岛宝箱 << '\n';
    file << "绘制信号枪 " << 按钮.绘制信号枪 << '\n';
    file << "其他物资 " << 按钮.其他物资 << '\n';
    file << "冲锋枪械 " << 按钮.冲锋枪械 << '\n';
    file << "投掷物品 " << 按钮.投掷物品 << '\n';
    file << "散弹枪械 " << 按钮.散弹枪械 << '\n';
    file << "显示倍镜 " << 按钮.显示倍镜 << '\n';
    file << "显示子弹 " << 按钮.显示子弹 << '\n';
    file << "显示步枪 " << 按钮.显示步枪 << '\n';
    file << "显示药品 " << 按钮.显示药品 << '\n';
    file << "显示配件 " << 按钮.显示配件 << '\n';
    file << "显示防具 " << 按钮.显示防具 << '\n';
    file << "狙击枪械 " << 按钮.狙击枪械 << '\n';
    file << "当前帧率 " << 按钮.当前帧率 << '\n';
    file << "方框粗细 " << 按钮.方框粗细 << '\n';
    file << "骨骼粗细 " << 按钮.骨骼粗细 << '\n';
    file << "射线粗细 " << 按钮.射线粗细 << '\n';
    file << "雷达X " << 按钮.雷达X << '\n';
    file << "雷达Y " << 按钮.雷达Y << '\n';
    file << "雷达缩放 " << 按钮.雷达缩放 << '\n';
    file << "骨骼距离限制 " << 骨骼距离限制 << '\n';
    file << "物资显示 " << g_materialEnabled << '\n';
    file << "物资配置 " << configType << '\n';
}

void 绘制::保存配置(const char*) {
    const string temporary = string(kDrawConfig) + ".tmp";
    ofstream file(temporary, ios::out | ios::trunc);
    if (!file) return;
    WriteSaveData(file);
    file.flush();
    const bool okay = file.good();
    file.close();
    if (okay) rename(temporary.c_str(), kDrawConfig);
}

void 绘制::读取配置(const char*) {
    ifstream file(kDrawConfig);
    if (file) ReadsSaveData(file);
}

void 绘制::初始化绘制(string 包名, int 真实X, int 真实Y) {
    pid = 读写.getPID(包名.c_str());
    读写.初始化读写(pid);
    绘图.初始化绘图(真实X, 真实Y);
    真实PX = 真实X;
    真实PY = 真实Y;
    PX = std::max(真实X, 真实Y) / 2.0f;
    PY = std::min(真实X, 真实Y) / 2.0f;
    if (!骨骼) 骨骼 = new class 骨骼(&读写);
    地址.libue4 = pid > 0 ? 读写.get_module_base("libUE4.so") : 0;
    g_mortarBss = pid > 0 && 地址.libue4 ? 读写.get_module_bss("libUE4.so") : 0;
    g_nextMortarBssProbe = 0;
    g_drawFrameReady = false;
    ClearReadHistory();
}

void 绘制::更新地址数据() {
    g_drawFrameReady=false; 世界数量=0;
    const double now=ImGui::GetTime();
    if (pid<=0 || !地址.libue4) { ClearReadHistory(); return; }
    const auto read=[&](uintptr_t address,void* output,size_t size) {
        const bool ok=读写.readv(address,output,size);
        if (!ok) ++g_readDiagnostics.failedReads;
        return ok;
    };
    const auto pointer=[&](uintptr_t address,uintptr_t& result) {
        return draw_only_stability::ReadValue(read,address,result) && result>=0x1000;
    };
    uintptr_t world=0;
    const bool worldKnown=draw_only_stability::ReadValue(read,地址.libue4+V139::GWorld基址,world);
    if (worldKnown && world!=g_contextWorld) {
        ClearReadHistory(); g_contextWorld=world>=0x1000?world:0;
    }
    if (worldKnown && world<0x1000) { ClearReadHistory(); return; }
    ViewSnapshot next{};
    next.addresses.libue4=地址.libue4;
    bool complete=false;
    do {
        if (!worldKnown) break;
        next.addresses.世界指针=world;
        uintptr_t level=0;
        const bool levelKnown=draw_only_stability::ReadValue(read,world+V139::世界_持久关卡,level);
        if (!levelKnown) break;
        if (level<0x1000) { ClearReadHistory(); return; }
        if (level!=g_contextLevel) {
            ClearReadHistory(); g_contextWorld=world; g_contextLevel=level;
        }
        next.addresses.世界地址=level;
        const auto acceptSelf=[&](uintptr_t actor) {
            uintptr_t root=0; int team=-1; D3DVector position{};
            if (actor<0x1000 || !pointer(actor+V139::坐标组件,root) ||
                !draw_only_stability::ReadValue(read,root+V139::坐标偏移,position) ||
                !ValidCoordinate(position)) return false;
            const bool teamKnown=draw_only_stability::ReadValue(read,actor+V139::队伍ID,team) &&
                draw_only_actor::TeamValid(team);
            next.addresses.自身地址=actor;
            next.self.自身队伍=teamKnown?team:-1; next.self.坐标=position; next.teamKnown=teamKnown;
            return true;
        };
        uintptr_t instance=0,players=0,player=0,controller=0,self=0;
        bool selfKnown=pointer(world+V139::世界_游戏实例,instance) &&
            pointer(instance+V139::实例_本地玩家,players) && pointer(players,player) &&
            pointer(player+V139::本地玩家_控制器,controller) &&
            draw_only_stability::ReadValue(read,controller+V139::自身_人物,self);
        if (!selfKnown) {
            uintptr_t a=0,b=0,c=0;
            selfKnown=pointer(world+V139::自身1,a) && pointer(a+V139::自身2,b) &&
                pointer(b+V139::自身3,c) && draw_only_stability::ReadValue(read,c+V139::自身_人物,self);
            if (selfKnown) controller=c;
        }
        if (!selfKnown) break;
        // A successful Pawn change invalidates the old view even when the new
        // pawn's position has not become readable yet. Explicit null is final.
        if (self!=g_contextSelf) {
            const bool respawn=g_contextSelf!=0;
            ClearReadHistory(); g_contextWorld=world; g_contextLevel=level; g_contextSelf=self;
            if (respawn) { PhysXGate::g_spike.store(3); PhysXGate::g_stress.store(0); }
        }
        if (self<0x1000) { ClearReadHistory(); return; }
        if (!acceptSelf(self)) break;
        uintptr_t matrixRoot=0,matrixOwner=0;
        if (!pointer(地址.libue4+V139::矩阵基址,matrixRoot) ||
            !pointer(matrixRoot+V139::矩阵_1,matrixOwner)) break;
        next.addresses.矩阵地址=matrixOwner+V139::矩阵_2;
        next.addresses.矩阵地址_Tol=next.addresses.矩阵地址;
        if (!draw_only_stability::ReadValue(read,next.addresses.矩阵地址,next.self.矩阵) ||
            !draw_only_mortar::MatrixValid(next.self.矩阵)) break;
        pointer(world+V139::世界_对局状态,next.addresses.对局状态);
        if (!pointer(地址.libue4+V139::GName基址,next.addresses.类地址)) next.addresses.类地址=0;
        next.addresses.PlayerController=controller;
        uintptr_t camera=0;
        if (pointer(controller+V139::控制器_相机管理,camera)) {
            next.addresses.PlayerCameraManager=camera;
            next.cameraReady=draw_only_stability::ReadValue(read,camera+V139::相机_PovLocation,next.camera) && ValidCoordinate(next.camera);
            float rotation[3]{};
            if (draw_only_stability::ReadValue(read,camera+V139::相机_PovRotation,rotation) &&
                draw_only_visuals::Finite(rotation[0]) && draw_only_visuals::Finite(rotation[1]) &&
                draw_only_visuals::Finite(rotation[2])) next.rotation=FRotator(rotation[0],rotation[1],rotation[2]);
            float fov=0;
            if (draw_only_stability::ReadValue(read,camera+V139::相机_PovFov,fov) &&
                draw_only_visuals::Finite(fov) && fov>1 && fov<179) next.self.Fov=fov;
        }
        next.self.CameraLocation=next.camera; next.self.CameraRotator=next.rotation;
        float yaw=0;
        if (draw_only_stability::ReadValue(read,controller+0x624,yaw) && draw_only_visuals::Finite(yaw))
            next.self.准星Y=yaw-90.f;
        int total=0,alive=0,teams=0;
        if (next.addresses.对局状态>=0x1000) {
            draw_only_stability::ReadValue(read,next.addresses.对局状态+V139::对局_总玩家,total);
            draw_only_stability::ReadValue(read,next.addresses.对局状态+V139::对局_存活玩家,alive);
            draw_only_stability::ReadValue(read,next.addresses.对局状态+V139::对局_存活队伍,teams);
        }
        next.self.全图人数=std::clamp(total,0,1000);
        next.self.玩家人数=std::clamp(alive,0,next.self.全图人数);
        next.self.队伍数=std::clamp(teams,0,1000);
        uintptr_t finalWorld=0;
        if (!draw_only_stability::ReadValue(read,地址.libue4+V139::GWorld基址,finalWorld)) break;
        if (finalWorld!=world) { ClearReadHistory(); return; }
        uintptr_t finalLevel=0,finalSelf=0;
        if (!draw_only_stability::ReadValue(read,world+V139::世界_持久关卡,finalLevel)) break;
        if (finalLevel!=level) { ClearReadHistory(); return; }
        if (!draw_only_stability::ReadValue(read,controller+V139::自身_人物,finalSelf)) break;
        if (finalSelf!=self) { ClearReadHistory(); return; }
        complete=true;
    } while (false);
    if (complete) g_viewHistory.Store(next,now);
    else {
        if (!g_viewHistory.Restore(next,now)) { ClearReadHistory(); return; }
        ++g_readDiagnostics.reusedFrames;
    }
    地址=next.addresses; 自身数据=next.self;
    PovLocation=next.camera; PovRotator=next.rotation; g_selfTeamKnown=next.teamKnown;
    g_cameraReady=next.cameraReady;
    g_drawFrameReady=complete && 地址.对局状态>=0x1000;
    if (!g_actorList.Refresh(read,地址.世界地址,now)) return;
    if (g_actorList.reused) ++g_readDiagnostics.reusedLists;
    地址.数组地址=g_actorList.Header().data;
    世界数量=static_cast<int>(g_actorList.Values().size());
    // Successful removal is authoritative; do not leave a cached ghost behind.
    const std::unordered_set<uintptr_t> present(g_actorList.Values().begin(),g_actorList.Values().end());
    for (auto it=g_characterHistory.begin();it!=g_characterHistory.end();) {
        if (!present.count(it->first) || !it->second.position.Fresh(now)) it=g_characterHistory.erase(it);
        else ++it;
    }
}

void 绘制::更新对象数据() {
    if (!骨骼 || !地址.自身地址 || !地址.数组地址 || 世界数量 <= 0) return;
    const double now=ImGui::GetTime();
    const auto read=[&](uintptr_t address,void* output,size_t size) {
        const bool ok=读写.readv(address,output,size);
        if (!ok) ++g_readDiagnostics.failedReads;
        return ok;
    };
    const draw_only_actor::Filters filters{自身数据.自身队伍,g_selfTeamKnown,按钮.忽略人机,按钮.忽略倒地};
    if (按钮.雷达) 绘图.RenderRadarScan(ImGui::GetForegroundDrawList(),
        {按钮.雷达X, 按钮.雷达Y}, 150.0f, 100, 按钮.rotationAngle, 150.0f, 自身数据.准星Y);
    int 绘制人机 = 0, 绘制真人 = 0;
    for (const uintptr_t actor:g_actorList.Values()) {
        对象地址.敌人地址 = actor;
        if (对象地址.敌人地址 < 0x1000 || 对象地址.敌人地址 == 地址.自身地址) continue;
        ++g_readDiagnostics.candidates;
        uintptr_t root=0; uint32_t ClassID=0;
        const bool rootKnown=draw_only_stability::ReadValue(read,actor+V139::坐标组件,root);
        const bool classKnown=draw_only_stability::ReadValue(read,actor+V139::对象_名称,ClassID);
        if ((rootKnown && root<0x1000) || (classKnown && ClassID>=0x4000000)) {
            g_characterHistory.erase(actor); ++g_readDiagnostics.positionDrops; continue;
        }
        draw_only_stability::CharacterHistory<D3DVector> localHistory;
        auto historyIt=g_characterHistory.find(actor);
        auto* history=historyIt==g_characterHistory.end()?&localHistory:&historyIt->second;
        history->Match(root,rootKnown,ClassID,classKnown);
        对象信息.敌人信息 = {};
        auto& position=对象信息.敌人信息.坐标;
        if (rootKnown && draw_only_stability::ReadValue(read,root+V139::坐标偏移,position) && ValidCoordinate(position))
            history->position.Store(position,now);
        else if (history->position.Restore(position,now)) ++g_readDiagnostics.reusedPositions;
        else { ++g_readDiagnostics.positionDrops; continue; }
        const float dx=自身数据.坐标.X-position.X,dy=自身数据.坐标.Y-position.Y,dz=自身数据.坐标.Z-position.Z;
        const float meters=std::sqrt(dx*dx+dy*dy+dz*dz)*.01f;
        if (!draw_only_visuals::Finite(meters) || meters>3000) continue;
        对象信息.敌人信息.距离=static_cast<int>(meters);
        float camear_r = 自身数据.矩阵[3] * 对象信息.敌人信息.坐标.X + 自身数据.矩阵[7] * 对象信息.敌人信息.坐标.Y + 自身数据.矩阵[11] * 对象信息.敌人信息.坐标.Z + 自身数据.矩阵[15];
        if (!draw_only_visuals::Finite(camear_r) || std::fabs(camear_r) <= .01f) continue;
        float r_x = PX + (自身数据.矩阵[0] * 对象信息.敌人信息.坐标.X + 自身数据.矩阵[4] * 对象信息.敌人信息.坐标.Y + 自身数据.矩阵[8] * 对象信息.敌人信息.坐标.Z + 自身数据.矩阵[12]) / camear_r * PX;
        float r_y = PY - (自身数据.矩阵[1] * 对象信息.敌人信息.坐标.X + 自身数据.矩阵[5] * 对象信息.敌人信息.坐标.Y + 自身数据.矩阵[9] * (对象信息.敌人信息.坐标.Z - 5) + 自身数据.矩阵[13]) / camear_r * PY;
        float r_z = PY - (自身数据.矩阵[1] * 对象信息.敌人信息.坐标.X + 自身数据.矩阵[5] * 对象信息.敌人信息.坐标.Y + 自身数据.矩阵[9] * (对象信息.敌人信息.坐标.Z + 205) + 自身数据.矩阵[13]) / camear_r * PY;
        D4DVector t_屏幕坐标 = {r_x - (r_y - r_z) / 4, r_y, (r_y - r_z) / 2, r_y - r_z};

        float X = r_x - (r_y - r_z) / 4;
        float Y = r_y;
        float W = (r_y - r_z) / 2;
        float MIDDLE = X + W / 2;
        float BOTTOM = Y + W;
        float TOP = Y - W;

        if (!draw_only_visuals::Finite(camear_r) || std::fabs(camear_r) < .01f ||
            !draw_only_visuals::Finite(r_x) || !draw_only_visuals::Finite(r_y) || !draw_only_visuals::Finite(r_z)) continue;
        char 计算地址[32]{};
        snprintf(计算地址, sizeof(计算地址), "%lx", static_cast<unsigned long>(对象地址.敌人地址));
        char ClassName[65]{};
        if (classKnown && 地址.类地址>=0x1000) {
            uintptr_t page=0,entry=0;
            if (draw_only_stability::ReadValue(read,地址.类地址+(ClassID/0x4000)*8,page) && page>=0x1000 &&
                draw_only_stability::ReadValue(read,page+(ClassID%0x4000)*8,entry) && entry>=0x1000)
                for (int retry=0;retry<2;++retry) {
                    std::memset(ClassName,0,sizeof(ClassName));
                    if (read(entry+0xC,ClassName,sizeof(ClassName)-1)) break;
                    std::memset(ClassName,0,sizeof(ClassName));
                }
        }
if (按钮.手雷预警) {
    int 手雷ID = 读写.getDword(对象地址.敌人地址 + V139::投掷物ID); 
    const char* 投掷物信息 = Getagrenade(手雷ID);
    if (手雷ID == 602004 or 手雷ID == 9825004) {
        if (!计时器.hasTimer(计算地址)) {
            手雷类.add(计算地址, 对象信息.敌人信息.坐标.X, 对象信息.敌人信息.坐标.Y);
        } else {
            手雷类.remove(计算地址);
        }
    }
    if (投掷物信息 != nullptr) {
        std::string name = 投掷物信息;
        if (t_屏幕坐标.W > 0) {
            float 计算_max = 7.0f - 计时器.elapsed(计算地址);
            if (计算_max >= 0) {
                name += "[" + std::to_string((int)对象信息.敌人信息.距离) + "米]";
                auto textSize = ImGui::CalcTextSize(name.c_str(), 0, 35);
                if (手雷ID != 602004 && 手雷ID != 9825004) {
                    ImGui::GetForegroundDrawList()->AddText(NULL, 35, {r_x - (textSize.x / 2), r_y + 30}, ImColor(255, 0, 0, 255), name.c_str());
                }
                if (手雷ID == 602004 or 手雷ID == 9825004) {
                    float 计时 = 计算_max / 7 * 100;
                    float aa = 计时 * 3.6;
                    
                    string 计算 = std::to_string((int)计算_max);
                    计算 += "秒";
                    textSize = ImGui::CalcTextSize(计算.c_str(), 0, 32);
                    ImGui::GetForegroundDrawList()->AddText(NULL, 32, {r_x - (textSize.x / 2), r_y - 18}, ImColor(255, 0, 0, 255), 计算.c_str());

                    string 距离 = std::to_string((int)对象信息.敌人信息.距离);
                    距离 += "米";
                    textSize = ImGui::CalcTextSize(距离.c_str(), 0, 32);
                    ImGui::GetForegroundDrawList()->AddText(NULL, 32, {r_x - (textSize.x / 2), r_y + 20}, ImColor(0, 255, 0, 255), 距离.c_str());

                    // 修复1：显式创建VecTor3对象
                    绘图::VecTor3 爆炸中心坐标;
                    爆炸中心坐标.x = 对象信息.敌人信息.坐标.X;
                    爆炸中心坐标.y = 对象信息.敌人信息.坐标.Y;
                    爆炸中心坐标.z = 对象信息.敌人信息.坐标.Z;
                    
                    // 修复2：确保函数调用正确
                    绘图.Draw3DExplosionCircle(爆炸中心坐标, ImColor(255, 0, 0, 255), 350.0f, 1.5f, 自身数据.矩阵);
                }
            }
        }

        绘图::VecTor3 坐标 = {对象信息.敌人信息.坐标.X, 对象信息.敌人信息.坐标.Y, 对象信息.敌人信息.坐标.Z};
        绘图.Parabola(坐标, 自身数据.矩阵);
    }
}//
        if (t_屏幕坐标.W > 0) {

            const auto palette = draw_only_visuals::PaletteColors();
            auto label = [&](const string& name, float y = 0.0f) {
                draw_only_visuals::Text(ImGui::GetForegroundDrawList(), r_x, r_y + y,
                    name, 21.0f, palette.secondary);
            };
            if (按钮.车辆) {
                string name = getMaterialName(ClassName);
                if (name != "Error") {
                    name += "  " + to_string(对象信息.敌人信息.距离) + "m";
                    if (按钮.载具血量 || 按钮.载具油量) {
                        const auto vehicle = 读写.getPtr64(对象地址.敌人地址 + V139::载具指针);
                        auto percent = [&](uintptr_t value, uintptr_t maximum) {
                            const float current = 读写.getFloat(vehicle + value);
                            const float total = 读写.getFloat(vehicle + maximum);
                            return draw_only_visuals::Finite(current) && draw_only_visuals::Finite(total) && total > 0.0f
                                ? static_cast<int>(std::clamp(current / total * 100.0f, 0.0f, 100.0f)) : 0;
                        };
                        if (按钮.载具血量) name += "  耐久 " + to_string(percent(V139::载具耐久, V139::载具耐久Max)) + "%";
                        if (按钮.载具油量) name += "  燃料 " + to_string(percent(V139::载具油量, V139::载具油量Max)) + "%";
                    }
                    label(name);
                }
            }
            if (g_materialEnabled) {
                const bool deadBox = strstr(ClassName, "CharacterDeadInventoryBox_C") || strstr(ClassName, "PickUpListWrapperActor") ||
                    strstr(ClassName, "RollTombBox_") || strstr(ClassName, "EscapePlayerTombBox") || strstr(ClassName, "DeadInventoryBox") ||
                    strstr(ClassName, "_TrainingBoxLi");
                string container;
                if (按钮.盒子 && deadBox) container = "物资盒";
                else if (按钮.空投 && strstr(ClassName, "AirDropBox")) container = "空投";
                else if ((按钮.超级物资箱 || 按钮.海岛宝箱) && strstr(ClassName, "MilitarySupplyBoxBase_")) container = "高级物资箱";
                else if (按钮.绘制信号枪 && (strstr(ClassName, "Flaregun") || strstr(ClassName, "FlareGun"))) container = "信号枪";
                else if (按钮.绘制信号枪 && strstr(ClassName, "Ammo_Flare")) container = "信号弹";
                else if (按钮.绘制宝箱 && (strstr(ClassName, "EscapeBox_SupplyBox_") || strstr(ClassName, "EscapeBoxHight_SupplyBox_"))) container = "宝箱";
                else if (按钮.绘制药箱 && (strstr(ClassName, "EscapeBox_Medical_") || strstr(ClassName, "EscapeBoxHight_Medical_"))) container = "药箱";
                else if (按钮.绘制武器箱 && (strstr(ClassName, "EscapeBox_Weapon_") || strstr(ClassName, "EscapeBoxHight_Weapon_"))) container = "武器箱";
                if (!container.empty()) {
                    const bool themedBox = container == "宝箱" || container == "药箱" || container == "武器箱";
                    const bool opened = themedBox && 读写.getDword(对象地址.敌人地址 + V139::宝箱开启) == 1;
                    if (!(opened && 按钮.隐藏已开启)) {
                        if (themedBox) container += string(" ") + Level(ClassName) + (opened ? " 已开启" : "");
                        label(container + "  " + to_string(对象信息.敌人信息.距离) + "m");
                    }
                }
                const int materialId = 读写.getDword(对象地址.敌人地址 + V139::物资ID字段);
                string name = getBoxName(materialId);
                if (configType == 2) {
                    const auto custom = customMaterialMap.find(ClassName);
                    if (custom != customMaterialMap.end()) name = custom->second;
                }
                if (name != "Error") label(name + "  " + to_string(对象信息.敌人信息.距离) + "m");
                if (按钮.盒子物资 && deadBox && std::hypot(PX-r_x, PY-r_y) < 50.0f) {
                    const int count = 读写.getDword(对象地址.敌人地址 + V139::盒内物资 + 8);
                    const auto array = 读写.getPtr64(对象地址.敌人地址 + V139::盒内物资);
                    if (array && count > 0 && count < 1000) {
                        int row = 0;
                        for (int item = 0; item < count && row < 12; ++item) {
                            const auto address = array + V139::物资结构步长 * item;
                            string itemName = GetItems(读写.getDword(address + V139::物资_ID));
                            if (itemName == "NULL") continue;
                            itemName += " ×" + to_string(std::max(0, 读写.getDword(address + V139::物资_数量)));
                            label(itemName, -28.0f * ++row);
                        }
                    }
                }
            }

        }

        if (draw_only_actor::ExcludedClass(ClassName)) {
            g_characterHistory.erase(actor); ++g_readDiagnostics.filtered; continue;
        }
        const bool isBoss = 骨骼->isBoss(ClassName);
        const auto metadata=draw_only_actor::ReadSnapshot(actor,read,
            draw_only_actor::KnownCharacterClass(ClassName,isBoss) || history->Confirmed(now));
        const auto identification=draw_only_actor::Identify(ClassName,metadata,isBoss);
        if (identification==draw_only_actor::Identification::Excluded ||
            draw_only_actor::ShouldSkip(metadata,filters)) {
            g_characterHistory.erase(actor); ++g_readDiagnostics.filtered; continue;
        }
        if (identification==draw_only_actor::Identification::Character) history->Confirm(now);
        else if (!history->Confirmed(now)) { ++g_readDiagnostics.unknownTypes; continue; }
        if (history==&localHistory) {
            history=&g_characterHistory.emplace(actor,localHistory).first->second;
        }
        ++g_readDiagnostics.characters;
        auto& enemy = 对象信息.敌人信息;
        enemy.队伍 = metadata.teamKnown ? metadata.team : -1;
        enemy.isboot = metadata.botKnown && metadata.bot;
        enemy.状态 = metadata.stateKnown ? metadata.state : 0;
        enemy.当前血量 = metadata.currentHealthKnown ? metadata.currentHealth : 0;
        enemy.最大血量 = metadata.maxHealthKnown ? metadata.maxHealth : 0;
        if (enemy.isboot) ++绘制人机; else ++绘制真人;
        enemy.手持 = 读写.getDword(读写.getPtr64(对象地址.敌人地址 + V139::手持武器页) + V139::武器ID);
        enemy.子弹数量 = 读写.getDword(读写.getPtr64(读写.getPtr64(对象地址.敌人地址 + V139::武器_子弹链) + V139::武器_子弹链2) + V139::武器_子弹数);
        enemy.子弹最大数量 = 读写.getDword(读写.getPtr64(读写.getPtr64(对象地址.敌人地址 + V139::武器_子弹链) + V139::武器_子弹链2) + V139::武器_子弹数Max);
        const uintptr_t mesh = metadata.mesh;
        const int Bonecount = metadata.boneCountKnown ? metadata.boneCount : 0;
        const uintptr_t bones = metadata.bones;
        // An unavailable animation buffer only removes unavailable joints;
        // names, health, radar and the root-based box still belong to the actor.
        if (draw_only_actor::HasSkeleton(metadata)) {
            骨骼->更新骨骼数据(mesh + V139::骨骼组件, bones + 0x30, enemy.骨骼坐标, Bonecount,
                                 enemy.队伍, ClassName, enemy.坐标);
        }
        char name[64]{};
        读写.getUTF8(name, 读写.getPtr64(对象地址.敌人地址 + V139::玩家名字));
        enemy.名字 = name;
        enemy.分区.clear();
        bool 是否掐雷 = false;
      if (按钮.手雷预警) {
        // sprintf(计算地址, "%lx",对象地址.敌人地址);
        if (对象信息.敌人信息.手持 == 602004 or 对象信息.敌人信息.手持 == 9825004) {
          std::string 状态字符串 = std::to_string(对象信息.敌人信息.状态);
          if (状态字符串.length() == 5) {
            计时器.addTimer(计算地址, 0);
            是否掐雷 = true;
          } else {
            if (计时器.hasTimer(计算地址)) {
              string 手雷递送 = 手雷类.calculateKey(对象信息.敌人信息.坐标.X, 对象信息.敌人信息.坐标.Y);
              if (手雷递送 == "") {
                Antitankgrenade[计算地址] = Antitankgrenade[计算地址] + 1;
                if (Antitankgrenade[计算地址] == 15) {
                  计时器.removeTimer(计算地址);
                  Antitankgrenade.erase(计算地址); // 没用的东西赶紧滚
                }
              } else {
                计时器.renameTimer(计算地址, 手雷递送);
                手雷类.remove(手雷递送);
              }
            }
          }
        } else {
          if (计时器.hasTimer(计算地址)) {
            string 手雷递送 = 手雷类.calculateKey(对象信息.敌人信息.坐标.X, 对象信息.敌人信息.坐标.Y);
            if (手雷递送 == "") {
              Antitankgrenade[计算地址] = Antitankgrenade[计算地址] + 1;
              if (Antitankgrenade[计算地址] == 15) {
                计时器.removeTimer(计算地址);
                Antitankgrenade.erase(计算地址); // 没用的东西赶紧滚
              }
            } else {
              计时器.renameTimer(计算地址, 手雷递送);
              手雷类.remove(手雷递送);
            }
          }
        }
      }

        bool isVisible[15];
        // Missing joints are not exposure samples. With no valid torso joint,
        // retain the actor marker and trace its root instead of the zero sentinel.
        const bool torsoVisible = draw_only_projection::ResolveVisibility(
            enemy.骨骼坐标, enemy.坐标, 按钮.掩体识别 && g_cameraReady && ValidCoordinate(PovLocation), isVisible,
            [&](const D3DVector& point) {
                return CustomLineTrace(Vector3(PovLocation.X, PovLocation.Y, PovLocation.Z),
                    Vector3(point.X, point.Y, point.Z));
            });
        if (按钮.雷达 && enemy.距离 <= 300) {
            enemy.雷达 = 计算.rotateCoord(自身数据.准星Y,
                (自身数据.坐标.X - enemy.坐标.X) / 200.0f * 按钮.雷达缩放,
                (自身数据.坐标.Y - enemy.坐标.Y) / 200.0f * 按钮.雷达缩放);
            const float length = std::hypot(enemy.雷达.X, enemy.雷达.Y);
            const float scale = length > 142.0f ? 142.0f / length : 1.0f;
            const ImVec2 position(按钮.雷达X + enemy.雷达.X * scale, 按钮.雷达Y + enemy.雷达.Y * scale);
            auto* list = ImGui::GetForegroundDrawList();
            const auto palette = draw_only_visuals::PaletteColors();
            list->AddCircleFilled(position, enemy.isboot ? 3.0f : 4.2f, draw_only_visuals::Color(torsoVisible, enemy.isboot), 16);
            list->AddCircle(position, enemy.isboot ? 3.0f : 4.2f, palette.border, 16, .75f);
        }
        骨骼数据 screenBones = 计算.计算骨骼(自身数据.矩阵, enemy.骨骼坐标, PX, PY);
        draw_only_projection::ProjectActorBounds(自身数据.矩阵,enemy.坐标,PX,PY,t_屏幕坐标);
        绘图.初始化坐标(t_屏幕坐标, screenBones);
        if (按钮.背敌预警) {
            OffScreen(ImGui::GetForegroundDrawList(), {r_x, r_y, r_y-r_z, (r_y-r_z)*.5f}, camear_r,
                draw_only_visuals::Color(torsoVisible, enemy.isboot),
                250.0f, enemy.距离);
        }
        if (camear_r <= .01f) continue;
        ++g_readDiagnostics.drawn;
        if (按钮.方框) 绘图.绘制方框(enemy.isboot, torsoVisible);
        if (按钮.射线) 绘图.绘制射线(torsoVisible, screenBones);
        if (按钮.距离) 绘图.绘制距离(enemy.距离, enemy.队伍);
        if (按钮.血量 && draw_only_actor::HasHealthBar(metadata))
            绘图.绘制血量(enemy.最大血量, enemy.当前血量, enemy.isboot, torsoVisible);
        if (按钮.名字) 绘图.绘制名字(enemy.分区, enemy.名字, enemy.isboot,
            7.0f - 计时器.elapsed(计算地址), 是否掐雷, enemy.队伍, Bonecount, torsoVisible);
        if (按钮.显示头部骨骼点) 绘图.绘制头部骨骼点(screenBones.Head, enemy.isboot, enemy.队伍, enemy.距离, isVisible[0]);
        if (按钮.骨骼) 绘图.绘制骨骼(screenBones, t_屏幕坐标, isVisible, enemy.距离, Bonecount);
        if (按钮.手持) 绘图.绘制手持(enemy.手持, enemy.状态, enemy.子弹数量, enemy.子弹最大数量);
    }
    if (按钮.人数 || 按钮.绘制全图) 绘图.绘制人数(绘制人机, 绘制真人, 地址.自身地址);
}

void 绘制::运行绘制() {
    ++g_readDiagnostics.frames;
    const auto frameStart=std::chrono::steady_clock::now();
    更新地址数据();
    const auto viewDone=std::chrono::steady_clock::now();
    更新对象数据();
    const auto objectsDone=std::chrono::steady_clock::now();
    g_readDiagnostics.maxViewMs=std::max(g_readDiagnostics.maxViewMs,
        std::chrono::duration<double,std::milli>(viewDone-frameStart).count());
    g_readDiagnostics.maxObjectsMs=std::max(g_readDiagnostics.maxObjectsMs,
        std::chrono::duration<double,std::milli>(objectsDone-viewDone).count());
    if (按钮.迫击炮落点 && g_drawFrameReady) {
        const double now = ImGui::GetTime();
        if (!g_mortarBss && now >= g_nextMortarBssProbe) {
            g_mortarBss = 读写.get_module_bss("libUE4.so");
            g_nextMortarBssProbe = now + 2.0;
        }
        // This local sample is rebuilt once per frame; failure never reuses a previous marker.
        draw_only_mortar::Sample sample{};
        const auto read = [&](uintptr_t address, void* output, size_t size) {
            return 读写.readv(address, output, size);
        };
        if (draw_only_mortar::ReadFrame(true, g_drawFrameReady, g_mortarBss,
                自身数据.矩阵, 自身数据.坐标, PX, PY, sample, read))
            draw_only_mortar::Render(ImGui::GetBackgroundDrawList(), sample);
    }
    if (按钮.手雷预警) 计时器.checkAndRemoveTimers();
    LogReadDiagnostics(ImGui::GetTime());
}

const char *绘制::Level(char *name) {
  if (strstr(name, "Lv1") != 0) {
    return "Lv1";
  } else if (strstr(name, "Lv2") != 0) {
    return "Lv2";
  } else if (strstr(name, "Lv3") != 0) {
    return "Lv3";
  } else if (strstr(name, "Lv4") != 0) {
    return "Lv4";
  } else if (strstr(name, "Lv5") != 0) {
    return "Lv5";
  } else if (strstr(name, "Lv6") != 0) {
    return "Lv6";
  } else if (strstr(name, "Lv7") != 0) {
    return "Lv7";
  }
  return "未知";
}

const char *绘制::getMaterialName(char *name) {
  static char dealname[32];
  memset(dealname, '\0', sizeof(dealname));
  if (strstr(name, "VH_MotorcycleCart") != 0) {
    strcpy(dealname, "三轮摩托车");
    按钮.VehicleID = 43;
  } else if (strstr(name, "VH_Scooter") != 0) {
    strcpy(dealname, "小绵羊车");
    按钮.VehicleID = 45;
  } else if (strstr(name, "VH_Motorcycle") != 0) {
    strcpy(dealname, "摩托车");
    按钮.VehicleID = 44;
  } else if (strstr(name, "VH_Tuk") != 0) {
    strcpy(dealname, "三轮摩托车");
    按钮.VehicleID = 48;
  } else if (strstr(name, "Buggy") != 0) {
    strcpy(dealname, "蹦蹦");
    按钮.VehicleID = 30;
  } else if (strstr(name, "Mirado") != 0) {
    strcpy(dealname, "敞篷跑车");
    按钮.VehicleID = 35;
  } else if (strstr(name, "CoupeRB") != 0) {
    strcpy(dealname, "跑车");
    按钮.VehicleID = 39;
  } else if (strstr(name, "Dacia") != 0) {
    strcpy(dealname, "轿车");
    按钮.VehicleID = 31;
  } else if (strstr(name, "PickUp_02_C") != 0) {
    strcpy(dealname, "皮卡");
    按钮.VehicleID = 33;
  } else if (strstr(name, "Rony") != 0) {
    strcpy(dealname, "皮卡");
    按钮.VehicleID = 38;
  } else if (strstr(name, "_StationWagon_C") != 0) {
    strcpy(dealname, "旅行车");
    按钮.VehicleID = 40;
  } else if (strstr(name, "UAZ") != 0) {
    strcpy(dealname, "吉普");
    按钮.VehicleID = 32;
  } else if (strstr(name, "PG117") != 0) {
    strcpy(dealname, "快艇");
    按钮.VehicleID = 49;
  } else if (strstr(name, "AquaRail") != 0) {
    strcpy(dealname, "冲锋艇");
    按钮.VehicleID = 50;
  } else if (strstr(name, "MiniBus") != 0) {
    strcpy(dealname, "面包车");
    按钮.VehicleID = 37;
  } else if (strstr(name, "VH_BRDM") != 0) {
    strcpy(dealname, "两栖装甲车");
    按钮.VehicleID = 0;
  } else if (strstr(name, "LadaNiva") != 0) {
    strcpy(dealname, "雪地越野车");
    按钮.VehicleID = 36;
  } else if (strstr(name, "_4SportCar_C") != 0) {
    strcpy(dealname, "敞篷跑车");
    按钮.VehicleID = 41;
  } else if (strstr(name, "_Bigfoot_C") != 0) {
    strcpy(dealname, "大脚车");
    按钮.VehicleID = 53;
  } else if (strstr(name, "_Training_C") != 0) {
    strcpy(dealname, "自行车");
    按钮.VehicleID = 0;
  } else if (strstr(name, "ATV1_C") != 0) {
    strcpy(dealname, "越野摩托车");
    按钮.VehicleID = 0;
  } else if (strstr(name, "_Horse") != 0) {
    strcpy(dealname, "马");
    按钮.VehicleID = 52;
  } else if (strstr(name, "_02_C") != 0) {
    strcpy(dealname, "雪地摩托车");
    按钮.VehicleID = 46;
  } else if (strstr(name, "Snowmobile_C") != 0) {
    strcpy(dealname, "雪地摩托车");
    按钮.VehicleID = 47;
  } else if (strstr(name, "_Motorglider_C") != 0) {
    strcpy(dealname, "滑翔机");
    按钮.VehicleID = 51;
  } else if (strstr(name, "_DesertCar_C") != 0) {
    strcpy(dealname, "大脚越野车");
    按钮.VehicleID = 0;
  } else if (strstr(name, "_New_C") != 0) {
    strcpy(dealname, "赛车");
    按钮.VehicleID = 0;
  } else {
    strcpy(dealname, "Error");
  }
  return dealname;
}

string 绘制::getBoxName(int id) {
  if (按钮.显示药品 && id == 601006)
    return "药品·医疗箱";
  if (按钮.显示药品 && id == 601005)
    return "药品·急救包";
  if (按钮.显示药品 && id == 601004)
    return "药品·绷带";
  if (按钮.显示药品 && id == 601001)
    return "药品·可乐";
  if (按钮.显示药品 && id == 601002)
    return "药品·肾上腺素";
  if (按钮.显示药品 && id == 601003)
    return "药品·止痛药";
if (按钮.显示药品) {
    if (id == 604014 || id == 604086)
      return "救助·自救器";
  }
  if (按钮.显示防具 && id == 503003)
    return "防具·三级防弹衣";
  if (按钮.显示防具 && id == 502003)
    return "防具·三级头盔";
  if (按钮.显示防具 && id == 501003)
    return "背包·三级包";

  if (按钮.其他物资) {
    if (id == 108004)
      return "近战·平底锅";
    if (id == 108003)
      return "近战·镰刀";
    if (id == 108002)
      return "近战·撬棍";
    if (id == 108001)
      return "近战·大砍刀";
    if (id == 107010)
      return "防具·突击盾牌";
    if (id == 603001)
      return "汽油·汽油";
    if (id == 403990)
      return "衣服·吉利服";
    if (id == 403187)
      return "衣服·吉利服";
    if (id == 106005)
      return "武器·R45";
    if (id == 106010)
      return "武器·沙漠之鹰";
    if (id == 106011)
      return "武器·TMP-9";
    if (id == 106008)
      return "武器·蝎式手枪";
    if (id == 106004)
      return "武器·P18C";
    if (id == 106002)
      return "武器·R1911";
    if (id == 106001)
      return "武器·P92";
  }
  if (按钮.狙击枪械) {
    if (id == 107001)
      return "武器·十字弩";
    if (id == 103003)
      return "武器·AWM";
    if (id == 103001)
      return "武器·Kar98K";
    if (id == 103002)
      return "武器·M24";
    if (id == 103006)
      return "武器·Mini14";
    if (id == 103008)
      return "武器·Win94";
    if (id == 103009)
      return "武器·SLR";
    if (id == 103012)
      return "武器·AMR狙击枪";
    if (id == 103013)
      return "武器·M417";
    if (id == 103014)
      return "武器·MK20-H";
    if (id == 103100)
      return "武器·MK12";
    if (id == 103010)
      return "武器·QBU";
    if (id == 103004)
      return "武器·SKS";
    if (id == 103007)
      return "武器·MK14";
    if (id == 103015)
      return "武器·M200";
    if (id == 103011)
      return "武器·莫辛狙击枪";
  }
  if (按钮.显示步枪) {
    if (id == 101012)
      return "武器·蜜獾";
    if (id == 101008)
      return "武器·M762";
    if (id == 101011)
      return "武器·VC-VAL突击步枪";
    if (id == 101010)
      return "武器·G36C步枪";
    if (id == 101013)
      return "武器·Famas突击步枪";
    if (id == 105002)
      return "武器·DP28";
    if (id == 101005)
      return "武器·狗砸";
    if (id == 101001)
      return "武器·AKM";
    if (id == 101007)
      return "武器·QBZ";
    if (id == 101009)
      return "武器·MK47";
    if (id == 101006)
      return "武器·AUG";
    if (id == 101002)
      return "武器·M16A4";
    if (id == 101003)
      return "武器·SCAR";
    if (id == 101004)
      return "武器·M416";
    if (id == 105001)
      return "武器·M249";
    if (id == 105012)
      return "武器·PKM";
    if (id == 105010)
      return "武器·MG3";
  }
  if (按钮.冲锋枪械) {
    if (id == 102001)
      return "武器·UZI";
    if (id == 102004)
      return "武器·汤姆逊冲锋枪";
    if (id == 102003)
      return "武器·维克托";
    if (id == 102002)
      return "武器·UMP9";
    if (id == 103005)
      return "武器·VSS射手步枪";
    if (id == 102005)
      return "武器·野牛冲锋枪";
    if (id == 102007)
      return "武器·MP5K";
    if (id == 102105)
      return "武器·P90冲锋枪";
  }
  if (按钮.散弹枪械) {
    if (id == 106003)
      return "武器·R1895";
    if (id == 106006)
      return "武器·短管散弹枪";
    if (id == 104003)
      return "武器·S12K";
    if (id == 104002)
      return "武器·S1897";
    if (id == 104001)
      return "武器·双管猎枪";
    if (id == 104004)
      return "武器·DBS";
    if (id == 104100)
      return "武器·SPAS-12";
    if (id == 104005)
      return "武器·AA12-G";
  }

  if (按钮.显示子弹 && id == 302001)
    return "子弹·7.62MM";
  if (按钮.显示子弹 && id == 303001)
    return "子弹·5.56MM";
  if (按钮.显示子弹 && id == 301001)
    return "子弹·9MM";
  if (按钮.显示子弹 && id == 304001)
    return "子弹·12口径";
  if (按钮.显示子弹 && id == 305001)
    return "子弹·45口径";
  if (按钮.显示子弹 && id == 308001)
    return "子弹·信号弹";
  if (按钮.显示子弹 && id == 307001)
    return "子弹·箭矢";

  if (按钮.显示倍镜) {
    if (id == 203001)
      return "[倍镜]红点";
    if (id == 203014)
      return "[倍镜]3倍镜";
    if (id == 203015)
      return "[倍镜]6倍镜";
    if (id == 203005)
      return "[倍镜]8倍镜";
  }

  if (按钮.显示配件 && (id == 204014 || id == 204010))
    return "配件·子弹袋";
  if (按钮.显示配件 && id == 205004)
    return "配件·箭袋";
  if (按钮.显示配件 && id == 202007)
    return "配件·激光瞄准器";
  if (按钮.显示配件 && id == 202004)
    return "配件·轻型握把";
  if (按钮.显示配件 && id == 202005)
    return "配件·半截握把";
  if (按钮.显示配件 && id == 205001)
    return "配件·UZI枪托";
  if (按钮.显示配件 && id == 205003)
    return "配件·狙击枪托";
  if (按钮.显示配件 && id == 205002)
    return "配件·步枪枪托";
  if (按钮.显示配件 && id == 201003)
    return "配件·狙击枪补偿器";
  if (按钮.显示配件 && id == 201005)
    return "配件·狙击枪消焰器";
  if (按钮.显示配件 && id == 201007)
    return "配件·狙击枪消音器";
  if (按钮.显示配件 && id == 201011)
    return "配件·步枪消音器";
  if (按钮.显示配件 && id == 201009)
    return "配件·步枪补偿器";
  if (按钮.显示配件 && id == 201010)
    return "配件·步枪消焰器";
  if (按钮.显示配件 && id == 201006)
    return "配件·冲锋枪消音器";
  if (按钮.显示配件 && (id == 201004 || id == 201002))
    return "配件·冲锋枪消焰器";
  if (按钮.显示配件 && id == 202006)
    return "配件·拇指握把";
  if (按钮.显示配件 && id == 202002)
    return "配件·垂直握把";
  if (按钮.显示配件 && id == 202001)
    return "配件·直角握把";
  if (按钮.显示配件 && id == 205011)
    return "配件·撞火枪托";
  if (按钮.显示配件 && id == 204017)
    return "配件·霰弹快速";
  if (按钮.显示配件 && id == 201012)
    return "配件·鸭嘴枪口";
  if (按钮.显示配件 && id == 201001)
    return "配件·霰弹收束";

  if (按钮.显示配件) {
    if (id == 204009)
      return "配件·狙击枪快扩";
    if (id == 204007)
      return "配件·狙击枪扩容";
    if (id == 204008)
      return "配件·狙击枪快速";
    if (id == 204013)
      return "配件·步枪快扩";
    if (id == 204011)
      return "配件·步枪扩容";
    if (id == 204012)
      return "配件·步枪快速";
    if (id == 204006)
      return "配件·冲锋枪快扩";
    if (id == 204004)
      return "配件·冲锋枪扩容";
    if (id == 204005)
      return "配件·冲锋枪快速";
    if (id == 204003)
      return "配件·手枪快扩";
    if (id == 204002)
      return "配件·手枪快速";
    if (id == 204001)
      return "配件·手枪扩容";
  }
  if (按钮.投掷物品) {
    if (id == 602003)
      return "投掷·燃烧瓶";
    if (id == 602002)
      return "投掷·烟雾弹";
    if (id == 602001)
      return "投掷·震爆弹";
    if (id == 602004)
      return "投掷·手榴弹";
  }
  return "Error";
}

#pragma once
// Local display model for native UI tests. No game or driver is accessed.
struct DemoSettings {
    bool PhysX画线=false;
    bool 人数=true;
    bool 其他物资=false;
    bool 冲锋枪械=false;
    bool 名字=true;
    bool 射线=false;
    float 射线粗细=1.2f;
    float 当前帧率=90.f;
    bool 忽略人机=false;
    bool 忽略倒地=false;
    bool 手持=true;
    bool 手雷预警=false;
    bool 投掷物品=false;
    bool 掩体识别=false;
    bool 散弹枪械=false;
    bool 方框=true;
    float 方框粗细=1.2f;
    bool 显示倍镜=false;
    bool 显示头部骨骼点=false;
    bool 显示子弹=false;
    bool 显示步枪=false;
    bool 显示药品=false;
    bool 显示配件=false;
    bool 显示防具=false;
    bool 狙击枪械=false;
    bool 盒子=false;
    bool 绘制=true;
    bool 绘制信号枪=false;
    bool 绘制全图=false;
    bool 绘制宝箱=false;
    bool 绘制武器箱=false;
    bool 绘制药箱=false;
    bool 背敌预警=false;
    bool 血量=true;
    bool 超级物资箱=false;
    bool 距离=true;
    bool 车辆=false;
    bool 载具油量=false;
    bool 载具血量=false;
    bool 迫击炮落点=false;
    bool 隐藏已开启=false;
    bool 雷达=false;
    float 雷达X=1.2f;
    float 雷达Y=1.2f;
    float 雷达缩放=1.2f;
    bool 骨骼=true;
    float 骨骼粗细=1.2f;
};
struct DemoModel {
    DemoSettings 按钮;
    float 骨骼距离限制=300.f;
    int saveCalls=0,loadCalls=0;
    std::string lastProfile;
    void 保存配置(const char* name){++saveCalls;lastProfile=name;}
    void 读取配置(const char* name){++loadCalls;lastProfile=name;}
} 绘制;
static bool 模型初始化=false;
static std::string g_brand="Windows",g_model="Drawing UI Preview",g_kernel="Standalone renderer";
static struct {int width=1600,height=1000;} displayInfo;
static void GetBase(){spatial::Notice("交互预览：初始化按钮响应正常");}
static void LoadDeviceInfo(){}
static void ModernStyleToggle(const char* label,bool* value){spatial::Toggle(label,value);}

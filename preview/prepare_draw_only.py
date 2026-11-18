from pathlib import Path
import re

root=Path(__file__).resolve().parent.parent
project=next(root/name for name in ('app-draw-final','app-draw-clean','app') if (root/name).is_dir())
src=project/'jni/src'
pages=(src/'SpatialLegacyPages.inl').read_text('utf-8-sig')
booleans=set(re.findall(r'Toggle\("[^"]*",\s*&绘制\.按钮\.(\w+)\)',pages))
fields=set(re.findall(r'绘制\.按钮\.(\w+)',pages))
out='''#pragma once
// Local display model for native UI tests. No game or driver is accessed.
struct DemoSettings {
'''
for name in sorted(fields):
    value='true' if name in {'方框','骨骼','名字','血量','距离','手持','绘制','人数'} else 'false'
    if name in booleans:
        out+=f'    bool {name}={value};\n'
    else:
        value='90.f' if name=='当前帧率' else '1.2f'
        out+=f'    float {name}={value};\n'
out+='''};
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
'''
(root/'preview/demo_bindings.h').write_text(out,'utf-8')
print('Drawing-only model: '+str(len(fields))+' settings')

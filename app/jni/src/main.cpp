#include "辅助类.h"
#include "图片调用.h"

int abs_ScreenX = 0, abs_ScreenY = 0;
布局 布局;
绘制 绘制;

int main() {
    spatial_console::SessionIntro();
    if (布局.初始化程序() != 0) {
        spatial_console::Message(spatial_console::Terminal(), "启动已结束", "显示环境未就绪，已关闭启动流程。");
        return 1;
    }
    加载内存图片();
    spatial_console::Ready();
    布局.开启悬浮窗();
    return 0;
}

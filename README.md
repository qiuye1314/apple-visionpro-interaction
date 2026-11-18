# lsrael 1.39 绘制专版 · 液体玻璃

> **Android 绘制专版 · 液体玻璃界面 · ImGui + Embree/PhysX · ARM64**
>
> 从 1.39 源码独立整理的绘制版本：保留人物、物资、载具、雷达与预警；**追踪、自瞄、断点及内存写回链路已移除**。
> 界面全面改为液体玻璃材质——双层玻璃边框、主题柔光、立体 S 标记、分层导航卡片，七套主题同步切换。
>
> 协议：**GPL-3.0**

---

## 🎯 本次版本要点

| 项 | 内容 |
|---------|------|
| 版本性质 | **绘制专版**，从 1.39 源码独立整理，非主版本功能继承 |
| 已移除 | 追踪、自瞄、断点、游戏内存写回链路 |
| 已保留 | 人物、物资、载具、雷达、预警、人物掩体漏打（默认关闭） |
| 界面 | 主页 / 绘制 / 物资 / 设置 四页，手指上下滚动 + 独立退出 |
| 主题 | 七套全局主题，配色贯穿所有玻璃面板 |
| 分辨率 | 主页、绘制、物资、设置按可用视口自适应；窄屏导航与内容分别适配 |
| 悬浮窗 | 默认宽高缩至原自适应默认值的 **90%**，可在设置中调节或双指缩放 |

---

## 🧊 液体玻璃界面

| 项 | 说明 |
|---|---|
| 边框 | 悬浮窗升级为**双层玻璃边框** + 主题柔光 + 立体 S 标记 + 分层导航卡片 |
| 表面 | 更通透的液体玻璃，面板可透出自定义图片与天气背景 |
| 统一材质 | 主页文案、功能卡片、主题选择、背景设置、数值输入、通知条**共用同一套填充/边缘反光/主题底色** |
| 弹窗 | 去除额外厚底色，按钮底色同步透明；关闭背景图片后仍保持玻璃表面 |
| 深色主题 | 玻璃底色改为**主题浅色低透明度色泽**，不再使用黑色面板填充 |
| 去除项 | 文案专属曲线、按钮额外阴影/渐变、背景窗口黑色模态遮罩、侧栏重复填充 |
| 文字可读性 | 文字、图标、开关状态保留清晰对比，七套主题继续同步切换 |
| 动效 | 「减少动态效果」开启后光效静止；**陀螺仪仍需手动单独开启** |

---

## 🎨 背景空间

| 场景 | 说明 |
|---|---|
| 内置图片 | 星眸、泡影、蓝蝶三张原图**已嵌入可执行文件**，无需额外复制 |
| 天气场景 | 轻雪、细雨、星野、流萤、极光、纯净、自定义 |
| 图片处理 | 按原比例居中铺满面板，**不拉伸**；比例不同时裁去超出部分 |
| 轮换 | 仅使用勾选的内置场景，间隔可调；自定义图片/渐变使用固定模式 |
| 自定义图 | 支持 PNG/JPEG 与中文路径，**最大 32 MB、最长边 8192 像素**，加载后缩放至 2048 |
| 暂停 | 收起悬浮窗或开启「减少动态效果」后背景动画暂停 |
| 脚本 | 原图与生成脚本位于 `app/jni/assets/backdrops`，改图后运行 `generate_presets.py` 再重新编译 |

---

## 👤 人物绘制与掩体判定

> 人物掩体漏打保留在「绘制 → 信息标记 → 人物掩体漏打」，**默认关闭**。
> 开启后使用 PhysX 场景 + **15 个骨骼**的遮挡判断：骨骼逐段区分可见/遮挡，头点按头部状态显示，
> 整体角框/姓名/血条/射线按头·胸·骨盆任一可见判断。缺失关节不参与遮挡射线；
> 躯干关节均不可用时，以实际人物位置判断整体标记遮挡。

### 本次修复（人物漏显 / 骨骼异常）

| 问题 | 修复方式 |
|---|---|
| 人物漏显 | 人物识别改为 **已知类名 or 骨骼·队伍·人机·血量组合校验**，不再依赖 `0x10BC` / `0x10C0` 浮点必须等于 479.5，避免速度槽差异误排除 |
| 骨骼不可用 | 仍显示人物基础信息，不再整人丢框 |
| 人机标志 | 按**单字节**读取；失败的血量、队伍、状态不作为确定过滤依据 |
| 对象列表 | 分块读取 + 前后数组头核对 + 去重；有效空列表立即生效 |
| 俯视/翻转视角 | 改用**三维体积投影**作为备用人物框，不再按旧高度公式整人拒画 |
| 骨骼传输失败 | 最多补读一次 |
| 缺失头/脚踝 | 用有效关节与人物根坐标回退，异常单点不再吞掉整个人物框 |
| 趴姿连线 | 按人物二维范围判断，不再仅按站立高度过滤 |
| 无实测依据的关节映射 | **不猜测替换**，取得有效映射前可能不画 |

---

## ⚙️ 驱动与读取链路

| 项 | 内容 |
|---|---|
| 驱动选项 | 只保留 **1 = TWT**、**2 = RT**（DEV / HK / QX 连接代码已移除） |
| TWT | 读取接口对接 `kernel.h` |
| RT | 沿用 socket + **ioctl 601** 读取协议 |
| 统一入口 | PhysX 场景与人物绘制**使用同一所选驱动的读取入口** |
| PhysX 实例偏移 | 更新为 `0x175E6D98` |
| 迫击炮落点 | 独立开关，默认关闭，可随绘制配置保存；坐标链 `libUE4.so:bss + 0x42DAA8 → 0x18 → 0x9C8 → 0xE24`（末级以 0xE24 为准，非截断的 362） |
| BSS 解析 | 走驱动查询或 RT 模块映射，**不与模块相对地址 0x17A14AA8 重复相加** |

> 上述坐标与 PhysX 偏移均来自用户提供数据，当前**只完成模拟读链与编译验证**。

---

## 🩺 稳定性

| 场景 | 处理 |
|---|---|
| 读取失败 / 无效变换 / 越界索引 / 镜头后方关节 | 全部隔离，保留真实组件平移与双脚高度 |
| 临时视图与坐标读取失败 | 最多回退 **120 毫秒**，回退不延长期限 |
| 身份变化 | 关卡、世界、自身、人物身份变化时清理对应旧数据 |
| PhysX 场景更新 | 与查询同步，命中网格保持有效引用 |
| 遮挡与线框查询 | **非阻塞**；忙碌时界面不等待，也**不把忙碌误当无遮挡** |
| 扫描过载 | 冷却后恢复探测；无效实例自动重试限频，手动初始化立即执行 |
| 场景校验 | 校验数组数量、地址范围、地形采样、三角形索引，失败数据不进入场景构建 |
| 地形布局 | 512×512 与 1024×1024 通过兼容检查 |
| 线框上限 | 每帧最多 4000 个三角形，异常投影不送入绘制 |
| 触摸设备异常 | 退避或退出，清理本设备按下状态并保留其它设备触点，槽号越界直接丢弃 |
| 退出流程 | 先停并等待触摸线程再关闭设备，镜像状态用非阻塞互斥 |
| 诊断日志 | 每 2 秒输出 `lsrael-draw`，含读取失败/回退/过滤/进入绘制人物数量/分段最大耗时 |

---

## 📁 目录结构

```
.
├── app/
│   ├── build-android.cmd                       构建入口（Windows）
│   └── jni/                     （468 个文件）  清理后的完整源码与静态依赖
│       ├── Android.mk / Application.mk
│       ├── assets/backdrops/                   三张内置背景原图 + generate_presets.py
│       ├── include/                            头文件
│       │   ├── ImGui/                          ImGui + FreeType
│       │   ├── embree3/                        Embree 3 光线追踪
│       │   ├── foundation/                     PhysX 基础库
│       │   └── Draw.h / ReadGame.h / auth_check.h …
│       ├── src/                                实现源码
│       │   ├── DrawOnlyVisuals.h               绘制视觉主逻辑
│       │   ├── DrawOnlyActorRead.h             人物读取
│       │   ├── DrawOnlyBoneReader.h            骨骼解析
│       │   ├── DrawOnlyDriver.h                驱动入口（TWT / RT）
│       │   ├── DrawOnlyMemoryBridge.h          内存桥
│       │   ├── DrawOnlyProjection.h            投影
│       │   ├── DrawOnlySceneBounds.h           场景包围
│       │   ├── DrawOnlyVisibleScene.h          可见场景
│       │   ├── DrawOnlyMortar.h / -Visuals.h   迫击炮落点
│       │   ├── DrawOnlyFrameStability.h        帧稳定性
│       │   ├── DrawOnlyPhysXThrottle.h         扫描限流
│       │   ├── SpatialBackdrop*.h              液体玻璃背景
│       │   ├── SpatialTheme.h / SpatialUI.h    主题与主页
│       │   ├── SpatialGyroAndroid.h            陀螺仪（Android）
│       │   ├── SpatialMotion.h                 动效
│       │   ├── SpatialVideo*.h                 视频纹理
│       │   ├── ReadGame.cpp                    读链
│       │   ├── PhysX.h                         场景与遮挡
│       │   ├── ImGui/TouchHelperA.cpp          多点触摸
│       │   └── main.cpp                        入口
│       └── lib/                                预编译静态库
│
├── build/                     （4 个）         构建产物
│   ├── lsrael_DrawOnly_1_39.sh                 ARM64 ELF 可执行文件（17.4 MB）
│   ├── build-info.json                         构建参数 / 输入校验 / 产物 SHA256
│   ├── android-build.log                       编译日志
│   └── elf-dynamic.txt                         ELF 动态段
│
├── preview/                   （168 png + 158 ppm）  截图与校验程序
│   ├── SpatialPreview.exe                      Windows 交互预览（14 MB）
│   ├── Verify*.exe  × 21                       各模块独立校验程序
│   ├── draw-only-home-theme-0..6.png           七主题 × 主页
│   ├── draw-only-drawing-theme-0..6.png        七主题 × 绘制页
│   ├── draw-only-entities-theme-0..6.png       七主题 × 人物
│   ├── draw-only-background-*.png              背景空间（场景/预设/自定义/竖屏）
│   ├── draw-only-gyro-*.png                    陀螺仪 3D / 拆解 / 关闭
│   ├── draw-only-home-quotes-0..2.png          主页文案三态
│   ├── draw-only-responsive-*.png · .ppm       多视口回归（含 4K 原始位图）
│   ├── draw-only-liquid-*.tga                  带透明通道的材质图
│   ├── capture-manifest.json                   截图尺寸与源图校验值
│   ├── responsive-matrix.json                  本次通过的视口/交互检查矩阵
│   └── verify.ps1                              可复现桌面检查
│
├── audit/liquid-glass.json                    界面/骨骼/双驱动/落点/PhysX 差异与构建测试记录
├── audit_liquid.py                             重查源码、构建输入、测试日志与截图校验值
├── build_android.py                            自动复制到 ASCII 临时目录全量编译
├── package_release.py                          打包脚本
├── transfer_liquid.py                          手机端推送脚本
├── 文件校验.json                                890 条全量文件校验（bytes + sha256）
├── 使用说明.txt                                 原作者使用说明
├── LICENSE                                     GPL-3.0
└── README.md
```

> `preview/` 下同名 `.ppm` 与 `.png` 成对存在：`.ppm` 为**无压缩原始位图**，用于像素级比对，
> 体积占比约 80%（996 MB），GitHub 网页无法直接预览，需要时用本地图像工具打开。

---

## 🖼 界面预览

主页（液体玻璃 + 随机文案）：

![主页](preview/draw-only-home-quotes-0.png)

| 总览 | 拆解视图 |
|---|---|
| ![绘制](preview/draw-only-drawing-theme-0.png) | ![拆解](preview/draw-only-exploded.png) |

**七套主题**

| 主题 | 主页 | 绘制页 | 人物 |
|:--:|:--:|:--:|:--:|
| 0 | ![h0](preview/draw-only-home-theme-0.png) | ![d0](preview/draw-only-drawing-theme-0.png) | ![e0](preview/draw-only-entities-theme-0.png) |
| 1 | ![h1](preview/draw-only-home-theme-1.png) | ![d1](preview/draw-only-drawing-theme-1.png) | ![e1](preview/draw-only-entities-theme-1.png) |
| 2 | ![h2](preview/draw-only-home-theme-2.png) | ![d2](preview/draw-only-drawing-theme-2.png) | ![e2](preview/draw-only-entities-theme-2.png) |
| 3 | ![h3](preview/draw-only-home-theme-3.png) | ![d3](preview/draw-only-drawing-theme-3.png) | ![e3](preview/draw-only-entities-theme-3.png) |
| 4 | ![h4](preview/draw-only-home-theme-4.png) | ![d4](preview/draw-only-drawing-theme-4.png) | ![e4](preview/draw-only-entities-theme-4.png) |
| 5 | ![h5](preview/draw-only-home-theme-5.png) | ![d5](preview/draw-only-drawing-theme-5.png) | ![e5](preview/draw-only-entities-theme-5.png) |
| 6 | ![h6](preview/draw-only-home-theme-6.png) | ![d6](preview/draw-only-drawing-theme-6.png) | ![e6](preview/draw-only-entities-theme-6.png) |

**背景空间**

| 总览 | 自定义 | 竖屏 |
|---|---|---|
| ![bg](preview/draw-only-background-overview.png) | ![custom](preview/draw-only-background-custom.png) | ![portrait](preview/draw-only-background-portrait.png) |

| 场景 0 | 场景 2 | 场景 4 |
|---|---|---|
| ![s0](preview/draw-only-background-scene-0.png) | ![s2](preview/draw-only-background-scene-2.png) | ![s4](preview/draw-only-background-scene-4.png) |

**陀螺仪 / 3D**

| 3D | 拆解 | 关闭 |
|---|---|---|
| ![g3d](preview/draw-only-gyro-3d.png) | ![gx](preview/draw-only-gyro-exploded.png) | ![goff](preview/draw-only-gyro-off.png) |

---

## 🛠 构建与复现

**Android 编译**（需要 Python 3 + NDK r26d，输出 `arm64-v8a` / `android-31`）：

```bash
python build_android.py --ndk D:/Android/android-ndk-r26d
# 或设置环境变量后
set ANDROID_NDK_HOME=D:/Android/android-ndk-r26d && python build_android.py
```

脚本会自动复制到 **ASCII 临时目录**全量编译（默认 `D:/work-build/lsrael-draw-only-139`），
可用 `--stage-root` 指定其他 ASCII 路径，新产物写入 `build` 目录。

**桌面检查**：

```powershell
powershell -ExecutionPolicy Bypass -File preview/verify.ps1
python audit_liquid.py
```

**手机推送**：

```bash
python transfer_liquid.py
```

---

## ⚠️ 使用注意

| 项 | 说明 |
|---|---|
| 文件后缀 | `lsrael_DrawOnly_1_39.sh` **实际是 ELF 可执行程序**，不是 shell 脚本 |
| 杀软告警 | 仓库含 21 个 Windows `.exe` 与 ARM64 ELF，部分环境会误报，可对照 `文件校验.json` |
| 配置落点 | 绘制 `/data/local/tmp/lsrael_draw_139`、界面 `…_ui.cfg`、背景 `…_ui.cfg.background` |
| 体积 | 解包约 **1.2 GB**，其中 `.ppm` 原始位图 996 MB |
| 内置图 | 三张 JPEG 已嵌入可执行文件，无需额外复制；改图需重编 |
| 第三方 | Embree、PhysX foundation、ImGui、FreeType、OpenSSL 遵循各自许可证 |
| 作者环境 | `build-info.json` 中的 `D:\工作区\…`、`D:\work-build\…` 为作者本机路径 |

---

## 📄 协议

本项目采用 **GPL-3.0**，见 [`LICENSE`](LICENSE)。第三方组件遵循其原始许可证。

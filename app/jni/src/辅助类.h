#ifndef 辅助类_H
#define 辅助类_H

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <limits.h>
#include <map>
#include <mutex>
#include <string>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <vector>
#include <unordered_map>
#include "SpatialConsole.h"
#include "DrawOnlyDriver.h"
#include "DrawOnlyMemoryBridge.h"
#include "ReadGame.h"
#include "./CPUaffinity/timer.h"

using namespace std;
#define PI 3.141592653589793238

// Drawing edition: this interface intentionally contains memory reads only.
class Kernel {
    std::atomic<pid_t> pid{-1};
    int fd = -1;

    static bool Numeric(const char* name) {
        if (!name || !*name) return false;
        for (const unsigned char* p = reinterpret_cast<const unsigned char*>(name); *p; ++p)
            if (*p < '0' || *p > '9') return false;
        return true;
    }

    static string LinkTarget(const string& path) {
        char value[PATH_MAX]{};
        const ssize_t count = readlink(path.c_str(), value, sizeof(value) - 1);
        if (count <= 0) return {};
        return string(value, static_cast<size_t>(count));
    }

    static int OpenTwt() {
        int descriptor = -1;
#if defined(__aarch64__)
        // Connection arguments from the supplied TWT header.
        ::syscall(__NR_reboot, 0x114514L, 0x1919810L, 0x2778L, &descriptor);
#endif
        if (descriptor >= 0) {
            const int flags = fcntl(descriptor, F_GETFD);
            if (flags >= 0) fcntl(descriptor, F_SETFD, flags | FD_CLOEXEC);
            return descriptor;
        }
        DIR* files = opendir("/proc/self/fd");
        if (!files) return -1;
        for (dirent* e = readdir(files); e; e = readdir(files)) {
            if (!Numeric(e->d_name)) continue;
            const string target = LinkTarget(string("/proc/self/fd/") + e->d_name);
            if (target.find("anon_inode:") == string::npos ||
                target.find("TwT_driver") == string::npos) continue;
            // Own a duplicate so destruction never closes a borrowed descriptor.
            descriptor = fcntl(atoi(e->d_name), F_DUPFD_CLOEXEC, 0);
            if (descriptor >= 0) break;
        }
        closedir(files);
        return descriptor;
    }

    static int DriverIoctl(int descriptor, unsigned long command,
                           draw_only_driver::Request* request) {
        return ioctl(descriptor, command, request);
    }

    uintptr_t ModuleFromMaps(const char* module, bool bss) const {
        const pid_t process = pid.load();
        if (!module || !*module || process <= 0) return 0;
        ifstream maps("/proc/" + to_string(process) + "/maps");
        return draw_only_driver::ModuleFromMaps(maps, module, bss);
    }

public:
    int 选择值 = 0;

    Kernel() {
        选择值 = spatial_console::ChooseDriver();
        if (选择值 == 0) std::exit(0);
        if (选择值 == 1) fd = OpenTwt();
        else if (选择值 == 2) fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
        if (fd < 0) {
            fprintf(stderr, "读取通道未连接，请确认选择的驱动已经刷入并正常运行。\n");
            std::exit(1);
        }
        const bool bound = draw_only_memory::Bind(this,
            [](void* context, uintptr_t address, void* output, size_t size) {
                return static_cast<Kernel*>(context)->readv(address, output, size);
            },
            [](void* context, const char* name) { return static_cast<Kernel*>(context)->getPID(name); },
            [](void* context, const char* name) { return static_cast<Kernel*>(context)->get_module_base(name); });
        if (!bound) {
            close(fd); fd = -1;
            fprintf(stderr, "绘制读取通道注册失败。\n");
            std::exit(1);
        }
        printf("[+] 绘制读取通道已连接\n");
    }

    ~Kernel() { draw_only_memory::Unbind(this); if (fd >= 0) close(fd); }
    Kernel(const Kernel&) = delete;
    Kernel& operator=(const Kernel&) = delete;

    void 初始化读写(pid_t process) { pid = process; }

    bool readv(uintptr_t addr, void* buffer, size_t size) {
        return draw_only_driver::Read(static_cast<draw_only_driver::Backend>(选择值),
                                      fd, pid.load(), addr, buffer, size, DriverIoctl);
    }

    template<class T> T read(uintptr_t addr) {
        T value{};
        readv(addr, &value, sizeof(value));
        return value;
    }
    template<class T> T Read(uintptr_t addr) { return read<T>(addr); }

    uintptr_t get_module_base(const char* module) {
        if (选择值 == 1) {
            const uintptr_t base = draw_only_driver::FindTwtModule(fd, pid.load(), module, false, DriverIoctl);
            if (base) return base;
        }
        return ModuleFromMaps(module, false);
    }

    uintptr_t get_module_bss(const char* module) {
        if (选择值 == 1) {
            const uintptr_t bss = draw_only_driver::FindTwtModule(fd, pid.load(), module, true, DriverIoctl);
            if (bss) return bss;
        }
        return ModuleFromMaps(module, true);
    }

    int getPID(const char* name) {
        if (!name || !*name) return -1;
        if (选择值 == 1) {
            const int result = draw_only_driver::FindTwtPid(fd, name, DriverIoctl);
            if (result > 0) return result;
        }
        DIR* proc = opendir("/proc");
        if (!proc) return -1;
        int result = -1;
        for (dirent* e = readdir(proc); e; e = readdir(proc)) {
            if (!Numeric(e->d_name)) continue;
            char command[256]{};
            const string filename = string("/proc/") + e->d_name + "/cmdline";
            FILE* stream = fopen(filename.c_str(), "rb");
            if (!stream) continue;
            const size_t count = fread(command, 1, sizeof(command) - 1, stream);
            fclose(stream);
            if (count && strcmp(command, name) == 0) { result = atoi(e->d_name); break; }
        }
        closedir(proc);
        return result;
    }

    float getFloat(uintptr_t addr) { return read<float>(addr); }
    int getDword(uintptr_t addr) { return read<int>(addr); }
    uintptr_t getPtr32(uintptr_t addr) { return read<uint32_t>(addr & 0xFFFFFFFFFFULL); }
    uintptr_t getPtr64(uintptr_t addr) { return read<uintptr_t>(addr); }
    char getByte(uintptr_t addr) { return read<char>(addr); }

    void getUTF8(char* output, unsigned long address) {
        if (!output) return;
        output[0] = '\0';
        uint16_t utf16[15]{};
        if (!readv(address, utf16, 28)) return;
        size_t length = 0;
        for (size_t i = 0; i < 14 && utf16[i] != 0; ++i) {
            uint32_t cp = utf16[i];
            if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < 14 && utf16[i + 1] >= 0xDC00 && utf16[i + 1] <= 0xDFFF) {
                cp = 0x10000 + ((cp - 0xD800) << 10) + (utf16[++i] - 0xDC00);
            } else if (cp >= 0xD800 && cp <= 0xDFFF) cp = 0xFFFD;
            const size_t bytes = cp <= 0x7F ? 1 : cp <= 0x7FF ? 2 : cp <= 0xFFFF ? 3 : 4;
            if (length + bytes >= 32) break;
            if (bytes == 1) output[length++] = static_cast<char>(cp);
            else {
                output[length++] = static_cast<char>((bytes == 2 ? 0xC0 : bytes == 3 ? 0xE0 : 0xF0) | (cp >> (6 * (bytes - 1))));
                for (int shift = static_cast<int>(bytes - 2) * 6; shift >= 0; shift -= 6)
                    output[length++] = static_cast<char>(0x80 | ((cp >> shift) & 0x3F));
            }
        }
        output[length] = '\0';
    }
};

#include "Draw.h"
#include "imgui_impl_opengl3.h"
#include "结构体.h"
#include "骨骼.hpp"
class StringFloatMap
{
private:
    map<string, vector<float>> data;

public:
    void add(string key, float value1, float value2)
    {
        vector<float> values;
        values.push_back(value1);
        values.push_back(value2);
        data[key] = values;
    }

    void remove(string key)
    {
        data.erase(key);
    }

    bool exists(string key)
    {
        return data.find(key) != data.end();
    }

    string calculateKey(float 坐标X, float 坐标Y)
    {
        int maps = 999;
        string 键名;
        for (const auto &pair : data)
        {
            const vector<float> &values = pair.second;
            int result = (int)sqrt(pow(坐标X - values[0], 2) + pow(坐标Y - values[1], 2)) * 0.01;
            // printf("测试值:%f \n",result);
            if (result < maps)
            {
                // return pair.first;
                maps = result;
                键名 = pair.first;
                // break;
            }
        }
        if (maps < 20)
        {
            return 键名;
        }
        return ""; // 如果没有满足条件的键名，则返回空字符串
    }
};

class Timer
{
    using Clock = std::chrono::steady_clock;
    std::map<std::string, Clock::time_point> timers;
public:
    void addTimer(const string& name, int) { timers.emplace(name, Clock::now()); }
    void updateTimers() {}
    void checkAndRemoveTimers() {
        const auto now = Clock::now();
        for (auto it = timers.begin(); it != timers.end();) {
            if (std::chrono::duration<float>(now - it->second).count() >= 12.0f) it = timers.erase(it);
            else ++it;
        }
    }
    float elapsed(const string& name) const {
        const auto it = timers.find(name);
        return it == timers.end() ? 0.0f : std::chrono::duration<float>(Clock::now() - it->second).count();
    }
    void removeTimer(const string& name) { timers.erase(name); }
    bool hasTimer(const string& name) const { return timers.find(name) != timers.end(); }
    void renameTimer(const string& from, const string& to) {
        const auto it = timers.find(from);
        if (it != timers.end()) { const auto start = it->second; timers.erase(it); timers[to] = start; }
    }
};


class 计算
{
public:
    int 计算距离(D3DVector 自身坐标, D3DVector 对方坐标);
    D4DVector 计算屏幕坐标(float 矩阵[16], D3DVector 人物坐标, float px, float py);
    D2DVector 计算屏幕坐标2(float 矩阵[16], D3DVector 人物坐标, float px, float py);
    float 计算屏幕距离(D2DVector &坐标, float px, float py);
    骨骼数据 计算骨骼(float (&矩阵)[16], D3DVector (&骨骼)[17], float px, float py);
    D2DVector rotateCoord(float angle, float objRadar_x, float objRadar_y);
    D2DVector rotateCoord(D3DVector Enemy, D3DVector RealPerson);
};
// 绘图
class 绘图
{
public:
    struct 颜色
    {
        ImColor 红色 = ImColor(255, 0, 0, 255);
        ImColor 白色 = ImColor(255, 255, 255, 255);
        ImColor 蓝色 = ImColor(0, 0, 255, 255);
        ImColor 绿色 = ImColor(0, 255, 0, 255);
        ImColor 黄色 = ImColor(255, 255, 0, 255);
        ImColor 黑色 = ImColor(0, 0, 0, 255);
    };

    struct VecTor2
    {
        float x;
        float y;
        VecTor2()
        {
            this->x = 0;
            this->y = 0;
        }
        VecTor2(float x, float y)
        {
            this->x = x;
            this->y = y;
        }
        bool operator!=(const VecTor2 &Pos)
        {
            if (this->x != Pos.x || this->y != Pos.y)
            {
                return true;
            }
            return false;
        }
        VecTor2 operator+(float v) const
        {
            return VecTor2(x + v, y + v);
        }
        VecTor2 operator-(float v) const
        {
            return VecTor2(x - v, y - v);
        }
        VecTor2 operator*(float v) const
        {
            return VecTor2(x * v, y * v);
        }
        VecTor2 operator/(float v) const
        {
            return VecTor2(x / v, y / v);
        }
        VecTor2 &operator+=(float v)
        {
            x += v;
            y += v;
            return *this;
        }
        VecTor2 &operator-=(float v)
        {
            x -= v;
            y -= v;
            return *this;
        }
        VecTor2 &operator*=(float v)
        {
            x *= v;
            y *= v;
            return *this;
        }
        VecTor2 &operator/=(float v)
        {
            x /= v;
            y /= v;
            return *this;
        }
        VecTor2 operator+(const VecTor2 &v) const
        {
            return VecTor2(x + v.x, y + v.y);
        }
        VecTor2 operator-(const VecTor2 &v) const
        {
            return VecTor2(x - v.x, y - v.y);
        }
        VecTor2 operator*(const VecTor2 &v) const
        {
            return VecTor2(x * v.x, y * v.y);
        }
        VecTor2 operator/(const VecTor2 &v) const
        {
            return VecTor2(x / v.x, y / v.y);
        }
        VecTor2 &operator+=(const VecTor2 &v)
        {
            x += v.x;
            y += v.y;
            return *this;
        }
        VecTor2 &operator-=(const VecTor2 &v)
        {
            x -= v.x;
            y -= v.y;
            return *this;
        }
        VecTor2 &operator*=(const VecTor2 &v)
        {
            x *= v.x;
            y *= v.y;
            return *this;
        }
        VecTor2 &operator/=(const VecTor2 &v)
        {
            x /= v.x;
            y /= v.y;
            return *this;
        }
    };
    struct VecTor3
    {
        float x;
        float y;
        float z;
        
        float Length() const {
            return sqrtf(x*x + y*y + z*z);
        }
        
        VecTor3()
        {
            this->x = 0;
            this->y = 0;
            this->z = 0;
        }
        VecTor3(float x, float y, float z)
        {
            this->x = x;
            this->y = y;
            this->z = z;
        }
        bool operator!=(const VecTor3 &Pos)
        {
            if (this->x != Pos.x || this->y != Pos.y || this->z != Pos.z)
            {
                return true;
            }
            return false;
        }
        VecTor3 operator+(float v) const
        {
            return VecTor3(x + v, y + v, z + v);
        }
        VecTor3 operator-(float v) const
        {
            return VecTor3(x - v, y - v, z - v);
        }
        VecTor3 operator*(float v) const
        {
            return VecTor3(x * v, y * v, z * v);
        }
        VecTor3 operator/(float v) const
        {
            return VecTor3(x / v, y / v, z / v);
        }
        VecTor3 &operator+=(float v)
        {
            x += v;
            y += v;
            z += v;
            return *this;
        }
        VecTor3 &operator-=(float v)
        {
            x -= v;
            y -= v;
            z -= v;
            return *this;
        }
        VecTor3 &operator*=(float v)
        {
            x *= v;
            y *= v;
            z *= v;
            return *this;
        }
        VecTor3 &operator/=(float v)
        {
            x /= v;
            y /= v;
            z /= v;
            return *this;
        }
        VecTor3 operator+(const VecTor3 &v) const
        {
            return VecTor3(x + v.x, y + v.y, z + v.z);
        }
        VecTor3 operator-(const VecTor3 &v) const
        {
            return VecTor3(x - v.x, y - v.y, z - v.z);
        }
        VecTor3 operator*(const VecTor3 &v) const
        {
            return VecTor3(x * v.x, y * v.y, z * v.z);
        }
        VecTor3 operator/(const VecTor3 &v) const
        {
            return VecTor3(x / v.x, y / v.y, z / v.z);
        }
        VecTor3 &operator+=(const VecTor3 &v)
        {
            x += v.x;
            y += v.y;
            z += v.z;
            return *this;
        }
        VecTor3 &operator-=(const VecTor3 &v)
        {
            x -= v.x;
            y -= v.y;
            z -= v.z;
            return *this;
        }
        VecTor3 &operator*=(const VecTor3 &v)
        {
            x *= v.x;
            y *= v.y;
            z *= v.z;
            return *this;
        }
        VecTor3 &operator/=(const VecTor3 &v)
        {
            x /= v.x;
            y /= v.y;
            z /= v.z;
            return *this;
        }
    };

    struct VecTor4
    {
        float x;
        float y;
        float z;
        float w;
        VecTor4()
        {
            this->x = 0;
            this->y = 0;
            this->z = 0;
            this->w = 0;
        }
        VecTor4(float x, float y, float z, float w)
        {
            this->x = x;
            this->y = y;
            this->z = z;
            this->w = w;
        }
        bool operator!=(const VecTor4 &Pos)
        {
            if (this->x != Pos.x || this->y != Pos.y || this->z != Pos.z || this->w != Pos.w)
            {
                return true;
            }
            return false;
        }
        VecTor4 operator+(float v) const
        {
            return VecTor4(x + v, y + v, z + v, w + v);
        }
        VecTor4 operator-(float v) const
        {
            return VecTor4(x - v, y - v, z - v, w - v);
        }
        VecTor4 operator*(float v) const
        {
            return VecTor4(x * v, y * v, z * v, w * v);
        }
        VecTor4 operator/(float v) const
        {
            return VecTor4(x / v, y / v, z / v, w / v);
        }
        VecTor4 &operator+=(float v)
        {
            x += v;
            y += v;
            z += v;
            w += v;
            return *this;
        }
        VecTor4 &operator-=(float v)
        {
            x -= v;
            y -= v;
            z -= v;
            w -= v;
            return *this;
        }
        VecTor4 &operator*=(float v)
        {
            x *= v;
            y *= v;
            z *= v;
            w *= v;
            return *this;
        }
        VecTor4 &operator/=(float v)
        {
            x /= v;
            y /= v;
            z /= v;
            w /= v;
            return *this;
        }
        VecTor4 operator+(const VecTor4 &v) const
        {
            return VecTor4(x + v.x, y + v.y, z + v.z, w + v.w);
        }
        VecTor4 operator-(const VecTor4 &v) const
        {
            return VecTor4(x - v.x, y - v.y, z - v.z, w - v.w);
        }
        VecTor4 operator*(const VecTor4 &v) const
        {
            return VecTor4(x * v.x, y * v.y, z * v.z, w * v.w);
        }
        VecTor4 operator/(const VecTor4 &v) const
        {
            return VecTor4(x / v.x, y / v.y, z / v.z, w / v.w);
        }
        VecTor4 &operator+=(const VecTor4 &v)
        {
            x += v.x;
            y += v.y;
            z += v.z;
            w += v.w;
            return *this;
        }
        VecTor4 &operator-=(const VecTor4 &v)
        {
            x -= v.x;
            y -= v.y;
            z -= v.z;
            w -= v.w;
            return *this;
        }
        VecTor4 &operator*=(const VecTor4 &v)
        {
            x *= v.x;
            y *= v.y;
            z *= v.z;
            w *= v.w;
            return *this;
        }
        VecTor4 &operator/=(const VecTor4 &v)
        {
            x /= v.x;
            y /= v.y;
            z /= v.z;
            w /= v.w;
            return *this;
        }
    };

    颜色 颜色; // 颜色的类
    float PX, PY;
    float MIDDLE, BOTTOM, TOP;
    float left, right, top, top1, bottom;
    bool isAiming;
    void 初始化绘图(int X, int Y);
    void 初始化坐标(D4DVector &屏幕坐标, 骨骼数据 &骨骼);
//class 绘图 {
//public:
    // ... 其他函数声明 ...
    
    // 修改前的声明（如果存在的话）：
    // void 绘制方框(bool isboot);
    
    // 修改后的声明（两个参数）：
    void 绘制方框(bool isboot, bool isVisible);
    
    // ... 其他函数声明 ...
void 绘制头部骨骼点(D2DVector& 头部坐标, bool isboot, int 队伍, float 距离, bool isVisible);
    void 绘制人数(int 人机, int 真人, uintptr_t 自身);
    void 绘制距离(int 距离, int 队伍);
    void 绘制射线(bool 是否可见,骨骼数据 &骨骼);
    void 绘制血量(float 最大血量, float 当前血量, bool isbot, bool isVisible);
    void 绘制名字(string 敌人UID,string 名字, bool isboot, float 计时, bool 是否掐雷, int 阵营, int Bonecount, bool isVisible);
    void 绘制骨骼(骨骼数据 &骨骼, D4DVector &屏幕坐标, bool LineOfSightToTab[15], int 距离, int Bonecount);
    void 绘制手持(int 手持, int 状态, int 子弹, int 最大子弹);
    void 绘制加粗文本(float size, float x, float y, ImColor color, ImColor color1, const char *str);
    void 绘制字体描边(float size, int x, int y, ImVec4 color, const char *str);
    void RenderRadarScan(ImDrawList* draw_list, ImVec2 center, float radius, int numSegments, float& rotationAngle, float lineLength, float yawRotation);

    void RenderRadarScan(ImDrawList* draw_list, ImVec2 center, float radius, int numSegments, float& rotationAngle, float lineLength);
    bool WorldTurnScreen(VecTor2 &Screen, VecTor3 World, float Matrix[]);

    void Draw3DExplosionCircle(VecTor3 center, ImColor color, float radius, float thickness, float matrix[]);
    


    void Parabola(VecTor3 obj, float Matrix[]);
};
// 绘制
class 绘制
{
public:
    int 距离字体大小 = 18;
    int 名称字体大小 = 18;
    int 手持字体大小 = 18;
    int 动作字体大小 = 18;
    int 世界数量 = 0;
    float 骨骼距离限制 = 300.0f;
    ImVec2 Pos{};
    int winWidth = 684;
    int winHeith = 896;
    int pid = -1;
    float PX = 0, PY = 0;
    float 真实PX = 0, 真实PY = 0;
    Kernel 读写;
    StringFloatMap 手雷类;
    Timer 计时器;
    ImFont* font_24 = nullptr;
    地址 地址{};
    开关 按钮{};
    计算 计算;
    骨骼* 骨骼 = nullptr;
    绘图 绘图;
    自身数据 自身数据{};
    对象地址 对象地址{};
    对象信息 对象信息{};
    void 初始化绘制(string 包名, int 真实X, int 真实Y);
    void 更新地址数据();
    void 更新对象数据();
    void 运行绘制();
    const char* getMaterialName(char* name);
    string getBoxName(int id);
    const char* Level(char* name);
    void OffScreen(ImDrawList* draw, D4DVector obj, float camera, ImU32 color, float radius, float distance);
    void ReadsSaveData(ifstream& file);
    void WriteSaveData(ofstream& file);
    void 保存配置(const char* name);
    void 读取配置(const char* name);
};

class 布局
{
public:
    void 开启悬浮窗();
    void 绘制悬浮窗();
    int 初始化程序();
};

#endif

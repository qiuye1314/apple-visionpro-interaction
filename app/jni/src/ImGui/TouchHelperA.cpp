#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <poll.h>
#include <atomic>
#include <cerrno>
#include <cmath>
#include <linux/input.h>
#include <linux/uinput.h>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <sys/stat.h>
#include <chrono>
#include "src/辅助类.h"

#include "imgui.h"
#include "SpatialInput.h"
#include "SpatialTouchRecovery.h"
#include <TouchHelperA.h>
#include <vector>
#include <thread>
#include <unordered_map>
#include "spinlock.h"
#define maxE 5
#define maxF 10
#define UNGRAB 0
#define GRAB 1

using namespace std;

namespace TouchConfig {
    // 随机设备配置
    static std::mt19937 rng(std::random_device{}());
    static string deviceName = "";
    static string devicePhys = "";
    static struct input_id deviceID = {0};
    
    // 随机间隔配置
    static std::chrono::milliseconds minIntervalMs(500);
    static std::chrono::milliseconds maxIntervalMs(666);
    
    // 生成随机字符串
    static void genRandomString(char *string, int length) {
        int flag;
        for (int i = 0; i < length - 1; i++) {
            flag = rand() % 3;
            switch (flag) {
                case 0: string[i] = 'A' + rand() % 26; break;
                case 1: string[i] = 'a' + rand() % 26; break;
                case 2: string[i] = '0' + rand() % 10; break;
                default: string[i] = 'x'; break;
            }
        }
        string[length - 1] = '\0';
    }
    
    // 设置频率范围
    static void setClickFrequencyRange(float minClicksPerSec, float maxClicksPerSec) {
        if(minClicksPerSec > 0 && maxClicksPerSec > minClicksPerSec) {
            minIntervalMs = std::chrono::milliseconds(static_cast<int>(1000 / maxClicksPerSec));
            maxIntervalMs = std::chrono::milliseconds(static_cast<int>(1000 / minClicksPerSec));
        }
    }
    
    // 生成随机间隔
    static std::chrono::milliseconds getRandomInterval() {
        std::uniform_int_distribution<int> dist(
            minIntervalMs.count(), 
            maxIntervalMs.count()
        );
        return std::chrono::milliseconds(dist(rng));
    }
    
    // 初始化随机设备特征
    static void initRandomDevice() {
        char nameBuffer[UINPUT_MAX_NAME_SIZE];
        char physBuffer[UINPUT_MAX_NAME_SIZE];
        
        // 随机设备名称
        int nameLen = rand() % 10 + 10;
        genRandomString(nameBuffer, nameLen);
        deviceName = nameBuffer;
        
        // 随机设备ID
        deviceID.bustype = rand() % 0xFFFF;
        deviceID.vendor = rand() % 0xFFFF;
        deviceID.product = rand() % 0xFFFF;
        deviceID.version = rand() % 0xFFFF;
    }
}

extern 绘制 绘制;

bool other_touch;

static uint32_t orientation = 0;
static float screenHeight = 0, screenWidth = 0;

struct touchObj
{
    int id = 0;
    int slot = 0;
    bool isDown = false;
    bool isTmpDown = false;
    int x = 0;
    int y = 0;
    int size1 = 0;
    int size2 = 0;
    int size3 = 0;
};
struct Device {
        int fd;
        float S2TX;
        float S2TY;
        input_absinfo absX, absY;
        touchObj Finger[10];

        Device() { memset((void *) this, 0, sizeof(*this)); }
};
struct targ
{
    int fdNum;
    float S2TX;
    float S2TY;
};
static struct
{
    input_event downEvent[2]{{{}, EV_KEY, BTN_TOUCH, 1},
                             {{}, EV_KEY, BTN_TOOL_FINGER, 1}};
    input_event event[512]{0};
} input;

static targ targF[maxE];
static spinlock spin_lock;
static touchObj Finger[maxE][maxF];

static int fdNum = 0, origfd[maxE], nowfd = -1;

static float scale_x, scale_y;

static std::atomic<bool> Touch_initialized{false};
static bool Touch_readOnly = false;
static bool bTouch = false;
static std::vector<Device> devices;
static std::vector<pthread_t> touchThreads;

// 设备检测函数
static bool checkDeviceIsTouch(int fd);
static void lockProcAccess() {
    system("chmod 000 -R /proc/bus/input/*");
    // 双重防护
    system("mount -o bind /empty /proc/bus/input 2>/dev/null");
}

/**
 * @brief
 * 转屏幕坐标
 * @param x
 * @param y
 * @return Vector2
 */
D2DVector Touch2Screen(float x, float y)
{
    
    float xt = x / scale_x;
    float yt = y / scale_y;
    if (other_touch)
    {
        switch (orientation)
        {
        case 1:
            x = xt;
            y = yt;
            break;
        case 2:
            y = yt;
            x = screenHeight - xt;
            break;
        case 3:
            x = screenHeight - xt;
            y = screenWidth - yt;
            break;
        default:
            y = xt;
            x = screenHeight - yt;
            break;
        }
    }
    else
    {
        switch (orientation)
        {
        case 1:
            x = yt;
            y = screenHeight - xt;
            break;
        case 2:
            x = screenHeight - xt;
            y = screenWidth - yt;
            break;
        case 3:
            y = xt;
            x = screenWidth - yt;
            break;
        default:
            x = xt;
            y = yt;
            break;
        }
    }
    return {x, y};
}

// 克隆真实设备特征
static void cloneRealDeviceFeatures(int fd, struct uinput_user_dev* ui_dev) {
    struct input_id id;
    if (!ioctl(fd, EVIOCGID, &id)) {
        ui_dev->id.bustype = id.bustype;
        ui_dev->id.vendor = id.vendor;
        ui_dev->id.product = id.product;
        ui_dev->id.version = id.version;
    }

    // 克隆按键位图
    uint8_t bits[1024] = {0};
    int res = ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(bits)), bits);
    if (res > 0) {
        for (int j = 0; j < res; j++) {
            for (int k = 0; k < 8; k++) {
                if (bits[j] & (1 << k)) {
                    int code = j * 8 + k;
                    if (code != BTN_TOUCH && code != BTN_TOOL_FINGER) {
                        ioctl(nowfd, UI_SET_KEYBIT, code);
                    }
                }
            }
        }
    }
}

static void Upload() {
    static bool isFirstDown = true;
    while (bTouch); // 等待锁释放
    bTouch = true;
    
    int tmpCnt = 0, tmpCnt2 = 0;
    for (auto& device : devices) {
        for (auto& finger : device.Finger) {
            if (finger.isDown) {
                if (tmpCnt2++ > 20) goto finish;
                
                // 位置信息
                input.event[tmpCnt].type = EV_ABS;
                input.event[tmpCnt].code = ABS_X;
                input.event[tmpCnt].value = (int)finger.x;
                tmpCnt++;

                input.event[tmpCnt].type = EV_ABS;
                input.event[tmpCnt].code = ABS_Y;
                input.event[tmpCnt].value = (int)finger.y;
                tmpCnt++;

                input.event[tmpCnt].type = EV_ABS;
                input.event[tmpCnt].code = ABS_MT_POSITION_X;
                input.event[tmpCnt].value = (int)finger.x;
                tmpCnt++;

                input.event[tmpCnt].type = EV_ABS;
                input.event[tmpCnt].code = ABS_MT_POSITION_Y;
                input.event[tmpCnt].value = (int)finger.y;
                tmpCnt++;

                input.event[tmpCnt].type = EV_ABS;
                input.event[tmpCnt].code = ABS_MT_TRACKING_ID;
                input.event[tmpCnt].value = finger.id;
                tmpCnt++;
                
                // 触摸尺寸元数据
                if (finger.size1) {
                    input.event[tmpCnt].type = EV_ABS;
                    input.event[tmpCnt].code = ABS_MT_TOUCH_MAJOR;
                    input.event[tmpCnt].value = finger.size1;
                    tmpCnt++;
                }
                if (finger.size2) {
                    input.event[tmpCnt].type = EV_ABS;
                    input.event[tmpCnt].code = ABS_MT_WIDTH_MAJOR;
                    input.event[tmpCnt].value = finger.size2;
                    tmpCnt++;
                }
                if (finger.size3) {
                    input.event[tmpCnt].type = EV_ABS;
                    input.event[tmpCnt].code = ABS_MT_TOUCH_MINOR;
                    input.event[tmpCnt].value = finger.size3;
                    tmpCnt++;
                }
                
                // 多指上报
                input.event[tmpCnt].type = EV_ABS;
                input.event[tmpCnt].code = ABS_MT_SLOT;
                input.event[tmpCnt].value = finger.id;
                tmpCnt++;

                input.event[tmpCnt].type = EV_SYN;
                input.event[tmpCnt].code = SYN_MT_REPORT;
                input.event[tmpCnt].value = 0;
                tmpCnt++;
            }
        }
    }
finish:
    bool is = false;
    if (tmpCnt == 0) {
        input.event[tmpCnt].type = EV_SYN;
        input.event[tmpCnt].code = SYN_MT_REPORT;
        input.event[tmpCnt].value = 0;
        tmpCnt++;
        if (!isFirstDown) {
            isFirstDown = true;
            input.event[tmpCnt].type = EV_KEY;
            input.event[tmpCnt].code = BTN_TOUCH;
            input.event[tmpCnt].value = 0;
            tmpCnt++;
            input.event[tmpCnt].type = EV_KEY;
            input.event[tmpCnt].code = BTN_TOOL_FINGER;
            input.event[tmpCnt].value = 0;
            tmpCnt++;
        }
    } else {
        is = true;
    }
    
    input.event[tmpCnt].type = EV_SYN;
    input.event[tmpCnt].code = SYN_REPORT;
    input.event[tmpCnt].value = 0;
    tmpCnt++;
    
    // 添加随机延迟增加混淆
    usleep(rand() % 500 + 100);
    
    if (is && isFirstDown) {
        isFirstDown = false;
        write(nowfd, &input, sizeof(struct input_event) * (tmpCnt + 2));
    } else {
        write(nowfd, input.event, sizeof(struct input_event) * tmpCnt);
    }
    
    bTouch = false;
}

// Called while spin_lock is held. A device disappearing must release only its
// own contacts; another touchscreen can still have a valid held pointer.
static void PublishTouchSnapshot() {
    auto sample=spatial_touch_recovery::Snapshot(devices,[](touchObj& finger){
        const auto pos=Touch2Screen(finger.x,finger.y);
        if(!Touch_readOnly&&pos.X>=绘制.Pos.x&&pos.X<=绘制.Pos.x+绘制.winWidth&&
            pos.Y>=绘制.Pos.y&&pos.Y<=绘制.Pos.y+绘制.winHeith)finger.isTmpDown=false;
        return spatial_input::Point{pos.X,pos.Y,finger.id};
    });
    spatial_input::Publish(sample);
}
static int CurrentDeviceSlot(int fd) {
    input_absinfo slot{};
    return ioctl(fd,EVIOCGABS(ABS_MT_SLOT),&slot)==0&&
        spatial_touch_recovery::ValidSlot(slot.value,maxF)?slot.value:-1;
}

static void* TypeA(void* arg) {
    int i = (int)(long)arg;
    Device& device = devices[i];
    
    int latest = 0;
    input_event inputEvent[64]{ 0 };
    spatial_touch_recovery::ReadRecovery recovery;
    bool discardUntilReport=false;
    timer touchFPS;
    touchFPS.SetFps(800);
    touchFPS.AotuFPS_init();
    touchFPS.setAffinity();
    
    while (Touch_initialized.load(std::memory_order_acquire)) {
        pollfd descriptor{device.fd,POLLIN,0};
        const int ready=poll(&descriptor,1,spatial_touch_recovery::PollMilliseconds);
        if(!Touch_initialized.load(std::memory_order_acquire))break;
        if(ready==0)continue;
        if(ready<0){
            const auto step=recovery.Next(-1,errno,sizeof(input_event));
            if(step==spatial_touch_recovery::ReadStep::Disconnect)break;
            if(step==spatial_touch_recovery::ReadStep::Wait)usleep(spatial_touch_recovery::RetryMicroseconds);
            continue;
        }
        if(descriptor.revents&(POLLERR|POLLHUP|POLLNVAL))break;
        if(!(descriptor.revents&POLLIN))continue;
        auto readSize = (int32_t)read(device.fd, inputEvent, sizeof(inputEvent));
        const auto step=recovery.Next(readSize,readSize<0?errno:0,sizeof(input_event));
        if(step==spatial_touch_recovery::ReadStep::Disconnect)break;
        if(step==spatial_touch_recovery::ReadStep::Retry)continue;
        if(step==spatial_touch_recovery::ReadStep::Wait){usleep(spatial_touch_recovery::RetryMicroseconds);continue;}
        
        size_t count = size_t(readSize) / sizeof(input_event);
        spin_lock.lock();
        
        for (size_t j = 0; j < count; j++) {
            input_event& ie = inputEvent[j];
            if(ie.type==EV_SYN&&ie.code==SYN_DROPPED){
                spatial_touch_recovery::Release(device.Finger);PublishTouchSnapshot();
                discardUntilReport=true;latest=-1;continue;
            }
            if(discardUntilReport){
                if(ie.type==EV_SYN&&ie.code==SYN_REPORT){
                    latest=CurrentDeviceSlot(device.fd);
                    discardUntilReport=false;
                }
                continue;
            }
            if (ie.type == EV_ABS) {
                if (ie.code == ABS_MT_SLOT) {
                    latest = spatial_touch_recovery::ValidSlot(ie.value,maxF)?ie.value:-1;
                    continue;
                }
                if(!spatial_touch_recovery::ValidSlot(latest,maxF))continue;
                touchObj& finger = device.Finger[latest];
                
                if (ie.code == ABS_MT_TRACKING_ID) {
                    finger.isDown = (ie.value != -1);
                    if (finger.isDown) {
                        finger.id = (i * 2 + 1) * maxF + latest;
                    }
                } 
                else if (ie.code == ABS_MT_POSITION_X) {
                    finger.x = (float)ie.value * device.S2TX;
                    finger.id = (i * 2 + 1) * maxF + latest;
                } 
                else if (ie.code == ABS_MT_POSITION_Y) {
                    finger.y = (float)ie.value * device.S2TY;
                    finger.id = (i * 2 + 1) * maxF + latest;
                }
                // 触摸尺寸元数据
                else if (ie.code == ABS_MT_TOUCH_MAJOR) {
                    finger.size1 = ie.value;
                } 
                else if (ie.code == ABS_MT_WIDTH_MAJOR) {
                    finger.size2 = ie.value;
                } 
                else if (ie.code == ABS_MT_TOUCH_MINOR) {
                    finger.size3 = ie.value;
                }
            }
            
             // GUI区域触摸处理
            if (ie.type == EV_SYN && ie.code == SYN_REPORT) {
                if(latest<0)latest=CurrentDeviceSlot(device.fd);
                PublishTouchSnapshot();

                if (!Touch_readOnly) Upload();
            }
        }
        
        spin_lock.unlock();
        touchFPS.SetFps(240); // UI touch polling is independent of drawing settings.
        touchFPS.AotuFPS();
    }
    spin_lock.lock();
    spatial_touch_recovery::Release(device.Finger);
    PublishTouchSnapshot();
    spin_lock.unlock();
    // Touch_Close joins all readers before closing descriptors or mutating devices.
    return nullptr;
}

bool Touch_Init(int w, int h, uint32_t orientation_, bool readOnly)
{
    Touch_Close();
    if(w<=0||h<=0)return false;
    Touch_readOnly=readOnly;
    // 初始化随机设备特征
    TouchConfig::initRandomDevice();
    
    char temp[128];
    DIR *dir = opendir("/dev/input/");
    if(!dir)return false;
    dirent *ptr = NULL;
    int eventCount = 0;
    while ((ptr = readdir(dir)) != NULL) {
        if (strstr(ptr->d_name, "event")) eventCount++;
    }
    closedir(dir);
    
    struct input_absinfo abs, absX[maxE], absY[maxE];
    int fd, i, tmp1, tmp2;
    int screenX, screenY, minCnt = eventCount + 1;
    fdNum = 0;
    
    for (i = 0; i <= eventCount; i++) {
        sprintf(temp, "/dev/input/event%d", i);
        fd = open(temp, O_RDWR | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) continue;
        
        if (checkDeviceIsTouch(fd)) {
            Device device{};
            if (ioctl(fd, EVIOCGABS(ABS_MT_POSITION_X), &device.absX) == 0 &&
                ioctl(fd, EVIOCGABS(ABS_MT_POSITION_Y), &device.absY) == 0 &&
                device.absX.maximum>0 && device.absY.maximum>0) {
                    
                device.fd = fd;
                if (!readOnly) ioctl(fd, EVIOCGRAB, GRAB);
                devices.push_back(device);
                
                if (i < minCnt) {
                    screenX = device.absX.maximum;
                    screenY = device.absY.maximum;
                    minCnt = i;
                }
                
                if (++fdNum >= maxE) break;
            } else close(fd);
        } else {
            close(fd);
        }
    }

    if (minCnt > eventCount) {Touch_Close();return false;}

    if (!readOnly) {
        // 创建虚拟输入设备
        nowfd = open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
        if (nowfd < 0) {Touch_Close();return false;}

        struct uinput_user_dev ui_dev = {0};
        
        // 应用随机设备特征
        strncpy(ui_dev.name, TouchConfig::deviceName.c_str(), UINPUT_MAX_NAME_SIZE);
        ui_dev.id = TouchConfig::deviceID;
        
        // 设置设备属性
        ioctl(nowfd, UI_SET_PROPBIT, INPUT_PROP_DIRECT);
        
        // 基本事件类型
        ioctl(nowfd, UI_SET_EVBIT, EV_ABS);
        ioctl(nowfd, UI_SET_EVBIT, EV_SYN);
        ioctl(nowfd, UI_SET_EVBIT, EV_KEY);
        
        // 坐标轴
        ioctl(nowfd, UI_SET_ABSBIT, ABS_X);
        ioctl(nowfd, UI_SET_ABSBIT, ABS_Y);
        ioctl(nowfd, UI_SET_ABSBIT, ABS_MT_POSITION_X);
        ioctl(nowfd, UI_SET_ABSBIT, ABS_MT_POSITION_Y);
        ioctl(nowfd, UI_SET_ABSBIT, ABS_MT_TRACKING_ID);
        
        // 触摸尺寸元数据
        ioctl(nowfd, UI_SET_ABSBIT, ABS_MT_TOUCH_MAJOR);
        ioctl(nowfd, UI_SET_ABSBIT, ABS_MT_WIDTH_MAJOR);
        ioctl(nowfd, UI_SET_ABSBIT, ABS_MT_TOUCH_MINOR);
        
        // 按键
        ioctl(nowfd, UI_SET_KEYBIT, BTN_TOUCH);
        ioctl(nowfd, UI_SET_KEYBIT, BTN_TOOL_FINGER);
        
        // 尝试克隆真实设备特征
        sprintf(temp, "/dev/input/event%d", minCnt);
        int realFd = open(temp, O_RDONLY | O_CLOEXEC);
        if (realFd >= 0) {
            cloneRealDeviceFeatures(realFd, &ui_dev);
            close(realFd);
        }
        
        // 设置坐标范围
        ui_dev.absmin[ABS_MT_POSITION_X] = 0;
        ui_dev.absmax[ABS_MT_POSITION_X] = screenX;
        ui_dev.absmin[ABS_MT_POSITION_Y] = 0;
        ui_dev.absmax[ABS_MT_POSITION_Y] = screenY;
        ui_dev.absmin[ABS_X] = 0;
        ui_dev.absmax[ABS_X] = screenX;
        ui_dev.absmin[ABS_Y] = 0;
        ui_dev.absmax[ABS_Y] = screenY;
        ui_dev.absmin[ABS_MT_TRACKING_ID] = 0;
        ui_dev.absmax[ABS_MT_TRACKING_ID] = 65535;
        
        // 触摸尺寸范围
        ui_dev.absmin[ABS_MT_TOUCH_MAJOR] = 0;
        ui_dev.absmax[ABS_MT_TOUCH_MAJOR] = 255;
        ui_dev.absmin[ABS_MT_WIDTH_MAJOR] = 0;
        ui_dev.absmax[ABS_MT_WIDTH_MAJOR] = 255;
        ui_dev.absmin[ABS_MT_TOUCH_MINOR] = 0;
        ui_dev.absmax[ABS_MT_TOUCH_MINOR] = 255;

        // 创建设备
        write(nowfd, &ui_dev, sizeof(ui_dev));
        if (ioctl(nowfd, UI_DEV_CREATE) == -1) {
            Touch_Close();
            return false;
        }
    }

    Touch_readOnly = readOnly;
    
    ::screenWidth = w;
    ::screenHeight = h;
    ::orientation = orientation_;
    
    if (::orientation == 1 || ::orientation == 3) {
        ::scale_x = (float)screenX / h;
        ::scale_y = (float)screenY / w;
    } else {
        ::scale_x = (float)screenX / w;
        ::scale_y = (float)screenY / h;
    }
    for(auto& device:devices){
        device.S2TX=(float)screenX/(float)device.absX.maximum;
        device.S2TY=(float)screenY/(float)device.absY.maximum;
    }
    Touch_initialized.store(true,std::memory_order_release);
    for (int i = 0; i < devices.size(); i++) {
        pthread_t thread;
        if(pthread_create(&thread,nullptr,TypeA,(void*)(long)i)==0)touchThreads.push_back(thread);
    }
    if(touchThreads.empty()){Touch_Close();return false;}
    
    // 锁定系统输入访问
    lockProcAccess();
    return true;
}

void UpdateScreenData(int w, int h, uint32_t orientation_)
{
    spin_lock.lock();
    ::screenWidth = w;
    ::screenHeight = h,
    ::orientation = orientation_;
    spin_lock.unlock();
}

static bool checkDeviceIsTouch(int fd) {
    uint8_t bits[128] = {0};
    bool hasSlot = false, hasX = false, hasY = false;
    
    if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(bits)), bits) < 0) {
        return false;
    }
    
    for (int j = 0; j < sizeof(bits); j++) {
        for (int k = 0; k < 8; k++) {
            if (bits[j] & (1 << k)) {
                int code = j * 8 + k;
                if (code == ABS_MT_SLOT) hasSlot = true;
                if (code == ABS_MT_POSITION_X) hasX = true;
                if (code == ABS_MT_POSITION_Y) hasY = true;
            }
        }
    }
    return hasSlot && hasX && hasY;
}

void Touch_Down(float x, float y)
{
    spin_lock.lock();
    touchObj &touch = devices[0].Finger[9];
    touch.id = 8;
    touch.x = (int)(x * scale_x);
    touch.y = (int)(y * scale_y);
    touch.isDown = true;
    // 随机触摸尺寸增加真实性
    touch.size1 = rand() % 10 + 5;
    touch.size2 = rand() % 8 + 4;
    touch.size3 = rand() % 6 + 3;
    Upload();
    spin_lock.unlock();
}

void Touch_Down(int id, float x, float y)
{
    spin_lock.lock();
    touchObj &touch = devices[0].Finger[id];
    touch.id = id;
    touch.x = (int)(x * scale_x);
    touch.y = (int)(y * scale_y);
    touch.isDown = true;
    // 随机触摸尺寸
    touch.size1 = rand() % 15 + 5;
    touch.size2 = rand() % 12 + 4;
    touch.size3 = rand() % 10 + 3;
    Upload();
    spin_lock.unlock();
}

void Touch_Up(int id)
{
    spin_lock.lock();
    touchObj &touch = devices[0].Finger[id];
    touch.isDown = false;
    Upload();
    spin_lock.unlock();
}

void Touch_Move(float x, float y)
{
    // // 添加随机偏移增加真实感
    // float offsetX = (rand() % 5 - 2) * scale_x;
    // float offsetY = (rand() % 5 - 2) * scale_y;
    // Touch_Down(x + offsetX, y + offsetY);
      Touch_Down(x, y);

}

void Touch_Up()
{
    spin_lock.lock();
    touchObj &touch = devices[0].Finger[9];
    touch.isDown = false;
    Upload();
    spin_lock.unlock();
}

/**
 * @brief 连点功能
 * @param isClick 如果在矩形内是否需要连点
 * @param clickRect 连点矩形位置
 * @return true 手指在连点位置内
 * @return false 手指不在连点位置内
 */
bool clickRegion(bool isClick, ARect clickRect) {
    static std::chrono::steady_clock::time_point lastClickTime;
    // 设置连点频率，每秒4-5次点击速度
    TouchConfig::setClickFrequencyRange(15.0f, 20.0f);
    
    for (auto& device : devices) {
        for (auto& finger : device.Finger) {
            if (finger.isDown) {
                auto pos = Touch2Screen(finger.x, finger.y);
                float mouseX = pos.X;
                float mouseY = pos.Y;
                
                // 修改这里：手动判断点是否在矩形内
                if (mouseX >= clickRect.left && mouseX <= clickRect.right && 
                    mouseY >= clickRect.top && mouseY <= clickRect.bottom) {
                    spin_lock.lock();
                    auto now = std::chrono::steady_clock::now();
                    // 动态生成随机间隔
                    auto requiredInterval = TouchConfig::getRandomInterval();
                    // printf("连点触发中");
                    if (now - lastClickTime >= requiredInterval && isClick) {
                        // 抬起用户连点位置手指
                        finger.isDown = false;
                        Upload();
                        
                        // 短暂延迟后重新按下
                        usleep(20000);
                        finger.isDown = true;
                        lastClickTime = now;
                    }
                    spin_lock.unlock();
                    return true;
                }
            }
        }
    }
    return false;
}


void Touch_Close() {
    Touch_initialized.store(false,std::memory_order_release);
    for(pthread_t thread:touchThreads)pthread_join(thread,nullptr);
    touchThreads.clear();
    for (auto& device : devices) {
        if (device.fd >= 0) {
            if (!Touch_readOnly) ioctl(device.fd, EVIOCGRAB, UNGRAB);
            close(device.fd);device.fd=-1;
        }
    }
    devices.clear();fdNum=0;
    if (nowfd >= 0) {ioctl(nowfd, UI_DEV_DESTROY);close(nowfd);nowfd=-1;}
    spatial_input::Publish(spatial_input::Sample{});
}

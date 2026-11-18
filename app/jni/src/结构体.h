#pragma once
#define LSRAEL_ROTATOR_DEFINED 1

/* 
 * 由入梦 频道@lsraelnb开源
 * 注入时间: 2026-07-05 11:50:33
 * 联系方式: 交流群：1074049072
 */
#ifndef 结构体_H
#define 结构体_H

#include <cstring>
#include <cmath>
#include <cstdint>
#include <vector>
#include <iostream>
#include "版本数据_1_39.h"   /* 【1.39 版本数据】所有 libUE4 偏移集中在这里 */
using namespace std;

struct Vec2
{
    float x;
    float y;
    Vec2()
    {
        this->x = 0;
        this->y = 0;
    }
    Vec2(float x, float y)
    {
        this->x = x;
        this->y = y;
    }
    Vec2 operator+(float v) const
    {
        return Vec2(x + v, y + v);
    }
    Vec2 operator-(float v) const
    {
        return Vec2(x - v, y - v);
    }
    Vec2 operator*(float v) const
    {
        return Vec2(x * v, y * v);
    }
    Vec2 operator/(float v) const
    {
        return Vec2(x / v, y / v);
    }
    Vec2 &operator+=(float v)
    {
        x += v;
        y += v;
        return *this;
    }
    Vec2 &operator-=(float v)
    {
        x -= v;
        y -= v;
        return *this;
    }
    Vec2 &operator*=(float v)
    {
        x *= v;
        y *= v;
        return *this;
    }
    Vec2 &operator/=(float v)
    {
        x /= v;
        y /= v;
        return *this;
    }
    Vec2 operator+(const Vec2 &v) const
    {
        return Vec2(x + v.x, y + v.y);
    }
    Vec2 operator-(const Vec2 &v) const
    {
        return Vec2(x - v.x, y - v.y);
    }
    Vec2 operator*(const Vec2 &v) const
    {
        return Vec2(x * v.x, y * v.y);
    }
    Vec2 operator/(const Vec2 &v) const
    {
        return Vec2(x / v.x, y / v.y);
    }
    Vec2 &operator+=(const Vec2 &v)
    {
        x += v.x;
        y += v.y;
        return *this;
    }
    Vec2 &operator-=(const Vec2 &v)
    {
        x -= v.x;
        y -= v.y;
        return *this;
    }
    Vec2 &operator*=(const Vec2 &v)
    {
        x *= v.x;
        y *= v.y;
        return *this;
    }
    Vec2 &operator/=(const Vec2 &v)
    {
        x /= v.x;
        y /= v.y;
        return *this;
    }
};

struct Vec3
{
    float x;
    float y;
    float z;

    Vec3()
    {
        x = y = z = 0.0f;
    }

    Vec3(float _x, float _y, float _z)
    {
        x = _x;
        y = _y;
        z = _z;
    }

    Vec3 operator+(const Vec3 &v) const
    {
        return {x + v.x, y + v.y, z + v.z};
    }

    Vec3 operator-(const Vec3 &v) const
    {
        return {x - v.x, y - v.y, z - v.z};
    }

    bool operator==(const Vec3 &v)
    {
        return x == v.x && y == v.y && z == v.z;
    }

    bool operator!=(const Vec3 &v)
    {
        return !(x == v.x && y == v.y && z == v.z);
    }

    static Vec3 Zero()
    {
        return {0.0f, 0.0f, 0.0f};
    }

    static float Dot(Vec3 lhs, Vec3 rhs)
    {
        return (((lhs.x * rhs.x) + (lhs.y * rhs.y)) + (lhs.z * rhs.z));
    }

    static float Distance(Vec3 a, Vec3 b)
    {
        Vec3 vector = Vec3(a.x - b.x, a.y - b.y, a.z - b.z);
        return sqrt(((vector.x * vector.x) + (vector.y * vector.y)) + (vector.z * vector.z));
    }
};



class FRotator
{
public:
    FRotator()
        : Pitch(0.f), Yaw(0.f), Roll(0.f)
    {
    }
    FRotator(float _Pitch, float _Yaw, float _Roll)
        : Pitch(_Pitch), Yaw(_Yaw), Roll(_Roll)
    {
    }
    ~FRotator()
    {
    }
    float Pitch;
    float Yaw;
    float Roll;
    inline FRotator Clamp()
    {
        if (Pitch > 180)
        {
            Pitch -= 360;
        }
        else
        {
            if (Pitch < -180)
            {
                Pitch += 360;
            }
        }
        if (Yaw > 180)
        {
            Yaw -= 360;
        }
        else
        {
            if (Yaw < -180)
            {
                Yaw += 360;
            }
        }
        if (Pitch > 89)
        {
            Pitch = 89;
        }
        if (Pitch < -89)
        {
            Pitch = -89;
        }
        while (Yaw < 180)
        {
            Yaw += 360;
        }
        while (Yaw > 180)
        {
            Yaw -= 360;
        }
        Roll = 0;
        return FRotator(Pitch, Yaw, Roll);
    }
    inline float Length()
    {
        return sqrtf(Pitch * Pitch + Yaw * Yaw + Roll * Roll);
    }
    FRotator operator+(FRotator v)
    {
        return FRotator(Pitch + v.Pitch, Yaw + v.Yaw, Roll + v.Roll);
    }
    FRotator operator-(FRotator v)
    {
        return FRotator(Pitch - v.Pitch, Yaw - v.Yaw, Roll - v.Roll);
    }
};

struct 开关
{
    bool 绘制 = false;
    bool 方框 = true;
    bool 骨骼 = true;
    bool 射线 = false;
    bool 名字 = true;
    bool 距离 = true;
    bool 血量 = true;
    bool 人数 = true;
    bool 手持 = true;
    bool 显示头部骨骼点 = false;
    bool 忽略人机 = false;
    bool 忽略倒地 = false;
    bool 掩体识别 = false;
    bool 绘制全图 = false;
    bool 雷达 = false;
    bool 背敌预警 = false;
    bool 手雷预警 = false;
    bool 迫击炮落点 = false;
    bool PhysX画线 = false;
    bool 车辆 = false;
    bool 载具血量 = false;
    bool 载具油量 = false;
    bool 盒子 = false;
    bool 盒子物资 = false;
    bool 空投 = false;
    bool 绘制宝箱 = false;
    bool 绘制药箱 = false;
    bool 绘制武器箱 = false;
    bool 隐藏已开启 = true;
    bool 超级物资箱 = false;
    bool 海岛宝箱 = false;
    bool 绘制信号枪 = false;
    bool 其他物资 = false;
    bool 冲锋枪械 = false;
    bool 投掷物品 = false;
    bool 散弹枪械 = false;
    bool 显示倍镜 = false;
    bool 显示子弹 = false;
    bool 显示步枪 = false;
    bool 显示药品 = false;
    bool 显示配件 = false;
    bool 显示防具 = false;
    bool 狙击枪械 = false;
    float 当前帧率 = 90.0f;
    float 方框粗细 = 1.2f;
    float 射线粗细 = 0.9f;
    float 骨骼粗细 = 1.3f;
    float 雷达X = 300.0f;
    float 雷达Y = 400.0f;
    float 雷达缩放 = 1.0f;
    float rotationAngle = 0.0f;
    int VehicleID = 0;
};
struct D3DVector
{
    float X;
    float Y;
    float Z;
    D3DVector()
    {
        this->X = 0;
        this->Y = 0;
        this->Z = 0;
    }
    D3DVector(float x, float y, float z)
    {
        this->X = x;
        this->Y = y;
        this->Z = z;
    }
};

struct 地址
{
    uintptr_t libdi;
    uintptr_t libue4;
    uintptr_t 世界指针;      /* 【1.39】UWorld*（GWorld 解引用） */
    uintptr_t 对局状态;      /* 【1.39】UWorld+0xAD8 = GameState */
    uintptr_t 世界地址;
    uintptr_t 类地址;
    uintptr_t 解密地址;
    uintptr_t 自身地址;
    uintptr_t 矩阵地址;
    uintptr_t 矩阵地址_Tol;
    uintptr_t 数组地址;
    long int 全图人数;
    long int 队伍数;
    long int 真实玩家;
    uintptr_t PlayerController;
    uintptr_t PlayerCameraManager;
    D3DVector SelfCameraCoordinate;
};

struct FMatrix
{
    float M[4][4];
};

struct D2DVector
{
    float X;
    float Y;
    D2DVector()
    {
        this->X = 0;
        this->Y = 0;
    }
    D2DVector(float x, float y)
    {
        this->X = x;
        this->Y = y;
    }
};



struct D4DVector
{
    float X;
    float Y;
    float Z;
    float W;
    D4DVector()
    {
        this->X = 0;
        this->Y = 0;
        this->Z = 0;
        this->W = 0;
    }
    D4DVector(float x, float y, float z, float w)
    {
        this->X = x;
        this->Y = y;
        this->Z = z;
        this->W = w;
    }
};





struct FTransform
{
    D4DVector Rotation;
    D3DVector Translation;
    float chunk;
    D3DVector Scale3D;
};





struct 骨骼数据
{
    D2DVector Head;
    D2DVector Chest;
    D2DVector Pelvis;
    D2DVector Left_Shoulder;
    D2DVector Right_Shoulder;
    D2DVector Left_Elbow;
    D2DVector Right_Elbow;
    D2DVector Left_Wrist;
    D2DVector Right_Wrist;
    D2DVector Left_Thigh;
    D2DVector Right_Thigh;
    D2DVector Left_Knee;
    D2DVector Right_Knee;
    D2DVector Left_Ankle;
    D2DVector Right_Ankle;

    std::vector<D2DVector *> 获取所有骨骼指针()
    {
        return {
            &Head, &Chest, &Pelvis,
            &Left_Shoulder, &Right_Shoulder,
            &Left_Elbow, &Right_Elbow,
            &Left_Wrist, &Right_Wrist,
            &Left_Thigh, &Right_Thigh,
            &Left_Knee, &Right_Knee,
            &Left_Ankle, &Right_Ankle};
    }
};
struct 自身数据
{
    D3DVector 坐标;
    float 矩阵[16] = {};
    int 自身队伍 = -1;
    float Fov = 90.0f;
    float 准星Y = 0.0f;
    int 全图人数 = 0;
    int 玩家人数 = 0;
    int 队伍数 = 0;
    D3DVector CameraLocation;
    FRotator CameraRotator;
};

// 地址结构体
struct 对象地址
{
    long 敌人地址;
    long 物品地址[1000];
    long 车辆地址[50];
};

struct 敌人信息
{
    D3DVector 坐标;
    D3DVector 骨骼坐标[17];
    D2DVector 雷达;
    bool isboot = false;
    int 队伍 = -1;
    int 状态 = 0;
    int 距离 = 0;
    int 手持 = 0;
    float 当前血量 = 0.0f;
    float 最大血量 = 100.0f;
    string 名字;
    string 分区;
    int 子弹数量 = 0;
    int 子弹最大数量 = 0;
};
struct 物品信息
{
    D3DVector 坐标;
    int 物品;
    int 距离;
};

struct 车辆信息
{
    D3DVector 坐标;
    int 车辆;
    int 距离;
};

// 数据结构体
struct 对象信息
{
    int isCanRead;
    int 敌人数量;
    int 物品数量;
    int 车辆数量;
    敌人信息 敌人信息;
    物品信息 物品信息[1000];
    车辆信息 车辆信息[50];
};

struct D3DXMATRIX
{
    float _11;
    float _12;
    float _13;
    float _14;
    float _21;
    float _22;
    float _23;
    float _24;
    float _31;
    float _32;
    float _33;
    float _34;
    float _41;
    float _42;
    float _43;
    float _44;
};

struct D3DXVECTOR4
{
    float X;
    float Y;
    float Z;
    float W;
};

struct FTransform1
{
    D3DXVECTOR4 Rotation;
    D3DVector Translation;
    D3DVector Scale3D;
};

#endif

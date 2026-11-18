#pragma once
// UI motion only: orientation input never changes application controls or drivers.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace spatial_motion {
inline bool Finite(float value){std::uint32_t bits;std::memcpy(&bits,&value,4);return (bits&0x7f800000u)!=0x7f800000u;}
inline bool Finite(double value){std::uint64_t bits;std::memcpy(&bits,&value,8);return (bits&UINT64_C(0x7ff0000000000000))!=UINT64_C(0x7ff0000000000000);}
inline float Limit(float value,float bound){return std::max(-bound,std::min(bound,value));}
struct Quaternion {float x=0,y=0,z=0,w=1;};
struct Pose {float pitch=0,yaw=0,roll=0;};
inline Quaternion Multiply(Quaternion a,Quaternion b){return {
    a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
    a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};}
inline bool Normalize(Quaternion& q){
    if(!Finite(q.x)||!Finite(q.y)||!Finite(q.z)||!Finite(q.w))return false;
    if(std::max({std::fabs(q.x),std::fabs(q.y),std::fabs(q.z),std::fabs(q.w)})>2.f)return false;
    const float length=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
    if(length<.01f)return false;
    const float inverse=1.f/std::sqrt(length);q.x*=inverse;q.y*=inverse;q.z*=inverse;q.w*=inverse;return true;
}
inline Quaternion Axis(float x,float y,float z,float radians){
    const float s=std::sin(radians*.5f);return {x*s,y*s,z*s,std::cos(radians*.5f)};
}
struct Sample {Quaternion orientation;double timestamp=0;int rotation=0;};
enum class Status {Idle,Waiting,Ready,Unavailable};
using Poll = Status(*)(bool active,Sample& sample);
class Controller {
public:
    void Reset(){reference_={};base_={};offset_={};timestamp_=-1;rotation_=-1;calibrated_=false;}
    const Pose& Offset()const{return offset_;}
    bool Push(Sample sample,bool held){
        if(!Finite(sample.timestamp)||sample.timestamp<0||!Normalize(sample.orientation))return false;
        if(sample.rotation<0||sample.rotation>3)return false;
        if(calibrated_&&sample.timestamp<=timestamp_)return false;
        if(!calibrated_||sample.rotation!=rotation_||sample.timestamp-timestamp_>.5){
            reference_=sample.orientation;base_={};offset_={};calibrated_=true;rotation_=sample.rotation;
        }
        timestamp_=sample.timestamp;
        if(held){reference_=sample.orientation;base_=offset_;return true;}
        const auto q=sample.orientation;
        Quaternion relative=Multiply({-reference_.x,-reference_.y,-reference_.z,reference_.w},q);
        if(relative.w<0){relative.x=-relative.x;relative.y=-relative.y;relative.z=-relative.z;relative.w=-relative.w;}
        const float length=std::sqrt(relative.x*relative.x+relative.y*relative.y+relative.z*relative.z);
        const float factor=length>.00001f?2.f*std::atan2(length,relative.w)/length*57.2957795f:114.591559f;
        float x=relative.x*factor,y=relative.y*factor,z=relative.z*factor;
        switch(rotation_){case 1:{const float old=x;x=y;y=-old;break;}case 2:x=-x;y=-y;break;
                          case 3:{const float old=x;x=-y;y=old;break;}default:break;}
        offset_={Limit(base_.pitch-x*.7f,18),Limit(base_.yaw+y*.7f,22),Limit(base_.roll-z*.4f,10)};
        return true;
    }
private:
    Quaternion reference_{};Pose base_{},offset_{};double timestamp_=-1;int rotation_=-1;bool calibrated_=false;
};
class Integrator {
public:
    void Reset(){orientation_={};last_=-1;}
    bool Push(float x,float y,float z,double timestamp){
        if(!Finite(x)||!Finite(y)||!Finite(z)||!Finite(timestamp)||timestamp<0||
           std::max({std::fabs(x),std::fabs(y),std::fabs(z)})>35)return false;
        if(last_>=0&&timestamp<=last_)return false;
        if(last_<0||timestamp-last_>.25){last_=timestamp;return true;}
        const float dt=static_cast<float>(timestamp-last_);last_=timestamp;
        if(std::fabs(x)<.012f)x=0;if(std::fabs(y)<.012f)y=0;if(std::fabs(z)<.012f)z=0;
        const float speed=std::sqrt(x*x+y*y+z*z);if(speed<.00001f)return true;
        orientation_=Multiply(orientation_,Axis(x/speed,y/speed,z/speed,speed*dt));return Normalize(orientation_);
    }
    Quaternion Orientation()const{return orientation_;}
private:Quaternion orientation_{};double last_=-1;
};
struct Runtime {Controller controller;Poll poll=nullptr;Status status=Status::Idle;double lastSeen=-1,requestedAt=-1;};
inline Runtime& Get(){static Runtime value;return value;}
inline void Install(Poll poll){Get().poll=poll;}
inline void Reset(){Get().controller.Reset();Get().lastSeen=-1;Get().requestedAt=-1;}
inline Pose Update(bool active,bool held,double now){
    auto& runtime=Get();Sample sample;
    runtime.status=runtime.poll?runtime.poll(active,sample):(active?Status::Unavailable:Status::Idle);
    if(!active||runtime.status==Status::Unavailable){Reset();return {};}
    if(runtime.requestedAt<0)runtime.requestedAt=now;
    if(runtime.status==Status::Ready&&runtime.controller.Push(sample,held))runtime.lastSeen=now;
    if(runtime.lastSeen>=0&&now-runtime.lastSeen>.5){runtime.controller.Reset();runtime.status=Status::Waiting;}
    if(now-(runtime.lastSeen>=0?runtime.lastSeen:runtime.requestedAt)>2){Reset();runtime.status=Status::Unavailable;}
    return runtime.controller.Offset();
}
} // namespace spatial_motion

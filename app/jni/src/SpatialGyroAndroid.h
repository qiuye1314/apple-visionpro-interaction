#pragma once
#include "SpatialMotion.h"
#include <android/looper.h>
#include <android/sensor.h>

namespace spatial_gyro_android {
class Backend {
public:
    ~Backend(){
        Stop();
        if(queue_&&manager_)ASensorManager_destroyEventQueue(manager_,queue_);
        if(looper_)ALooper_release(looper_);
    }
    int rotation=0;
    spatial_motion::Status Poll(bool active,spatial_motion::Sample& sample){
        using spatial_motion::Status;
        if(!active){Stop();attempted_=false;return Status::Idle;}
        if(!running_&&!Start())return Status::Unavailable;
        ASensorEvent events[32];bool received=false;
        for(int batch=0;batch<8&&ASensorEventQueue_hasEvents(queue_)>0;++batch){
            const ssize_t count=ASensorEventQueue_getEvents(queue_,events,32);
            if(count<0){Stop();return Status::Unavailable;}
            if(!count)break;
            for(ssize_t i=0;i<count;++i){const auto& event=events[i];if(event.type!=type_||event.timestamp<=0)continue;
                spatial_motion::Quaternion orientation;const double time=static_cast<double>(event.timestamp)*1e-9;
                if(type_==ASENSOR_TYPE_GYROSCOPE){
                    if(!integrator_.Push(event.vector.x,event.vector.y,event.vector.z,time))continue;
                    orientation=integrator_.Orientation();
                }else{
                    orientation={event.data[0],event.data[1],event.data[2],event.data[3]};
                    if(!spatial_motion::Normalize(orientation))continue;
                }
                sample={orientation,time,rotation};received=true;
            }
        }
        return received?Status::Ready:Status::Waiting;
    }
    int Type()const{return type_;}
private:
    bool Start(){
        if(attempted_)return false;attempted_=true;
        if(!manager_)manager_=ASensorManager_getInstanceForPackage("");
        if(!manager_)return false;
        if(!queue_){ALooper* looper=ALooper_forThread();if(!looper)looper=ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
            if(!looper)return false;
            // The thread-local looper may be released before static destructors.
            // Retain our own reference until the event queue has been destroyed.
            ALooper_acquire(looper);looper_=looper;
            queue_=ASensorManager_createEventQueue(manager_,looper_,0x5350,nullptr,nullptr);
            if(!queue_){ALooper_release(looper_);looper_=nullptr;}}
        if(!queue_)return false;
        const int types[]={ASENSOR_TYPE_GAME_ROTATION_VECTOR,ASENSOR_TYPE_ROTATION_VECTOR,ASENSOR_TYPE_GYROSCOPE};
        for(int type:types){
            const ASensor* sensor=ASensorManager_getDefaultSensor(manager_,type);if(!sensor)continue;
            if(ASensorEventQueue_enableSensor(queue_,sensor)<0)continue;
            const int interval=std::max(16667,ASensor_getMinDelay(sensor));
            if(ASensorEventQueue_setEventRate(queue_,sensor,interval)<0){ASensorEventQueue_disableSensor(queue_,sensor);continue;}
            sensor_=sensor;type_=type;running_=true;integrator_.Reset();return true;
        }
        return false;
    }
    void Stop(){
        if(running_&&queue_&&sensor_)ASensorEventQueue_disableSensor(queue_,sensor_);
        if(queue_){ASensorEvent buffer[32];for(int n=0;n<8&&ASensorEventQueue_hasEvents(queue_)>0;++n)
            if(ASensorEventQueue_getEvents(queue_,buffer,32)<=0)break;}
        running_=false;sensor_=nullptr;integrator_.Reset();
    }
    ASensorManager* manager_=nullptr;ASensorEventQueue* queue_=nullptr;ALooper* looper_=nullptr;const ASensor* sensor_=nullptr;
    spatial_motion::Integrator integrator_;bool attempted_=false,running_=false;int type_=0;
};
inline Backend& Get(){static Backend value;return value;}
inline spatial_motion::Status Poll(bool active,spatial_motion::Sample& sample){return Get().Poll(active,sample);}
inline void Install(int rotation){Get().rotation=rotation&3;spatial_motion::Install(Poll);}
} // namespace spatial_gyro_android

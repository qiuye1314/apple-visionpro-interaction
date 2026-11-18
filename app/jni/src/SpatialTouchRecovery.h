#pragma once
#include "SpatialInput.h"
#include <cerrno>
#include <cstddef>

namespace spatial_touch_recovery {
enum class ReadStep { Data, Retry, Wait, Disconnect };
constexpr int PollMilliseconds=100;
constexpr int RetryMicroseconds=8000;
struct ReadRecovery {
    unsigned errors=0;
    ReadStep Next(long bytes,int error,std::size_t eventSize) {
        if(bytes>0&&eventSize&&static_cast<std::size_t>(bytes)%eventSize==0){errors=0;return ReadStep::Data;}
        if(bytes==0)return ReadStep::Disconnect;
        if(bytes<0){
            if(error==EINTR)return ReadStep::Retry;
            if(error==EAGAIN||error==EWOULDBLOCK)return ReadStep::Wait;
            if(error==ENODEV||error==ENXIO||error==EBADF||error==EIO)return ReadStep::Disconnect;
        }
        return ++errors>=8?ReadStep::Disconnect:ReadStep::Wait;
    }
};
inline bool ValidSlot(int slot,std::size_t count){return slot>=0&&static_cast<std::size_t>(slot)<count;}
template<class Finger,std::size_t Count>
inline void Release(Finger (&fingers)[Count]) {
    for(auto& finger:fingers){finger.isDown=false;finger.isTmpDown=false;}
}
template<class Devices,class Convert>
inline spatial_input::Sample Snapshot(Devices& devices,Convert&& convert) {
    spatial_input::Sample sample;
    for(auto& device:devices)for(auto& finger:device.Finger){
        if(!finger.isDown||sample.count>=2)continue;
        sample.points[sample.count++]=convert(finger);
    }
    return sample;
}
} // namespace spatial_touch_recovery

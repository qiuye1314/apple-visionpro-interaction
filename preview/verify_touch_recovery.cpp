#include "SpatialTouchRecovery.h"
#include <array>
#include <cassert>
#include <climits>
#include <cstdio>
#include <thread>

struct Finger {bool isDown=false,isTmpDown=false;int x=0,y=0,id=0;};
struct Device {Finger Finger[10];};
static void Pass(const char* text){std::printf("PASS %s\n",text);}
int main(){
    using namespace spatial_touch_recovery;
    ReadRecovery recovery;
    for(int i=0;i<1000;++i)assert(recovery.Next(-1,EINTR,24)==ReadStep::Retry);
    assert(recovery.errors==0);
    for(int i=0;i<1000;++i)assert(recovery.Next(-1,EAGAIN,24)==ReadStep::Wait);
    assert(recovery.Next(-1,EWOULDBLOCK,24)==ReadStep::Wait&&recovery.errors==0);
    assert(PollMilliseconds>0&&PollMilliseconds<=100&&RetryMicroseconds>=1000);
    Pass("interrupted reads retry while temporarily unavailable reads require bounded waiting");
    for(int error:{ENODEV,ENXIO,EBADF,EIO})assert(recovery.Next(-1,error,24)==ReadStep::Disconnect);
    assert(recovery.Next(0,0,24)==ReadStep::Disconnect);
    Pass("EOF and removed or invalid devices terminate instead of busy polling");
    for(int i=0;i<7;++i)assert(recovery.Next(-1,EINVAL,24)==ReadStep::Wait);
    assert(recovery.Next(-1,EINVAL,24)==ReadStep::Disconnect);
    assert(recovery.Next(48,0,24)==ReadStep::Data&&recovery.errors==0);
    for(int i=0;i<7;++i)assert(recovery.Next(23,0,24)==ReadStep::Wait);
    assert(recovery.Next(23,0,24)==ReadStep::Disconnect);
    assert(recovery.Next(24,0,24)==ReadStep::Data&&recovery.errors==0);
    Pass("unknown failures and partial event packets have a finite retry budget reset by valid data");
    for(int slot:{INT_MIN,-1,10,11,INT_MAX})assert(!ValidSlot(slot,10));
    for(int slot=0;slot<10;++slot)assert(ValidSlot(slot,10));
    Pass("only in-range hardware slots can index the ten-contact device array");
    std::array<Device,2> devices{};
    devices[0].Finger[0]={true,true,100,200,1};
    devices[1].Finger[2]={true,true,300,400,22};
    auto convert=[](const Finger& finger){return spatial_input::Point{(float)finger.x,(float)finger.y,finger.id};};
    auto sample=Snapshot(devices,convert);
    assert(sample.count==2&&sample.points[0].id==1&&sample.points[1].id==22);
    Release(devices[0].Finger);
    sample=Snapshot(devices,convert);
    assert(sample.count==1&&sample.points[0].id==22&&sample.points[0].x==300&&sample.points[0].y==400);
    assert(!devices[0].Finger[0].isDown&&!devices[0].Finger[0].isTmpDown);
    assert(devices[1].Finger[2].isDown&&devices[1].Finger[2].isTmpDown);
    Pass("disconnecting one input device releases only its own contacts and retains the other device");
    std::vector<spatial_input::Sample> received;
    spatial_input::Read(received);
    std::thread input([&]{spatial_input::Publish(Snapshot(devices,convert));Release(devices[1].Finger);spatial_input::Publish(Snapshot(devices,convert));});
    input.join();
    assert(spatial_input::Read(received)&&received.size()==2);
    assert(received[0].count==1&&received[0].points[0].id==22&&received[1].count==0);
    Pass("the render input queue receives the surviving contact and final all-released event in order");
    for(auto& device:devices)for(auto& finger:device.Finger)finger={true,true,1,2,3};
    assert(Snapshot(devices,convert).count==2);
    Pass("multi-device snapshots remain bounded to the two pointers supported by the UI");
}

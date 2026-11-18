#include "SpatialMotion.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace spatial_motion;
static void Check(bool value,const char* text){if(!value){std::fprintf(stderr,"FAIL %s\n",text);std::exit(2);}}
static bool Near(float a,float b,float tolerance=.001f){return std::fabs(a-b)<tolerance;}
static Sample Reading(float x,float y,float z,float degrees,double time,int rotation=0){
    return {Axis(x,y,z,degrees*.01745329252f),time,rotation};
}
static bool requested=false;
static Status provider(bool active,Sample& sample){requested=active;sample=Reading(1,0,0,0,1);return active?Status::Ready:Status::Idle;}
static Status silentProvider(bool active,Sample&){return active?Status::Waiting:Status::Idle;}
int main(){
    Controller c;
    Check(c.Push(Reading(1,0,0,35,1),false),"first orientation accepted");
    Check(Near(c.Offset().pitch,0)&&Near(c.Offset().yaw,0),"first orientation calibrates without jumping");
    c.Push(Reading(1,0,0,45,1.1),false);Check(Near(c.Offset().pitch,-7),"relative pitch follows calibrated pose");
    std::puts("PASS calibration uses the current held orientation and maps only relative motion");
    for(int rotation=0;rotation<4;++rotation){
        c.Reset();c.Push(Reading(1,0,0,0,1,rotation),false);c.Push(Reading(1,0,0,10,1.1,rotation),false);
        const Pose p=c.Offset();
        Check(Near(p.pitch,rotation==0?-7:rotation==2?7:0),"screen rotation pitch mapping");
        Check(Near(p.yaw,rotation==1?-7:rotation==3?7:0),"screen rotation yaw mapping");
    }
    std::puts("PASS all four screen rotations remap the device axes into the displayed orientation");
    for(int sign:{-1,1}){
        c.Reset();c.Push(Reading(1,0,0,0,1),false);c.Push(Reading(1,0,0,170.f*sign,1.1),false);
        Check(std::fabs(c.Offset().pitch)<=18,"pitch limit");
        c.Reset();c.Push(Reading(0,1,0,0,1),false);c.Push(Reading(0,1,0,170.f*sign,1.1),false);
        Check(std::fabs(c.Offset().yaw)<=22,"yaw limit");
        c.Reset();c.Push(Reading(0,0,1,0,1),false);c.Push(Reading(0,0,1,170.f*sign,1.1),false);
        Check(std::fabs(c.Offset().roll)<=10,"roll limit");
    }
    std::puts("PASS large device turns remain inside the UI pitch, yaw and roll limits");
    c.Reset();c.Push(Reading(1,0,0,0,1),false);c.Push(Reading(1,0,0,10,1.1),false);
    const auto before=c.Offset();c.Push(Reading(1,0,0,25,1.2),true);c.Push(Reading(1,0,0,50,1.3),true);
    Check(Near(c.Offset().pitch,before.pitch),"held interaction freezes offset");
    c.Push(Reading(1,0,0,50,1.4),false);Check(Near(c.Offset().pitch,before.pitch),"release does not jump");
    c.Push(Reading(1,0,0,55,1.5),false);Check(Near(c.Offset().pitch,before.pitch-3.5f),"motion resumes from release baseline");
    std::puts("PASS touches freeze the pose and release resumes without accumulating the held movement");
    c.Push(Reading(1,0,0,70,3),false);Check(Near(c.Offset().pitch,0),"stale stream recalibrates");
    c.Push(Reading(1,0,0,75,3.1,1),false);Check(Near(c.Offset().pitch,0),"screen orientation change recalibrates");
    std::puts("PASS a stale stream or display rotation safely resets the motion baseline");
    const auto valid=c.Offset();
    Check(!c.Push({{0,0,0,0},3.2,1},false),"zero quaternion rejected");
    Check(!c.Push({{std::numeric_limits<float>::quiet_NaN(),0,0,1},3.2,1},false),"NaN quaternion rejected under fast-math");
    Check(!c.Push({{0,0,0,1},std::numeric_limits<double>::infinity(),1},false),"infinite timestamp rejected under fast-math");
    Check(!c.Push(Reading(1,0,0,90,3,1),false),"out of order event rejected");
    Check(Near(c.Offset().pitch,valid.pitch),"invalid input does not alter valid pose");
    std::puts("PASS malformed, nonfinite and out-of-order samples leave the last valid pose intact");
    c.Reset();c.Push(Reading(1,0,0,0,1),false);c.Push(Reading(1,0,0,10,1.1),false);const auto positive=c.Offset();
    c.Reset();c.Push(Reading(1,0,0,0,1),false);auto opposite=Reading(1,0,0,10,1.1);
    auto& q=opposite.orientation;q.x=-q.x;q.y=-q.y;q.z=-q.z;q.w=-q.w;c.Push(opposite,false);
    Check(Near(c.Offset().pitch,positive.pitch),"quaternion sign invariance");
    std::puts("PASS equivalent quaternion signs produce the same view instead of a full-turn jump");
    Integrator integration;integration.Push(0,0,0,0);
    for(int i=1;i<=60;++i)Check(integration.Push(1,0,0,i/60.),"gyro integration accepted");
    const auto q1=integration.Orientation();Check(Near(q1.x,std::sin(.5f),.002f),"angular velocity integrated to orientation");
    const auto q2=q1;integration.Push(.001f,0,0,61/60.);Check(Near(integration.Orientation().x,q2.x),"idle bias dead zone");
    Check(!integration.Push(0,0,0,1),"old gyro timestamp rejected");
    Check(!integration.Push(40,0,0,2),"invalid gyro velocity rejected");
    std::puts("PASS the raw-gyroscope fallback integrates real angular velocity and filters stationary noise");
    Install(provider);Update(false,false,1);Check(!requested,"disabled UI does not request sensors");
    Update(true,false,1);Check(requested&&Get().status==Status::Ready,"active UI requests sensors");
    Update(false,false,2);Check(!requested&&Near(Get().controller.Offset().pitch,0),"disabled UI releases and resets");
    Install(nullptr);Update(true,false,3);Check(Get().status==Status::Unavailable,"missing sensor reports unavailable");
    std::puts("PASS the provider lifecycle releases sensors when inactive and reports unavailable hardware");
    Reset();Install(silentProvider);Update(true,false,10);Update(true,false,11.9);
    Check(Get().status==Status::Waiting,"starting sensor has a bounded grace interval");
    Update(true,false,12.1);Check(Get().status==Status::Unavailable,"silent sensors time out instead of leaving an enabled dead button");
    Install(nullptr);
    std::puts("PASS a sensor that never sends data times out and returns to ordinary interaction");
    return 0;
}

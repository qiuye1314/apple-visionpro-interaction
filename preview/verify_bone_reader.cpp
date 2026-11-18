#include "DrawOnlyBoneReader.h"
#include <cassert>
#include <cstdio>
#include <map>
#include <set>
#include <vector>

namespace bones = draw_only_bones;
struct Vec3 { float X=0, Y=0, Z=0; };
struct Vec4 { float X=0, Y=0, Z=0, W=0; };
struct Transform { Vec4 Rotation; Vec3 Translation; float padding=0; Vec3 Scale3D; };
static_assert(sizeof(Transform) == 44 && offsetof(Transform, Translation) == 16 &&
              offsetof(Transform, Scale3D) == 32, "Test transform must match production payload");
static Transform Identity(Vec3 translation = {}) {
    Transform t{}; t.Rotation.W=1; t.Translation=translation; t.Scale3D={1,1,1}; return t;
}
static bool Zero(Vec3 p) { return p.X==0 && p.Y==0 && p.Z==0; }
static bool Near(float a,float b) { return std::fabs(a-b)<.01f; }
static void Pass(const char* text) { std::printf("PASS %s\n",text); }
struct Memory {
    static constexpr std::uintptr_t component=0x1000, shifted=0x2000;
    std::map<std::uintptr_t,Transform> values;
    std::set<std::uintptr_t> failures;
    std::map<std::uintptr_t,int> transientFailures;
    std::vector<std::uintptr_t> reads;
    Memory() {
        values[component]=Identity({100,200,300});
        for(int i=0;i<512;++i) values[shifted+i*bones::TransformStride]=Identity({float(i),float(i*2),float(i*3)});
    }
    bool operator()(std::uintptr_t address,void* output,std::size_t size) {
        assert(size==sizeof(Transform)); reads.push_back(address);
        const auto found=values.find(address);
        if(found==values.end()) return false;
        // Deliberately return a written but failed payload to test failure isolation.
        std::memcpy(output,&found->second,size);
        if(transientFailures[address]>0){--transientFailures[address];return false;}
        return !failures.count(address);
    }
    std::size_t Skeleton(Vec3 (&points)[17],int count=68,bones::Rig rig=bones::Rig::Standard) {
        return bones::ReadSkeleton<Transform>(component,shifted,count,bones::Mapping(count,rig),points,*this);
    }
};
int main() {
    const float nan=std::numeric_limits<float>::quiet_NaN(),inf=std::numeric_limits<float>::infinity();
    assert(!bones::Finite(nan)&&!bones::Finite(inf)&&!bones::Finite(-inf)&&bones::Finite(0));
    Transform bad{}; assert(!bones::ValidTransform(bad));
    bad=Identity(); bad.Rotation.X=nan; assert(!bones::ValidTransform(bad));
    bad=Identity(); bad.Rotation.W=0; assert(!bones::ValidTransform(bad));
    bad=Identity(); bad.Translation.Z=inf; assert(!bones::ValidTransform(bad));
    bad=Identity(); bad.Scale3D.Y=0; assert(!bones::ValidTransform(bad));
    bad=Identity(); bad.Scale3D.Z=nan; assert(!bones::ValidTransform(bad));
    assert(bones::ValidTransform(Identity()));
    Pass("bone transform finite and zero checks survive fast-math");

    Vec3 points[17]; Memory memory;
    assert(memory.Skeleton(points)==15);
    const auto map=bones::Mapping(68);
    assert(Near(points[0].X,105)&&Near(points[0].Y,210)&&Near(points[0].Z,322));
    assert(Zero(points[15])&&Zero(points[16]));
    Pass("production reader consumes shifted 48-byte array with 44-byte payloads");

    memory=Memory();
    memory.transientFailures[Memory::component]=1;
    memory.transientFailures[Memory::shifted+map[0]*bones::TransformStride]=1;
    assert(memory.Skeleton(points)==15);
    assert(Near(points[0].Z,322)&&memory.reads.size()==18);
    assert(memory.reads[0]==Memory::component&&memory.reads[1]==Memory::component);
    assert(memory.reads[2]==memory.reads[3]);
    Pass("transient component and joint transport failures recover within one retry");

    Transform sampled=Identity({9,8,7});
    memory=Memory(); memory.transientFailures[Memory::component]=3;
    assert(!bones::ReadTransform(Memory::component,sampled,memory));
    assert(memory.reads.size()==2&&memory.transientFailures[Memory::component]==1);
    assert(Zero(sampled.Translation)&&Zero(sampled.Scale3D)&&sampled.Rotation.W==0);
    Pass("two failed transform attempts clear previous output and stop before a third read");

    memory=Memory(); memory.values[Memory::component].Rotation.X=nan;
    sampled=Identity({9,8,7});
    assert(!bones::ReadTransform(Memory::component,sampled,memory));
    assert(memory.reads.size()==1&&Zero(sampled.Translation)&&sampled.Rotation.W==0);
    memory=Memory(); memory.values[Memory::component]={}; memory.transientFailures[Memory::component]=1;
    assert(!bones::ReadTransform(Memory::component,sampled,memory));
    assert(memory.reads.size()==2&&Zero(sampled.Scale3D));
    Pass("successful invalid transform payloads are cleared without additional retry");

    memory=Memory();

    memory.failures.insert(Memory::shifted+map[0]*bones::TransformStride);
    assert(memory.Skeleton(points)==14&&Zero(points[0]));
    assert(!Zero(points[1])&&!Zero(points[13])&&!Zero(points[14]));
    memory.failures.clear();
    memory.values[Memory::shifted+map[7]*bones::TransformStride]={};
    memory.values[Memory::shifted+map[8]*bones::TransformStride]=Identity({nan,0,0});
    assert(memory.Skeleton(points)==13&&Zero(points[7])&&Zero(points[8])&&!Zero(points[1]));
    Pass("failed or corrupt joint reads stay missing without collapsing other joints");

    memory=Memory();
    memory.values[Memory::shifted+map[2]*bones::TransformStride]=Identity();
    assert(memory.Skeleton(points)==15);
    assert(Near(points[2].X,100)&&Near(points[2].Y,200)&&Near(points[2].Z,300));
    Pass("valid local-origin joints retain the component world position");

    memory=Memory();
    auto& component=memory.values[Memory::component];
    component.Rotation={0,0,.70710678f,.70710678f}; component.Scale3D={2,3,4};
    memory.values[Memory::shifted+map[1]*bones::TransformStride]=Identity({10,20,30});
    assert(memory.Skeleton(points)==15);
    assert(Near(points[1].X,40)&&Near(points[1].Y,220)&&Near(points[1].Z,420));
    Pass("mesh translation rotation and nonuniform scale transform joints together");

    memory=Memory();
    memory.values[Memory::shifted+map[13]*bones::TransformStride]=Identity({5,0,-20});
    memory.values[Memory::shifted+map[14]*bones::TransformStride]=Identity({-5,0,10});
    assert(memory.Skeleton(points)==15);
    assert(Near(points[13].Z,280)&&Near(points[14].Z,310));
    memory.values[Memory::component].Rotation={0,.70710678f,0,.70710678f};
    assert(memory.Skeleton(points)==15);
    assert(Near(points[13].X,80)&&Near(points[14].X,110));
    assert(Near(points[13].Z,295)&&Near(points[14].Z,305));
    Pass("uneven terrain and prone rotation preserve independent ankle coordinates");

    memory=Memory(); memory.failures.insert(Memory::component);
    for(auto& point:points)point={9,8,7};
    assert(memory.Skeleton(points)==0);
    for(auto point:points)assert(Zero(point));
    assert(memory.reads.size()==2);
    memory.failures.clear(); memory.values[Memory::component]={}; memory.reads.clear();
    assert(memory.Skeleton(points)==0&&memory.reads.size()==1);
    for(auto point:points)assert(Zero(point));
    Pass("unavailable component clears stale joints and prevents invalid bone reads");

    memory=Memory(); bones::JointMap boundary; boundary.fill(-1);
    boundary[0]=24; boundary[1]=25; boundary[2]=26;
    assert(bones::ReadSkeleton<Transform>(Memory::component,Memory::shifted,26,boundary,points,memory)==1);
    assert(!Zero(points[0])&&Zero(points[1])&&Zero(points[2]));
    assert(memory.reads.size()==2&&memory.reads.back()==Memory::shifted+24*bones::TransformStride);
    assert(bones::ValidIndex(24,26)&&!bones::ValidIndex(25,26)&&!bones::ValidIndex(-1,26));
    Pass("shifted array rejects actual elements at and beyond the bone count");

    memory=Memory(); memory.reads.clear();
    assert(memory.Skeleton(points,14)==0&&memory.Skeleton(points,513)==0&&memory.reads.empty());
    bones::JointMap overflow; overflow.fill(-1); overflow[0]=1;
    assert(bones::ReadSkeleton<Transform>(Memory::component,
        std::numeric_limits<std::uintptr_t>::max()-16,26,overflow,points,memory)==0);
    assert(memory.reads.size()==1&&Zero(points[0]));
    Pass("invalid array counts and overflowing addresses never reach the reader");

    const auto seventyThree=bones::Mapping(73), fallback=bones::Mapping(72);
    assert(map[4]==33&&map[7]==13&&map[11]==56&&map[14]==61);
    assert(seventyThree[7]==-1&&seventyThree[8]==-1&&fallback[11]==-1);
    for(int count:{26,27,30,38,42,46,61,63,64,68,69,71,72,73,76}) {
        std::set<int> seen;
        for(int index:bones::Mapping(count)) if(index>=0) assert(seen.insert(index).second);
    }
    assert(bones::Mapping(66,bones::Rig::ClawsBoss)[3]==28);
    assert(bones::Mapping(66,bones::Rig::GuardBoss)[9]==52);
    Pass("existing 68-bone mapping is restored and ambiguous duplicate joints are isolated");

    memory=Memory(); bones::JointMap duplicate; duplicate.fill(-1); duplicate[1]=4; duplicate[3]=4;
    assert(bones::ReadSkeleton<Transform>(Memory::component,Memory::shifted,68,duplicate,points,memory)==1);
    assert(!Zero(points[1])&&Zero(points[3]));
    Pass("accidental repeated mapping indices cannot create a second joint");
    return 0;
}

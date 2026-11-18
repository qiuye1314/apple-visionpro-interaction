#include "DrawOnlyMortar.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <limits>
#include <map>
#include <utility>
#include <vector>

using namespace draw_only_mortar;
constexpr std::uintptr_t Bss = 0x7000000000ULL;
constexpr std::uintptr_t First = 0x7100000000ULL;
constexpr std::uintptr_t Second = 0x7200000000ULL;
constexpr std::uintptr_t Third = 0x7300000000ULL;
struct Memory {
    std::map<std::uintptr_t, std::vector<unsigned char>> bytes;
    std::vector<std::pair<std::uintptr_t, std::size_t>> requests;
    int failAt = 0;
    template<class Value> void Put(std::uintptr_t address, const Value& value) {
        const auto* start = reinterpret_cast<const unsigned char*>(&value);
        bytes[address] = std::vector<unsigned char>(start, start + sizeof(value));
    }
    bool operator()(std::uintptr_t address, void* output, std::size_t size) {
        requests.emplace_back(address, size);
        if (failAt == static_cast<int>(requests.size())) {
            // Model a failed/partial driver read; the caller must not publish these bytes.
            std::memset(output, 0x7f, size);
            return false;
        }
        const auto found = bytes.find(address);
        if (found == bytes.end() || found->second.size() != size) return false;
        std::memcpy(output, found->second.data(), size);
        return true;
    }
    explicit Memory(Point point = {1000,2000,10000}) {
        Put(Bss + 0x42DAA8, First);
        Put(First + 0x18, Second);
        Put(Second + 0x9C8, Third);
        Put(Third + 0xE24, point);
    }
};
static void Pass(const char* text) { std::printf("PASS %s\n", text); }
static bool Cleared(const Sample& sample) {
    return !sample.valid && sample.world.X == 0 && sample.world.Y == 0 && sample.world.Z == 0 &&
        sample.screen.X == 0 && sample.screen.Y == 0 && sample.distanceMeters == 0;
}
static Sample Previous() { return {{11,22,33},{44,55},66,true}; }
int main() {
    assert(sizeof(std::uintptr_t) == 8);
    assert(V139::迫击炮_BSS入口 == 0x42DAA8 && V139::迫击炮_链1 == 0x18 &&
        V139::迫击炮_链2 == 0x9C8 && V139::迫击炮_落点 == 0xE24);
    float matrix[16]{};
    matrix[0] = matrix[5] = matrix[11] = 1;
    const Point origin{1000,2000,0};
    Memory memory;
    Sample sample = Previous();
    assert(ReadFrame(true,true,Bss,matrix,origin,1000,500,sample,memory));
    assert(sample.valid && sample.world.X == 1000 && sample.world.Y == 2000 && sample.world.Z == 10000);
    assert(std::fabs(sample.screen.X - 1100) < .01f && std::fabs(sample.screen.Y - 400) < .01f);
    assert(std::fabs(sample.distanceMeters - 100) < .001f);
    const std::vector<std::pair<std::uintptr_t, std::size_t>> expected{
        {Bss+0x42DAA8,8}, {First+0x18,8}, {Second+0x9C8,8}, {Third+0xE24,12}
    };
    assert(memory.requests == expected);
    Pass("BSS-relative chain uses three checked pointers then exactly one packed XYZ read");
    for (int failed = 1; failed <= 4; ++failed) {
        Memory broken; broken.failAt = failed; sample = Previous();
        assert(!ReadFrame(true,true,Bss,matrix,origin,1000,500,sample,broken));
        assert(Cleared(sample) && broken.requests.size() == static_cast<std::size_t>(failed));
    }
    Pass("every failed or partial chain read clears the old marker and stops immediately");
    for (std::uintptr_t invalid : {std::uintptr_t(0),std::uintptr_t(1),First+3,AddressMask}) {
        Memory broken; broken.Put(Bss+0x42DAA8,invalid); sample = Previous();
        assert(!ReadFrame(true,true,Bss,matrix,origin,1000,500,sample,broken));
        assert(Cleared(sample) && broken.requests.size()==1);
    }
    Pass("null low and misaligned chain pointers cannot become coordinate addresses");
    for (std::uintptr_t invalid : {std::uintptr_t(0),std::uintptr_t(8),AddressMask-7}) {
        Memory broken; sample = Previous();
        assert(!ReadFrame(true,true,invalid,matrix,origin,1000,500,sample,broken));
        assert(Cleared(sample) && broken.requests.empty());
    }
    std::uintptr_t address = 123;
    assert(!AddressAt(AddressMask-7,8,12,address) && address==0);
    assert(!AddressAt(First,AddressMask,12,address) && address==0);
    Pass("invalid BSS and address overflow are rejected before driver access");
    Memory tagged;
    tagged.Put(Bss+0x42DAA8,First|0xab00000000000000ULL);
    tagged.Put(First+0x18,Second|0xcd00000000000000ULL);
    assert(ReadFrame(true,true,Bss|0xef00000000000000ULL,matrix,origin,1000,500,sample,tagged));
    assert(tagged.requests==expected && sample.valid);
    Pass("tagged ARM64 pointers retain the canonical read addresses");
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    for (Point bad : {Point{},Point{nan,1,1},Point{1,inf,1},Point{1,1,-inf},Point{1e9f,1,1}}) {
        Memory broken(bad); sample=Previous();
        assert(!ReadFrame(true,true,Bss,matrix,origin,1000,500,sample,broken));
        assert(Cleared(sample) && broken.requests.size()==4);
    }
    Pass("zero nonfinite and implausible landing coordinates are isolated under fast-math");
    for (int reason=0; reason<5; ++reason) {
        Memory broken; sample=Previous();
        float badMatrix[16]{}; std::memcpy(badMatrix,matrix,sizeof(matrix));
        if(reason==2)badMatrix[7]=nan;
        if(reason==3)std::memset(badMatrix,0,sizeof(badMatrix));
        const Point testedOrigin=reason==4?Point{nan,0,0}:origin;
        assert(!ReadFrame(reason!=0,reason!=1,Bss,badMatrix,testedOrigin,1000,500,sample,broken));
        assert(Cleared(sample) && broken.requests.empty());
    }
    Pass("disabled unavailable scenes and invalid camera or origin perform no reads");
    for (float width : {0.f,nan,inf}) {
        Memory broken; sample=Previous();
        assert(!ReadFrame(true,true,Bss,matrix,origin,width,500,sample,broken));
        assert(Cleared(sample) && broken.requests.empty());
    }
    for (Point bad : {Point{1,2,-10000},Point{1,2,.01f},Point{200000,0,10000}}) {
        Memory broken(bad); sample=Previous();
        assert(!ReadFrame(true,true,Bss,matrix,origin,1000,500,sample,broken));
        assert(Cleared(sample) && broken.requests.size()==4);
    }
    Pass("behind-camera near-plane offscreen points and invalid viewports leave no marker");
    Memory live;
    assert(ReadFrame(true,true,Bss,matrix,origin,1000,500,sample,live));
    live.requests.clear();live.failAt=4;
    assert(!ReadFrame(true,true,Bss,matrix,origin,1000,500,sample,live) && Cleared(sample));
    live.requests.clear();live.failAt=0;live.Put(Third+0xE24,Point{2000,1000,10000});
    assert(ReadFrame(true,true,Bss,matrix,origin,1000,500,sample,live));
    assert(sample.valid && sample.world.X==2000 && sample.world.Y==1000 && live.requests.size()==4);
    Pass("a failed frame removes the marker and a later valid frame uses fresh coordinates");
}

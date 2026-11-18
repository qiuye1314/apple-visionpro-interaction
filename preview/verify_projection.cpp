#include "DrawOnlyProjection.h"
#include <cassert>
#include <cstdio>
#include <limits>
#include <initializer_list>

struct World { float X, Y, Z; };
struct Screen { float X, Y; };
struct Bounds { float X, Y, Z, W; };
static void Pass(const char* text) { std::printf("PASS %s\n", text); }
int main() {
    using namespace draw_only_projection;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    assert(!Finite(nan) && !Finite(inf) && Finite(0));
    assert(!ValidWorldPoint(World{0,0,0}));
    assert(ValidWorldPoint(World{0,0,0}, false));
    assert(!ValidWorldPoint(World{nan,1,1}) && !ValidWorldPoint(World{1,inf,1}));
    Pass("world validation distinguishes missing bones from a real origin under fast-math");
    float matrix[16]{};
    matrix[0] = matrix[5] = matrix[11] = 1;
    Screen output{123,456};
    assert(Project(matrix, World{1,2,10},800,500,output));
    assert(std::fabs(output.X-880)<.001f && std::fabs(output.Y-400)<.001f);
    assert(Project(matrix, World{-12,0,10},800,500,output) && output.X < 0);
    Pass("valid and partially offscreen joints retain their perspective positions");
    for (World point : {World{0,0,0}, World{1,1,0}, World{1,1,-10},
                        World{1,1,.01f}, World{nan,1,10}, World{1,inf,10}}) {
        output = {123,456};
        assert(!Project(matrix,point,800,500,output));
        assert(output.X==0 && output.Y==0);
    }
    Pass("missing, behind-camera and near-plane joints cannot become false screen bones");
    matrix[4] = nan;
    assert(!Project(matrix,World{1,1,10},800,500,output));
    matrix[4] = 0;
    assert(!Project(matrix,World{1e8f,1,.02f},800,500,output));
    assert(!Project(matrix,World{1,1,10},inf,500,output));
    assert(!Project(matrix,World{1,1,10},0,500,output));
    Pass("corrupt matrices, extreme projections and invalid viewports are isolated");
    World joints[17]{};
    bool visible[15]{};
    int traces = 0;
    const auto trace = [&](const World& point) { ++traces; return point.Z > 5; };
    assert(ResolveVisibility(joints,World{10,20,10},false,visible,trace));
    assert(traces==0);
    for (bool item : visible) assert(item);
    assert(ResolveVisibility(joints,World{10,20,10},true,visible,trace));
    assert(traces==1);
    for (bool item : visible) assert(!item);
    Pass("missing skeleton uses one root visibility sample without inventing exposed joints");
    joints[1]={10,20,3}; joints[7]={10,20,10}; joints[0]={nan,0,0}; traces=0;
    assert(!ResolveVisibility(joints,World{10,20,10},true,visible,trace));
    assert(traces==2 && !visible[0] && !visible[1] && visible[7]);
    joints[2]={10,20,10};
    assert(ResolveVisibility(joints,World{10,20,1},true,visible,trace));
    assert(!visible[0] && !visible[1] && visible[2]);
    Pass("partial cover keeps per-joint colors and only valid torso samples expose the actor");

    float upright[16]{}; upright[0]=upright[7]=upright[9]=1.f;
    Bounds bounds{};
    assert(ProjectActorBounds(upright,World{0,1000,0},800,500,bounds));
    assert(std::fabs(bounds.X-766.6667f)<.002f && std::fabs(bounds.Y-502.6042f)<.002f);
    assert(std::fabs(bounds.Z-66.6667f)<.002f && std::fabs(bounds.W-109.375f)<.002f);
    Pass("standing actor bounds match the perspective projection of all eight world corners");
    float overhead[16]{}; overhead[0]=overhead[5]=1.f; overhead[11]=-1.f; overhead[15]=1000.f;
    assert(overhead[9]==0.f && ProjectActorBounds(overhead,World{0,0,0},800,500,bounds));
    assert(bounds.X<800 && bounds.X+bounds.Z>800 && bounds.Y-bounds.W<500 && bounds.Y>500);
    assert(std::fabs(bounds.Z-80.5031f)<.002f && std::fabs(bounds.W-50.3145f)<.002f);
    upright[0]=-1.f; upright[9]=-1.f;
    assert(ProjectActorBounds(upright,World{0,1000,0},800,500,bounds));
    assert(bounds.Z>0.f && bounds.W>0.f && std::fabs(bounds.W-109.375f)<.002f);
    assert(std::fabs(bounds.Y-606.7708f)<.002f);
    upright[0]=upright[9]=1.f;
    Pass("overhead m9=0 and upside-down negative-m9 views retain positive actor bounds");
    assert(ProjectActorBounds(upright,World{-1000,1000,0},800,500,bounds));
    assert(bounds.X<0.f && bounds.X+bounds.Z>0.f);
    assert(ProjectActorBounds(upright,World{0,100000,0},800,500,bounds));
    assert(bounds.Z>=3.f && bounds.W>=8.f);
    Pass("partially offscreen actors and distant finite volumes retain usable fallback bounds");
    for (World root : {World{0,-1000,0},World{0,0,0},World{0,.01f,0},
                       World{0,.005f,0},World{0,20,0},World{0,50,0},
                       World{nan,1000,0},World{0,inf,0},World{1.e9f,1000,0},World{1.e8f,1000,0}}) {
        bounds={1,2,3,4};
        assert(!ProjectActorBounds(upright,root,800,500,bounds));
        assert(bounds.X==0.f && bounds.Y==0.f && bounds.Z==0.f && bounds.W==0.f);
    }
    Pass("rear actors, near-plane explosions and malformed or fully offscreen roots clear fallback bounds");
    upright[4]=nan;
    assert(!ProjectActorBounds(upright,World{0,1000,0},800,500,bounds));
    upright[4]=0.f;
    assert(!ProjectActorBounds(upright,World{0,1000,0},inf,500,bounds));
    assert(!ProjectActorBounds(upright,World{0,1000,0},0,500,bounds));
    assert(!ProjectActorBounds(upright,World{0,1000,0},40000,500,bounds));
    assert(!ProjectActorBounds(nullptr,World{0,1000,0},800,500,bounds));
    Pass("actor fallback rejects invalid matrices and viewports before projection");
}

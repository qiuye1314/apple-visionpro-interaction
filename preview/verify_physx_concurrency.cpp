// Runs the production VisibleScene wrapper against an instrumented Embree ABI.
// No Android driver, game process, or production executable is accessed.
#include "DrawOnlyVisibleScene.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <future>
#include <limits>
#include <thread>

using namespace std::chrono_literals;
struct Gate {
    std::mutex mutex;
    std::condition_variable changed;
    bool enabled = false, released = false;
    int entered = 0;
    void Enable() {
        std::lock_guard<std::mutex> lock(mutex);
        enabled = true; released = false; entered = 0;
    }
    void Enter() {
        std::unique_lock<std::mutex> lock(mutex);
        if (!enabled) return;
        ++entered; changed.notify_all();
        changed.wait(lock, [&] { return released; });
    }
    bool Wait(int expected) {
        std::unique_lock<std::mutex> lock(mutex);
        return changed.wait_for(lock, 2s, [&] { return entered >= expected; });
    }
    void Release() {
        std::lock_guard<std::mutex> lock(mutex);
        released = true; enabled = false; changed.notify_all();
    }
};
struct RTCDeviceTy {};
struct RTCGeometryTy {
    RTCScene owner = nullptr;
    bool enabled = true;
    int references = 1;
    void* data = nullptr;
    std::vector<float> vertices;
    std::vector<uint32_t> indices;
};
struct RTCSceneTy {
    std::vector<RTCGeometry> geometries;
    std::atomic<int> readers{0}, commits{0};
    std::atomic<bool> committing{false};
    Gate commitGate, rayGate;
};
static RTCScene latestScene = nullptr;
static std::atomic<bool> failBuffer{false};
static void Mutate(RTCScene scene) {
    if (scene) assert(scene->readers.load() == 0 && !scene->committing.load());
}
extern "C" {
RTCDevice rtcNewDevice(const char*) { return new RTCDeviceTy; }
void rtcReleaseDevice(RTCDevice device) { delete device; }
void rtcSetDeviceErrorFunction(RTCDevice, RTCErrorFunction, void*) {}
RTCScene rtcNewScene(RTCDevice) { latestScene = new RTCSceneTy; return latestScene; }
void rtcSetSceneBuildQuality(RTCScene scene, RTCBuildQuality) { Mutate(scene); }
void rtcSetSceneFlags(RTCScene scene, RTCSceneFlags) { Mutate(scene); }
void rtcReleaseGeometry(RTCGeometry geometry) {
    if (--geometry->references == 0) delete geometry;
}
void rtcReleaseScene(RTCScene scene) {
    Mutate(scene);
    for (auto geometry : scene->geometries) rtcReleaseGeometry(geometry);
    delete scene;
}
void rtcCommitScene(RTCScene scene) {
    Mutate(scene); scene->committing.store(true);
    scene->commitGate.Enter();
    ++scene->commits; scene->committing.store(false);
}
RTCGeometry rtcNewGeometry(RTCDevice, RTCGeometryType) { return new RTCGeometryTy; }
RTCGeometry rtcGetGeometry(RTCScene scene, unsigned int id) {
    assert(id < scene->geometries.size()); return scene->geometries[id];
}
void rtcEnableGeometry(RTCGeometry geometry) { Mutate(geometry->owner); geometry->enabled = true; }
void rtcDisableGeometry(RTCGeometry geometry) { Mutate(geometry->owner); geometry->enabled = false; }
void rtcSetGeometryUserData(RTCGeometry geometry, void* data) { Mutate(geometry->owner); geometry->data = data; }
void* rtcSetNewGeometryBuffer(RTCGeometry geometry, RTCBufferType type, unsigned int,
                              RTCFormat, size_t stride, size_t count) {
    Mutate(geometry->owner);
    if (failBuffer.exchange(false)) return nullptr;
    assert(stride == 12);
    if (type == RTC_BUFFER_TYPE_VERTEX) {
        geometry->vertices.resize(count * 3); return geometry->vertices.data();
    }
    assert(type == RTC_BUFFER_TYPE_INDEX);
    geometry->indices.resize(count * 3); return geometry->indices.data();
}
void rtcCommitGeometry(RTCGeometry geometry) { Mutate(geometry->owner); }
unsigned int rtcAttachGeometry(RTCScene scene, RTCGeometry geometry) {
    Mutate(scene); geometry->owner = scene; ++geometry->references;
    scene->geometries.push_back(geometry); return static_cast<unsigned int>(scene->geometries.size() - 1);
}
void rtcIntersect1(RTCScene scene, RTCIntersectContext*, RTCRayHit* hit) {
    assert(!scene->committing.load()); ++scene->readers;
    scene->rayGate.Enter();
    assert(!scene->committing.load());
    for (unsigned int i = 0; i < scene->geometries.size(); ++i) {
        const auto geometry = scene->geometries[i];
        if (!geometry->enabled) continue;
        assert(geometry->data && !geometry->vertices.empty());
        hit->hit.geomID = i;
        hit->ray.tfar = geometry->vertices.front();
        break;
    }
    --scene->readers;
}
}

struct Point { float x, y, z; };
struct Mesh {
    int key = 0, generation = 0;
    std::vector<Point> Vertices;
    std::vector<uint32_t> Indices;
    struct { Point p{}; } Transform;
};
using Scene = draw_only_scene::VisibleScene<int, std::hash<int>, Mesh>;
static int Key(const Mesh& mesh) { return mesh.key; }
static Mesh Triangle(int key, int generation) {
    return {key, generation, {{static_cast<float>(generation),0,0},{0,1,0},{0,0,1}}, {0,1,2}, {}};
}
static const Point origin{0,0,0}, target{0,0,10000};
static void Pass(const char* message) { std::printf("PASS %s\n", message); }

int main() {
    Scene scene(Key);
    RTCScene native = latestScene;
    scene.UpdateMesh({Triangle(1,10)}, {});
    Scene::MeshSnapshot first;
    const auto hit = scene.Raycast(origin, target, &first);
    assert(hit.hit.geomID == 0 && first && first->generation == 10 && hit.ray.tfar == 10);
    Pass("ray and owning mesh snapshot are captured from the same scene generation");

    std::weak_ptr<const Mesh> old = first;
    scene.UpdateMesh({Triangle(2,20)}, {1});
    Scene::MeshSnapshot second;
    const auto replaced = scene.Raycast(origin, target, &second);
    assert(replaced.hit.geomID == hit.hit.geomID && second->generation == 20);
    assert(first->generation == 10 && !old.expired());
    first.reset(); assert(old.expired());
    Pass("removed mesh survives geometry ID reuse until its last reader releases it");

    auto listing = scene.GetMeshDatas();
    struct Position { float X, Y, Z; } position{};
    auto nearby = scene.GetNearMesh(position, 1.0);
    auto direct = scene.GetGeomeoryData(replaced.hit.geomID);
    scene.UpdateMesh({Triangle(3,30)}, {2});
    assert(listing.size() == 1 && nearby.size() == 1 && direct);
    assert(listing[0]->generation == 20 && nearby[0]->generation == 20 && direct->generation == 20);
    Pass("list near-mesh and direct lookups return immutable owning snapshots");

    native->commitGate.Enable();
    auto writer = std::async(std::launch::async, [&] { scene.UpdateMesh({Triangle(3,31)}, {}); });
    assert(native->commitGate.Wait(1));
    std::promise<void> readStarted;
    auto pendingRead = std::async(std::launch::async, [&] {
        readStarted.set_value(); Scene::MeshSnapshot value;
        const auto ray = scene.Raycast(origin, target, &value); return std::make_pair(ray, value);
    });
    readStarted.get_future().wait();
    const bool readBlocked = pendingRead.wait_for(25ms) == std::future_status::timeout;
    auto uiRead = std::async(std::launch::async, [&] {
        Scene::MeshSnapshot value = direct;
        const auto ray = scene.TryRaycast(origin, target, &value);
        return std::make_pair(ray, value);
    });
    const bool uiReturnedWhileCommitting = uiRead.wait_for(1s) == std::future_status::ready;
    const bool writerStillHeld = writer.wait_for(0ms) == std::future_status::timeout && native->committing.load();
    native->commitGate.Release(); writer.get();
    const auto afterCommit = pendingRead.get();
    assert(readBlocked && afterCommit.second->generation == 31 && afterCommit.first.ray.tfar == 31);
    Pass("queries wait until geometry mutation and scene commit both finish");
    const auto busyRead = uiRead.get();
    assert(uiReturnedWhileCommitting && writerStillHeld);
    assert(busyRead.first.status == draw_only_scene::RayStatus::Busy && !busyRead.second);
    assert(busyRead.first.hit.hit.geomID == RTC_INVALID_GEOMETRY_ID);
    Pass("UI query returns Busy and clears the old hit while the writer is still held in commit");
    Scene::MeshSnapshot uiRecovered;
    const auto recoveredRead = scene.TryRaycast(origin, target, &uiRecovered);
    assert(recoveredRead.status == draw_only_scene::RayStatus::Hit && uiRecovered);
    assert(uiRecovered->generation == 31 && recoveredRead.hit.ray.tfar == 31);
    Pass("UI query resumes with the newly committed ray and mesh on the following frame");

    native->rayGate.Enable();
    auto readerA = std::async(std::launch::async, [&] { return scene.Raycast(origin, target); });
    auto readerB = std::async(std::launch::async, [&] { return scene.Raycast(origin, target); });
    const bool concurrentReads = native->rayGate.Wait(2);
    native->rayGate.Release(); readerA.get(); readerB.get();
    assert(concurrentReads);
    Pass("two ray readers can enter the same committed scene concurrently");

    native->rayGate.Enable();
    auto heldRead = std::async(std::launch::async, [&] {
        Scene::MeshSnapshot value; const auto ray = scene.Raycast(origin, target, &value);
        return std::make_pair(ray, value);
    });
    assert(native->rayGate.Wait(1));
    std::promise<void> writeStarted;
    auto pendingWrite = std::async(std::launch::async, [&] {
        writeStarted.set_value(); scene.UpdateMesh({Triangle(4,40)}, {3});
    });
    writeStarted.get_future().wait();
    const bool writeBlocked = pendingWrite.wait_for(25ms) == std::future_status::timeout;
    native->rayGate.Release();
    const auto beforeReplace = heldRead.get(); pendingWrite.get();
    assert(writeBlocked && beforeReplace.first.ray.tfar == 31 && beforeReplace.second->generation == 31);
    Pass("writers wait for in-flight rays and cannot recycle IDs before snapshot capture");

    Scene independent(Key); independent.UpdateMesh({Triangle(9,90)}, {});
    native->commitGate.Enable();
    auto heldWriter = std::async(std::launch::async, [&] { scene.UpdateMesh({Triangle(4,41)}, {}); });
    assert(native->commitGate.Wait(1));
    auto otherRead = std::async(std::launch::async, [&] { return independent.Raycast(origin, target); });
    const bool independentReady = otherRead.wait_for(1s) == std::future_status::ready;
    native->commitGate.Release(); heldWriter.get();
    assert(otherRead.get().ray.tfar == 90 && independentReady);
    Pass("an update locks only its own scene and never a global scene or driver lock");

    const int commits = native->commits.load();
    failBuffer.store(true);
    scene.UpdateMesh({Triangle(4,42)}, {});
    Scene::MeshSnapshot missing = beforeReplace.second;
    const auto failed = scene.Raycast(origin, target, &missing);
    assert(failed.hit.geomID == RTC_INVALID_GEOMETRY_ID && !missing);
    assert(native->commits.load() == commits + 1 && beforeReplace.second->generation == 31);
    scene.UpdateMesh({Triangle(4,43)}, {});
    assert(scene.GetMeshDatas().size() == 1);
    Pass("buffer failures commit removal and clear hits while retained snapshots stay valid");

    Scene::MeshSnapshot invalid = beforeReplace.second;
    assert(scene.Raycast(origin, origin, &invalid).hit.geomID == RTC_INVALID_GEOMETRY_ID && !invalid);
    Point nanPoint{std::numeric_limits<float>::quiet_NaN(), 0, 0};
    assert(scene.Raycast(nanPoint, target).hit.geomID == RTC_INVALID_GEOMETRY_ID);
    Pass("zero-length and invalid rays do not enter Embree or retain an old hit");
    const auto invalidUi = scene.TryRaycast(nanPoint, target, &invalid);
    assert(invalidUi.status == draw_only_scene::RayStatus::Invalid && !invalid);
    Scene emptyScene(Key);
    const auto clearUi = emptyScene.TryRaycast(origin, target);
    assert(clearUi.status == draw_only_scene::RayStatus::Miss);
    using draw_only_scene::RayStatus;
    using draw_only_scene::CombineRayStatus;
    assert(CombineRayStatus(RayStatus::Miss, RayStatus::Miss) == RayStatus::Miss);
    assert(CombineRayStatus(RayStatus::Miss, RayStatus::Busy) == RayStatus::Busy);
    assert(CombineRayStatus(RayStatus::Busy, RayStatus::Miss) == RayStatus::Busy);
    assert(CombineRayStatus(RayStatus::Miss, RayStatus::Invalid) == RayStatus::Invalid);
    assert(CombineRayStatus(RayStatus::Busy, RayStatus::Hit) == RayStatus::Hit);
    Pass("busy and invalid scene queries cannot combine into confirmed clear visibility");

    std::atomic<bool> done{false};
    std::vector<std::thread> readers;
    for (int i = 0; i < 3; ++i) readers.emplace_back([&] {
        while (!done.load()) {
            Scene::MeshSnapshot mesh; const auto ray = scene.Raycast(origin, target, &mesh);
            assert(mesh && ray.hit.geomID != RTC_INVALID_GEOMETRY_ID && ray.ray.tfar == mesh->generation);
        }
    });
    for (int version = 44; version < 244; ++version) scene.UpdateMesh({Triangle(4,version)}, {});
    done.store(true);
    for (auto& reader : readers) reader.join();
    assert(scene.GetMeshDatas().size() == 1);
    Pass("repeated concurrent replacement preserves ray-mesh consistency without duplicate geometry");
}

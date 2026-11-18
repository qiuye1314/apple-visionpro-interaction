#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <unordered_map>
#include <vector>
#include "embree3/rtcore.h"

namespace draw_only_scene {

enum class RayStatus { Invalid, Busy, Miss, Hit };
struct RayQuery {
    RayStatus status = RayStatus::Invalid;
    RTCRayHit hit{};
};

// A confirmed obstruction is authoritative. A missing/busy scene can never
// turn an otherwise incomplete query into confirmed clear line of sight.
inline RayStatus CombineRayStatus(RayStatus first, RayStatus second) {
    if (first == RayStatus::Hit || second == RayStatus::Hit) return RayStatus::Hit;
    if (first == RayStatus::Invalid || second == RayStatus::Invalid) return RayStatus::Invalid;
    if (first == RayStatus::Busy || second == RayStatus::Busy) return RayStatus::Busy;
    return RayStatus::Miss;
}

inline void EmbreeError(void*, RTCError code, const char* text) {
    std::printf("[DEBUG] Embree Error [%d]: %s\n", code, text);
}

// One lock per scene: mutation/commit is exclusive; tracing is shared. Mesh
// snapshots own their data after the lock is released or a geometry ID is reused.
template<class Key, class Hash, class Mesh>
class VisibleScene {
public:
    using MeshSnapshot = std::shared_ptr<const Mesh>;
    using KeyExtractor = Key (*)(const Mesh&);

    explicit VisibleScene(KeyExtractor extractor) : getKey_(extractor) {
        device_ = rtcNewDevice(nullptr);
        rtcSetDeviceErrorFunction(device_, EmbreeError, nullptr);
        scene_ = rtcNewScene(device_);
        rtcSetSceneBuildQuality(scene_, RTC_BUILD_QUALITY_LOW);
        rtcSetSceneFlags(scene_, RTC_SCENE_FLAG_DYNAMIC);
        rtcCommitScene(scene_);
    }
    ~VisibleScene() {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        if (scene_) rtcReleaseScene(scene_);
        if (device_) rtcReleaseDevice(device_);
    }
    VisibleScene(const VisibleScene&) = delete;
    VisibleScene& operator=(const VisibleScene&) = delete;

    void UpdateMesh(const std::vector<Mesh>& additions, const std::set<Key>& removals) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        for (const auto& key : removals) {
            const auto found = geometryIds_.find(key);
            if (found == geometryIds_.end()) continue;
            const uint32_t id = found->second;
            RTCGeometry geometry = rtcGetGeometry(scene_, id);
            rtcDisableGeometry(geometry);
            rtcSetGeometryUserData(geometry, nullptr);
            disabledIds_.insert(id);
            geometryIds_.erase(found);
            meshes_.erase(id);
        }

        for (const auto& mesh : additions) {
            if (mesh.Vertices.empty() || mesh.Indices.empty() || mesh.Indices.size() % 3 != 0) continue;
            const Key key = getKey_(mesh);
            const auto active = geometryIds_.find(key);
            const bool replace = active != geometryIds_.end();
            const bool create = !replace && disabledIds_.empty();
            uint32_t id = RTC_INVALID_GEOMETRY_ID;
            RTCGeometry geometry = nullptr;
            if (replace) {
                id = active->second;
                geometry = rtcGetGeometry(scene_, id);
            } else if (!create) {
                id = *disabledIds_.begin();
                disabledIds_.erase(disabledIds_.begin());
                geometry = rtcGetGeometry(scene_, id);
                rtcEnableGeometry(geometry);
            } else {
                geometry = rtcNewGeometry(device_, RTC_GEOMETRY_TYPE_TRIANGLE);
            }

            const auto discard = [&] {
                if (create) {
                    rtcReleaseGeometry(geometry);
                } else {
                    rtcDisableGeometry(geometry);
                    rtcSetGeometryUserData(geometry, nullptr);
                    disabledIds_.insert(id);
                    meshes_.erase(id);
                    if (replace) geometryIds_.erase(key);
                }
            };
            auto copy = std::make_shared<Mesh>(mesh);
            auto* vertices = static_cast<float*>(rtcSetNewGeometryBuffer(geometry,
                RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, 3 * sizeof(float), mesh.Vertices.size()));
            if (!vertices) { discard(); break; }
            for (size_t i = 0; i < mesh.Vertices.size(); ++i) {
                vertices[i * 3] = mesh.Vertices[i].x;
                vertices[i * 3 + 1] = mesh.Vertices[i].y;
                vertices[i * 3 + 2] = mesh.Vertices[i].z;
            }
            auto* indices = static_cast<unsigned int*>(rtcSetNewGeometryBuffer(geometry,
                RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, 3 * sizeof(unsigned int), mesh.Indices.size() / 3));
            if (!indices) { discard(); break; }
            std::memcpy(indices, mesh.Indices.data(), mesh.Indices.size() * sizeof(uint32_t));
            rtcSetGeometryUserData(geometry, copy.get());
            rtcCommitGeometry(geometry);
            if (create) {
                id = rtcAttachGeometry(scene_, geometry);
                rtcReleaseGeometry(geometry);
                if (id == RTC_INVALID_GEOMETRY_ID) continue;
            }
            geometryIds_[key] = id;
            meshes_[id] = std::move(copy);
        }
        rtcCommitScene(scene_);
    }

    // The optional snapshot is captured under the same lock as rtcIntersect1,
    // so a writer cannot recycle the hit ID before its mesh is retained.
    template<class Point>
    RTCRayHit Raycast(const Point& origin, const Point& target, MeshSnapshot* mesh = nullptr) const {
        if (mesh) mesh->reset();
        RTCRayHit hit = EmptyHit();
        if (!PrepareRay(origin, target, hit)) return hit;
        std::shared_lock<std::shared_mutex> lock(mutex_);
        TraceLocked(hit, mesh);
        return hit;
    }

    // UI callers must not wait for mesh copies, BVH rebuilds, or scene commits.
    // Busy is distinct from Miss so it cannot be displayed as clear visibility.
    template<class Point>
    RayQuery TryRaycast(const Point& origin, const Point& target, MeshSnapshot* mesh = nullptr) const {
        if (mesh) mesh->reset();
        RayQuery result;
        result.hit = EmptyHit();
        if (!PrepareRay(origin, target, result.hit)) return result;
        std::shared_lock<std::shared_mutex> lock(mutex_, std::try_to_lock);
        if (!lock.owns_lock()) { result.status = RayStatus::Busy; return result; }
        TraceLocked(result.hit, mesh);
        result.status = result.hit.hit.geomID == RTC_INVALID_GEOMETRY_ID ? RayStatus::Miss : RayStatus::Hit;
        return result;
    }

    MeshSnapshot GetGeomeoryData(uint32_t id) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        const auto found = meshes_.find(id);
        return found == meshes_.end() ? MeshSnapshot{} : found->second;
    }

    std::vector<MeshSnapshot> GetMeshDatas() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        std::vector<MeshSnapshot> result;
        result.reserve(meshes_.size());
        for (const auto& entry : meshes_) result.push_back(entry.second);
        return result;
    }

    template<class Position>
    std::vector<MeshSnapshot> GetNearMesh(const Position& position, double radiusSquared) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        std::vector<MeshSnapshot> result;
        for (const auto& entry : meshes_) {
            const auto& mesh = entry.second;
            const double dx = mesh->Transform.p.x - position.X;
            const double dy = mesh->Transform.p.y - position.Y;
            if (dx * dx + dy * dy <= radiusSquared) result.push_back(mesh);
        }
        return result;
    }

private:
    static RTCRayHit EmptyHit() {
        RTCRayHit hit{};
        hit.hit.geomID = RTC_INVALID_GEOMETRY_ID;
        hit.hit.primID = RTC_INVALID_GEOMETRY_ID;
        return hit;
    }
    template<class Point>
    static bool PrepareRay(const Point& origin, const Point& target, RTCRayHit& hit) {
        const float ox = origin.x, oy = origin.y, oz = origin.z;
        const float dx = target.x - ox, dy = target.y - oy, dz = target.z - oz;
        const float length = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (!Finite(ox) || !Finite(oy) || !Finite(oz) || !Finite(dx) || !Finite(dy) ||
            !Finite(dz) || !Finite(length) || length <= 0.0001f) return false;
        hit.ray.org_x = ox; hit.ray.org_y = oy; hit.ray.org_z = oz;
        hit.ray.dir_x = dx / length; hit.ray.dir_y = dy / length; hit.ray.dir_z = dz / length;
        hit.ray.tnear = 0.0f; hit.ray.tfar = length; hit.ray.mask = -1;
        return true;
    }
    void TraceLocked(RTCRayHit& hit, MeshSnapshot* mesh) const {
        RTCIntersectContext context;
        rtcInitIntersectContext(&context);
        rtcIntersect1(scene_, &context, &hit);
        if (mesh && hit.hit.geomID != RTC_INVALID_GEOMETRY_ID) {
            const auto found = meshes_.find(hit.hit.geomID);
            if (found != meshes_.end()) *mesh = found->second;
        }
    }
    static bool Finite(float value) {
        uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        return (bits & 0x7f800000u) != 0x7f800000u;
    }
    mutable std::shared_mutex mutex_;
    RTCDevice device_ = nullptr;
    RTCScene scene_ = nullptr;
    std::unordered_map<Key, uint32_t, Hash> geometryIds_;
    std::set<uint32_t> disabledIds_;
    std::unordered_map<uint32_t, MeshSnapshot> meshes_;
    KeyExtractor getKey_;
};
} // namespace draw_only_scene

//
// Created by 王 on 2025/12/7.
//

#include <thread>
#include <cstdio>
#include "ReadGame.h"
#include "版本数据_1_39.h"   /* 【1.39】所有 libUE4 偏移集中在此 */
#include "PhysX.h"
#include "Draw.h"
#include "VectorHelper.h"
FGameData GameData;

MinimalViewInfo POV;

/* 【性能】PhysX 扫描线程启动标志（按需启动用） */
static bool g_physX线程已启动 = false;
static draw_only_physx::RetryCooldown g_physXRetry;

void ReadGame::Raycast()
{
    /* 【正确逻辑】准心射线命中【哪一个物体】，就把那个物体的【全部三角形】绘制出来。
       - 不是"只画命中点附近一小块"（那样是残缺的）
       - 也不是"画附近所有物体"（那样全屏都是绿的）
       - 而是：命中的那个 mesh，整块画完。 */

    // Keep the mesh alive while the worker may replace its scene geometry.
    const auto mesh_ptr = LineTrace::getNextHint(POV.Rotation, POV.Location);

    if (mesh_ptr == nullptr) {
        return;   // 没命中任何物体 → 什么都不画
    }

    const TriangleMeshData& mesh = *mesh_ptr;

    try {
        if (!mesh.Vertices.empty() && !mesh.Indices.empty() &&
            static_cast<uint32_t>(mesh.Type) < 10 &&
            mesh.Vertices.size() < 1000000 &&
            mesh.Indices.size() < 1000000)
        {
            // Wireframe strokes emit more than three ImGui vertices each.
            // Bound attempted work even when every sampled triangle is invalid
            // or offscreen; leave foreground vertex capacity for the panel.
            draw_only_physx::TriangleBudget budget;
            auto* drawList = ImGui::GetForegroundDrawList();
            const size_t 总三角形 = mesh.Indices.size() / 3;
            const size_t 步长 = draw_only_physx::TriangleBudget::Stride(总三角形);

            for (size_t i = 0; i + 2 < mesh.Indices.size() && budget.Check(); i += 3 * 步长) {
                const uint32_t idx0 = mesh.Indices[i];
                const uint32_t idx1 = mesh.Indices[i + 1];
                const uint32_t idx2 = mesh.Indices[i + 2];
                if (idx0 >= mesh.Vertices.size() || idx1 >= mesh.Vertices.size() ||
                    idx2 >= mesh.Vertices.size()) continue;

                Vector3 v0 = LineTrace::ToVec3(mesh.Vertices[idx0]);
                Vector3 v1 = LineTrace::ToVec3(mesh.Vertices[idx1]);
                Vector3 v2 = LineTrace::ToVec3(mesh.Vertices[idx2]);

                auto p0 = VectorHelper::WorldToScreen((FVector&)v0, POV);
                auto p1 = VectorHelper::WorldToScreen((FVector&)v1, POV);
                auto p2 = VectorHelper::WorldToScreen((FVector&)v2, POV);
                if (!draw_only_scene_bounds::ProjectedTriangleFinite(p0, p1, p2)) continue;

                if (VectorHelper::IsInScreen(p0) || VectorHelper::IsInScreen(p1) ||
                    VectorHelper::IsInScreen(p2)) {
                    if (sizeof(ImDrawIdx) == 2 && !(drawList->Flags & ImDrawListFlags_AllowVtxOffset) &&
                        drawList->_VtxCurrentIdx + 12 >= 65536 - 4096) break;
                    drawList->AddTriangle(
                        ImVec2(p0.X, p0.Y), ImVec2(p1.X, p1.Y), ImVec2(p2.X, p2.Y),
                        ImColor(0, 255, 0, 255), 0.3f);
                }
            }
        }
    }
    catch (...) { }

}

bool CustomLineTrace(const Vector3& location, const Vector3& coord){
    const auto result = LineTrace::LineTraceSingle(
        FVector(static_cast<float>(location.x), static_cast<float>(location.y), static_cast<float>(location.z)),
        FVector(static_cast<float>(coord.x), static_cast<float>(coord.y), static_cast<float>(coord.z))
    );
    

    
    return result == draw_only_scene::RayStatus::Miss;
}
/* 【恢复原版】启动 3 个扫描线程（现在有 -O2 优化，不再拖累帧率） */
void StartLoadMapModel() {
    if (g_physX线程已启动) return;

    DynamicLoadScene = new Physics::VisibleScene<PrunerPayload, PrunerPayloadHash>(Physics::prunerPayloadExtractor);
    HeightFieldScene = new Physics::VisibleScene<uint64_t, Int64Hash>(Physics::int64Extractor);
    DynamicRigidScene = new Physics::VisibleScene<PrunerPayload, PrunerPayloadHash>(Physics::prunerPayloadExtractor);

    std::thread LoadDynamicHeightFieldThread(VisibleCheck::UpdateDynamicHeightField);
    std::thread LoadDynamicRigidThread(VisibleCheck::UpdateDynamicRigid);
    std::thread LoadSceneByRangeThread(VisibleCheck::UpdateSceneByRange);
    LoadDynamicHeightFieldThread.detach();
    LoadDynamicRigidThread.detach();
    LoadSceneByRangeThread.detach();

    g_physX线程已启动 = true;
}


static bool RefreshPhysXInstance() {
    PhysxInstancePtr = 0;
    target_pid = ReadProcess("com.tencent.tmgp.pubgmhd");
    GameData.UE4 = target_pid > 0 ? ReadModule("libUE4.so") : 0;
    uintptr_t instance = 0;
    if (!draw_only_memory::ReadPointer(GameData.UE4, OffsetPhysxInstancePtr, instance) ||
        instance < 0x10000000) {
        return false;
    }
    PhysxInstancePtr = instance;
    return true;
}

void ReadGame::Initialization() {
    // Explicit initialization refreshes the source even if workers already
    // exist, so reconnecting after a game restart keeps its previous behavior.
    g_physXRetry.RecordAttempt(draw_only_physx::NowMilliseconds());
    if (RefreshPhysXInstance()) StartLoadMapModel();
}

/* 【新增】按需启动 PhysX 扫描线程（面板"开启PhysX"/"掩体漏打"打开时调用） */
void ReadGame::EnsurePhysXStarted() {
    if (g_physX线程已启动 && PhysxInstancePtr >= 0x10000000) return;
    if (!g_physXRetry.Allow(draw_only_physx::NowMilliseconds())) return;
    if (RefreshPhysXInstance()) StartLoadMapModel();
}

void ReadGame::ReadGameData() {
    /* 【1.39 版本数据】
         世界：GWorld = libue4 + 0x181BF200
         自身：UWorld +0xB20 GameInstance -> +0x48 LocalPlayers[0] -> +0x30 PlayerController -> +0x36D0 Pawn
               （速查表链 UWorld+0xC0 -> +0x88 -> +0x30 -> +0x36D0 作兜底）
         相机：Pawn +0x608(Controller) -> +0x680 PlayerCameraManager -> +0x650 MinimalViewInfo
         这样 POV.Location 与绘制用的 PovLocation 完全一致。 */
    GameData.UWorld = read<uintptr_t>(GameData.UE4 + V139::GWorld基址);
    if (GameData.UWorld == 0) return;

    uintptr_t self = 0;
    {
        uintptr_t 实例 = read<uintptr_t>(GameData.UWorld + V139::世界_游戏实例);
        if (实例) {
            uintptr_t 本地玩家 = read<uintptr_t>(read<uintptr_t>(实例 + V139::实例_本地玩家));
            if (本地玩家) {
                uintptr_t 控制器A = read<uintptr_t>(本地玩家 + V139::本地玩家_控制器);
                if (控制器A) self = read<uintptr_t>(控制器A + V139::自身_人物);
            }
        }
        if (self == 0) {
            self = read<uintptr_t>(
                read<uintptr_t>(
                    read<uintptr_t>(
                        read<uintptr_t>(GameData.UWorld + V139::自身1) + V139::自身2) + V139::自身3) + V139::自身_人物);
        }
    }
    if (self == 0) return;

    uintptr_t 控制器 = read<uintptr_t>(self + V139::自身_控制器);
    if (控制器 == 0) 控制器 = read<uintptr_t>(self + V139::自身_相机管理);
    if (控制器 == 0) return;

    auto camMgr = read<uintptr_t>(控制器 + V139::控制器_相机管理);
    if (camMgr == 0) return;

    GetAddress(camMgr + V139::相机_PovLocation, &POV, sizeof(MinimalViewInfo));

    /* 【诊断】每约3秒报一次，确认 POV 已填充 */
    static int cnt = 0;
    if ((++cnt % 180) == 1) {
        printf("[Ray] POV 更新: Loc=(%.0f,%.0f,%.0f) Rot=(%.1f,%.1f)\n",
               POV.Location.X, POV.Location.Y, POV.Location.Z,
               POV.Rotation.Pitch, POV.Rotation.Yaw);
        __android_log_print(ANDROID_LOG_INFO, "lsrael_pte",
                            "POV 更新: Loc=(%.0f,%.0f,%.0f)",
                            POV.Location.X, POV.Location.Y, POV.Location.Z);
    }
}



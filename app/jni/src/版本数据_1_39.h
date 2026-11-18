#pragma once
/* ============================================================================
 *  和平精英 (PUBG MOBILE 国服) 版本数据 · 1.39
 *  数据来源：
 *    1) /storage/emulated/0/和平/和平[1.39]版本数据.hpp        —— 语义速查表
 *    2) /storage/emulated/0/和平/1.39和平精英SDK.zip
 *         · 和平SDK/Offsets.hpp   —— 引擎基址（Names/UObjectArray/Engine/World）
 *         · 和平SDK/Logs.txt      —— 实测基址与遍历日志（GWorld=0x181BF200 等）
 *         · 和平SDK/AIOHeader.hpp —— 全部类/结构体字段反射偏移（交叉校验）
 *
 *  规则：
 *    · 换赛季只改本文件，源码里不许再出现 libUE4 的裸偏移字面量。
 *    · 每一项后面标 [表] 的是速查表给的，标 [SDK] 的是 AIOHeader 反射确认的，
 *      标 [表+SDK] 的两边一致，可信度最高。
 *    · 标 [沿用] 的是 1.39 数据没给、SDK 也没标出来的，保持 1.38 的值并留待实测。
 * ==========================================================================*/

#include <cstdint>

namespace V139 {

/* ------------------------------------------------------------------ 引擎基址 */
constexpr uintptr_t GWorld基址   = 0x181BF200;   // [SDK+Logs] GWorld  UWorld*   Logs.txt: [<Base>+0x181BF200]=0x73221CA200
constexpr uintptr_t GName基址    = 0x176E6650;   // [表+SDK]  GNames  FName 池（类名表）
constexpr uintptr_t 矩阵基址     = 0x18188930;   // [表]      矩阵：+0x20 -> +0x270
constexpr uintptr_t PhysX实例    = 0x175E6D98;   // [用户提供 2026-11-18] 相对 libUE4.so 模块基址

/* --------------------------------------------------------------- 迫击炮落点 */
// [用户提供 2026-11-18] 从实际 libUE4.so:bss 起算，不叠加模块相对地址 0x17A14AA8。
// 完整命名链明确末级为 0xE24（十进制3620），提交的列表尾部362已截断。
constexpr uintptr_t 迫击炮_BSS入口 = 0x42DAA8;
constexpr uintptr_t 迫击炮_链1    = 0x18;
constexpr uintptr_t 迫击炮_链2    = 0x9C8;
constexpr uintptr_t 迫击炮_落点   = 0xE24;       // 连续 X/Y/Z 三个 float

/* -------------------------------------------------------------- 世界 / 关卡链 */
constexpr uintptr_t 矩阵_1        = 0x20;        // [表] 矩阵基址 -> +0x20
constexpr uintptr_t 矩阵_2        = 0x270;       // [表] -> +0x270 = 相机矩阵
constexpr uintptr_t 世界_持久关卡 = 0xb8;        // [表+SDK] UWorld+0xB8 = PersistentLevel(ULevel*)
constexpr uintptr_t 世界_对局状态  = 0xAD8;      // [表+SDK] UWorld+0xAD8 = GameState
constexpr uintptr_t 世界_游戏实例  = 0xB20;      // [SDK]    UWorld+0xB20 = OwningGameInstance
constexpr uintptr_t 实例_本地玩家  = 0x48;       // [SDK]    UGameInstance+0x48 = TArray<ULocalPlayer*>
constexpr uintptr_t 本地玩家_控制器 = 0x30;      // [SDK]    UPlayer+0x30 = APlayerController*

/* ------------------------------------------------------- 玩家控制器 / 自身链 */
/* 速查表写法（基址取 GWorld）：+0xC0 -> +0x88 -> +0x30 -> 0x36D0
   本机 SDK 反射里 UWorld+0xC0 是 NetDriver，与该链不符，因此运行时两条链都试：
   链A（SDK 反射确认）：GWorld -> +0xB20 GameInstance -> +0x48 LocalPlayers[0]
                        -> +0x30 PlayerController -> +0x36D0 自身Pawn
   链B（速查表原文）：  GWorld -> +0xC0 -> +0x88 -> +0x30 -> +0x36D0 自身Pawn        */
constexpr uintptr_t 自身1        = 0x00c0;       // [表]
constexpr uintptr_t 自身2        = 0x0088;       // [表]
constexpr uintptr_t 自身3        = 0x0030;       // [表+SDK]
constexpr uintptr_t 自身_人物    = 0x36d0;       // [表+SDK] ASTExtraPlayerController::STExtraBaseCharacter
constexpr uintptr_t 控制器_相机管理 = 0x680;     // [SDK]    APlayerController::PlayerCameraManager
constexpr uintptr_t 自身_控制器  = 0x0608;       // [SDK]    APawn::Controller = APlayerController*
constexpr uintptr_t 自身_相机管理 = 0x60C8;      // [沿用]   1.38 的 自身->PlayerController 直连（1.39 SDK 未标注，作兜底）
constexpr uintptr_t 相机_PovLocation = 0x650;    // [沿用]   PlayerCameraManager -> MinimalViewInfo
constexpr uintptr_t 相机_PovRotation = 0x668;    // [沿用]   +0x18
constexpr uintptr_t 相机_PovFov      = 0x688;    // [沿用]   +0x38

/* ------------------------------------------- 人物（AUAECharacter/ASTExtraCharacter） */
constexpr uintptr_t 队伍ID       = 0x0b78;       // [表+SDK] AUAECharacter::TeamID
constexpr uintptr_t 坐标组件     = 0x0260;       // [表+SDK] Character->坐标组件
constexpr uintptr_t 坐标偏移     = 0x0200;       // [表] 0x1F0 + 0x10（ComponentToWorld 后 0x10）
constexpr uintptr_t 玩家名字     = 0x0af8;       // [表+SDK] AUAECharacter::PlayerName (FString)
constexpr uintptr_t 人物高度     = 0x10c0;       // [表+SDK] 值 479.5（SDK 里该槽位叫 SpeedValue，就是过滤用的那个 float）
constexpr uintptr_t 血量         = 0x1060;       // [表+SDK] ASTExtraCharacter::Health
constexpr uintptr_t 最大血量     = 0x1068;       // [表+SDK] HealthMax
constexpr uintptr_t 人物状态     = 0x1700;       // [表]     状态（指针再解一层）
constexpr uintptr_t 人机判断     = 0x0b94;       // [表+SDK] AUAECharacter::bIsAI
constexpr uintptr_t 骨骼Mesh     = 0x0658;       // [表+SDK] ACharacter::Mesh (USkeletalMeshComponent*)
constexpr uintptr_t 骨骼组件     = 0x01f0;       // [表+SDK] USceneComponent::ComponentToWorld
constexpr uintptr_t 骨骼数组     = 0x0848;       // [表]     骨骼变换数组（+0x848 数据 / +0x850 数量）
constexpr uintptr_t 手持武器页   = 0x1170;       // [表+SDK] ASTExtraCharacter::CurrentUsingWeaponSafety

/* --------------------------------------- 武器（ASTExtraWeapon / UShootWeaponEntity） */
constexpr uintptr_t 武器ID        = 0x0de8;      // [表+SDK] ASTExtraWeapon::RepWeaponID
constexpr uintptr_t 武器_子弹链   = 0x3380;      // [沿用]   1.38 的子弹链入口
constexpr uintptr_t 武器_子弹链2  = 0x0698;      // [沿用]
constexpr uintptr_t 武器_子弹数   = 0x1c70;      // [沿用]
constexpr uintptr_t 武器_子弹数Max = 0x1c74;     // [沿用]

/* ------------------------------------------------- 载具（ASTExtraVehicleBase / UVehicleCommonComponent） */
constexpr uintptr_t 载具指针   = 0x0c00;         // [表+SDK] ASTExtraVehicleBase::VehicleCommon
constexpr uintptr_t 载具耐久   = 0x01f8;         // [表+SDK] HP
constexpr uintptr_t 载具耐久Max = 0x01f4;        // [表+SDK] HPMax
constexpr uintptr_t 载具油量   = 0x021c;         // [表+SDK] Fuel
constexpr uintptr_t 载具油量Max = 0x0218;        // [表]

/* ---------------------------------------------------------------- 物资 / 投掷物 */
constexpr uintptr_t 盒内物资   = 0x0e98;         // [表+SDK] APickUpListWrapperActor::PickUpDataList
constexpr uintptr_t 物资结构步长 = 0x0040;       // [SDK]    FPickUpItemData 尺寸 0x40
constexpr uintptr_t 物资_ID     = 0x0004;        // [SDK]    FItemDefineID.TypeSpecificID
constexpr uintptr_t 物资_数量   = 0x0018;        // [SDK]    FPickUpItemData.Count
constexpr uintptr_t 投掷物ID   = 0x0794;         // [表]
constexpr uintptr_t 宝箱开启   = 0x0270;         // [沿用]   已开启状态
constexpr uintptr_t 物资ID字段 = 0x06f4;         // [沿用]   地面物资 ID

/* ------------------------------------------- 对局（ASTExtraGameStateBase） ---------------------------------- */
constexpr uintptr_t 对局_总玩家 = 0x0d9c;       // [表+SDK] ASTExtraGameStateBase::PlayerNum
constexpr uintptr_t 对局_存活玩家 = 0x141c;     // [表+SDK] AlivePlayerNum
constexpr uintptr_t 对局_存活队伍 = 0x148c;     // [表+SDK] AliveTeamNum

/* ------------------------------------------------------- 反射结构（SDK 确认，赛季基本不变） */
constexpr uintptr_t 对象_名称    = 0x18;         // [SDK] UObject::NamePrivate (FName)

} // namespace V139

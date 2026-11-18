#pragma once

#include "版本数据_1_39.h"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

// Checked character metadata reads, shared with the host failure-injection tests.
// Unknown fields are never substituted for confirmed death, team or AI state.
namespace draw_only_actor {
constexpr int ReadAttempts = 2;

inline bool Finite(float value) {
    std::uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return (bits & 0x7f800000u) != 0x7f800000u;
}
inline bool PointerValid(std::uintptr_t value) {
    return value >= 0x1000 && value % alignof(std::uintptr_t) == 0;
}
inline bool TeamValid(int value) { return value >= -1 && value < 200; }
inline bool CurrentHealthValid(float value) {
    return Finite(value) && std::fabs(value) <= 1.0e6f;
}
inline bool MaxHealthValid(float value) {
    return Finite(value) && value > 0 && value <= 1.0e6f;
}

template<class T, class Reader>
inline bool CheckedRead(std::uintptr_t base, std::uintptr_t offset,
                        T& output, Reader&& read) {
    static_assert(std::is_trivially_copyable<T>::value, "Read only a typed memory payload");
    output = {};
    const auto maximum = std::numeric_limits<std::uintptr_t>::max();
    if (base < 0x1000 || offset > maximum - base ||
        sizeof(T) - 1 > maximum - (base + offset)) return false;
    for (int attempt = 0; attempt < ReadAttempts; ++attempt) {
        T candidate{};
        if (read(base + offset, &candidate, sizeof(candidate))) {
            output = candidate;
            return true;
        }
    }
    return false;
}

struct Snapshot {
    int team = 0;
    bool teamKnown = false;
    bool bot = false, botKnown = false;
    int state = 0;
    bool stateKnown = false;
    float currentHealth = 0, maxHealth = 0;
    bool currentHealthKnown = false, maxHealthKnown = false;
    std::uintptr_t mesh = 0, bones = 0;
    int boneCount = 0;
    bool meshKnown = false, bonesKnown = false, boneCountKnown = false;
};

inline bool HasSkeleton(const Snapshot& snapshot) {
    return snapshot.meshKnown && snapshot.bonesKnown && snapshot.boneCountKnown &&
        PointerValid(snapshot.mesh) && PointerValid(snapshot.bones) &&
        snapshot.boneCount >= 15 && snapshot.boneCount <= 512;
}

template<class Reader>
inline Snapshot ReadSnapshot(std::uintptr_t actor, Reader&& read, bool knownCharacter = true) {
    Snapshot result;
    if (!PointerValid(actor)) return result;
    result.meshKnown = CheckedRead(actor, V139::骨骼Mesh, result.mesh, read) && PointerValid(result.mesh);
    if (result.meshKnown) {
        result.bonesKnown = CheckedRead(result.mesh, V139::骨骼数组, result.bones, read) &&
            PointerValid(result.bones);
        // The existing source reads count at bone-array header + sizeof(pointer).
        result.boneCountKnown = CheckedRead(result.mesh, V139::骨骼数组 + sizeof(std::uintptr_t),
            result.boneCount, read) && result.boneCount >= 15 && result.boneCount <= 512;
    }
    // Ordinary world objects fail this inexpensive structural probe. Already
    // confirmed characters still receive metadata when an animation buffer is absent.
    if (!knownCharacter && !HasSkeleton(result)) return result;

    result.teamKnown = CheckedRead(actor, V139::队伍ID, result.team, read) && TeamValid(result.team);
    std::uint8_t ai = 0;
    result.botKnown = CheckedRead(actor, V139::人机判断, ai, read) && ai <= 1;
    result.bot = result.botKnown && ai != 0;
    // Preserve the existing team -1 convention, but only after a successful read.
    if (result.teamKnown && result.team == -1) { result.bot = true; result.botKnown = true; }

    std::uintptr_t stateAddress = 0;
    if (CheckedRead(actor, V139::人物状态, stateAddress, read) && PointerValid(stateAddress))
        result.stateKnown = CheckedRead(stateAddress, 0, result.state, read);
    result.currentHealthKnown = CheckedRead(actor, V139::血量, result.currentHealth, read) &&
        CurrentHealthValid(result.currentHealth);
    result.maxHealthKnown = CheckedRead(actor, V139::最大血量, result.maxHealth, read) &&
        MaxHealthValid(result.maxHealth);
    return result;
}

inline bool HasHealthBar(const Snapshot& snapshot) {
    return snapshot.currentHealthKnown && snapshot.maxHealthKnown &&
        CurrentHealthValid(snapshot.currentHealth) && MaxHealthValid(snapshot.maxHealth);
}
inline bool Contains(const char* name, const char* text) {
    return name && *name && std::strstr(name, text) != nullptr;
}
inline bool StartsWith(const char* name, const char* text) {
    return name && std::strncmp(name, text, std::strlen(text)) == 0;
}

enum class Identification { Unknown, Character, Excluded };

inline bool ExcludedClass(const char* name) {
    // These are non-character patterns already handled by 绘制.cpp.
    constexpr const char* excluded[] = {
        "BPPawn_Escape_Raven", "BPPawn_Escape_UAV_C",
        "CharacterDeadInventoryBox_C", "PickUpListWrapperActor", "RollTombBox_",
        "EscapePlayerTombBox", "DeadInventoryBox", "_TrainingBoxLi", "AirDropBox",
        "MilitarySupplyBoxBase_", "Flaregun", "FlareGun", "Ammo_Flare",
        "EscapeBox_SupplyBox_", "EscapeBoxHight_SupplyBox_",
        "EscapeBox_Medical_", "EscapeBoxHight_Medical_",
        "EscapeBox_Weapon_", "EscapeBoxHight_Weapon_"
    };
    for (const char* pattern : excluded) if (Contains(name, pattern)) return true;
    constexpr const char* vehicles[] = {
        "VH_", "Buggy", "Mirado", "CoupeRB", "Dacia", "PickUp_02_C", "_StationWagon_C",
        "UAZ", "PG117", "AquaRail", "MiniBus", "LadaNiva", "_4SportCar_C", "_Bigfoot_C",
        "ATV1_C", "Snowmobile_C", "_Motorglider_C", "_DesertCar_C"
    };
    for (const char* pattern : vehicles) if (Contains(name, pattern)) return true;
    return false;
}

inline bool KnownCharacterClass(const char* name, bool knownBoss = false) {
    // BP_PlayerPawn_Impostors_C occurs in the supplied original drawing source;
    // STExtraCharacter/UAECharacter are named by the existing V139 field definitions.
    constexpr const char* bosses[] = {
        "Pawn_Escape_RD_Grenade_C", "Pawn_Escape_RD_RoyalGuards_C",
        "Pawn_Escape_Boss_Robocop_C", "Pawn_Escape_BOSS_Claws_C", "Pawn_Escape_RD_SupplyBoss_C"
    };
    if (knownBoss || Contains(name, "BPPawn_Escape_") || StartsWith(name, "BP_PlayerPawn_") ||
        (name && (std::strcmp(name, "STExtraCharacter") == 0 || std::strcmp(name, "ASTExtraCharacter") == 0 ||
                  std::strcmp(name, "UAECharacter") == 0 || std::strcmp(name, "AUAECharacter") == 0))) return true;
    for (const char* boss : bosses) if (Contains(name, boss)) return true;
    return false;
}

inline Identification Identify(const char* className, const Snapshot& snapshot,
                               bool knownBoss = false, bool knownNonCharacter = false) {
    if (knownNonCharacter || ExcludedClass(className)) return Identification::Excluded;
    if (KnownCharacterClass(className, knownBoss)) return Identification::Character;
    // Unlisted classes need independent skeletal, team, AI and health evidence.
    // SpeedValue is deliberately not an identity predicate. HealthMax can fail
    // independently without suppressing a character supported by current health.
    if (HasSkeleton(snapshot) && snapshot.teamKnown && TeamValid(snapshot.team) && snapshot.botKnown &&
        ((snapshot.currentHealthKnown && CurrentHealthValid(snapshot.currentHealth)) ||
         (snapshot.maxHealthKnown && MaxHealthValid(snapshot.maxHealth)))) return Identification::Character;
    return Identification::Unknown;
}

struct Filters {
    int selfTeam = 0;
    bool selfTeamKnown = false;
    bool ignoreBots = false;
    bool ignoreDowned = false;
};
inline bool ShouldSkip(const Snapshot& snapshot, const Filters& filters) {
    if (snapshot.stateKnown && (snapshot.state == 1048592 || snapshot.state == 1048576)) return true;
    if (snapshot.teamKnown && TeamValid(snapshot.team) && snapshot.team >= 0 &&
        filters.selfTeamKnown && TeamValid(filters.selfTeam) && snapshot.team == filters.selfTeam) return true;
    if (filters.ignoreBots && snapshot.botKnown && snapshot.bot) return true;
    if (filters.ignoreDowned && snapshot.currentHealthKnown && CurrentHealthValid(snapshot.currentHealth) &&
        snapshot.currentHealth <= 0) return true;
    return false;
}
} // namespace draw_only_actor

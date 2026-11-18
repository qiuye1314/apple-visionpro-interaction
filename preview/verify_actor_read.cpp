#include "DrawOnlyActorRead.h"
#include <cassert>
#include <cstdio>
#include <map>
#include <vector>

namespace actor = draw_only_actor;
static void Pass(const char* text) { std::printf("PASS %s\n", text); }
struct Request { std::uintptr_t address; std::size_t size; };
struct Memory {
    static constexpr std::uintptr_t target = 0x10000, mesh = 0x20000, bones = 0x30000, state = 0x40000;
    // The two old speed filter slots are fixtures only, never production identity inputs.
    static constexpr std::uintptr_t oldSpeedSlot = 0x10BC;
    std::map<std::uintptr_t, unsigned char> data;
    std::map<std::uintptr_t, int> failures;
    std::vector<Request> requests;
    template<class T> void Set(std::uintptr_t address, T value) {
        const auto* bytes = reinterpret_cast<const unsigned char*>(&value);
        for (std::size_t i = 0; i < sizeof(value); ++i) data[address+i] = bytes[i];
    }
    Memory() {
        Set(target+V139::队伍ID, 2);
        Set<std::uint8_t>(target+V139::人机判断, 0);
        Set<std::uint8_t>(target+V139::人机判断+1, 1);
        Set<std::uint8_t>(target+V139::人机判断+2, 0xff);
        Set<std::uint8_t>(target+V139::人机判断+3, 0x81);
        Set(target+V139::人物状态, state); Set(state, 0);
        Set(target+V139::血量, 90.0f); Set(target+V139::最大血量, 100.0f);
        Set(target+oldSpeedSlot, 479.5f);
        Set(target+V139::人物高度, 600.0f);
        Set(target+V139::骨骼Mesh, mesh);
        Set(mesh+V139::骨骼数组, bones);
        Set(mesh+V139::骨骼数组+sizeof(std::uintptr_t), 68);
    }
    bool operator()(std::uintptr_t address, void* output, std::size_t size) {
        requests.push_back({address,size});
        auto* bytes = static_cast<unsigned char*>(output);
        // Failed calls may have modified their buffers; such output must never be committed.
        std::memset(output, 0xcc, size);
        if (failures[address] > 0) { --failures[address]; return false; }
        for (std::size_t i = 0; i < size; ++i) if (!data.count(address+i)) return false;
        for (std::size_t i = 0; i < size; ++i) bytes[i] = data[address+i];
        return true;
    }
    int Calls(std::uintptr_t address) const {
        int count=0; for (const auto& call : requests) if (call.address==address) ++count; return count;
    }
};

int main() {
    Memory memory;
    auto snapshot = actor::ReadSnapshot(Memory::target,memory);
    assert(snapshot.teamKnown && snapshot.team==2);
    assert(snapshot.botKnown && !snapshot.bot);
    assert(snapshot.stateKnown && snapshot.state==0);
    assert(snapshot.currentHealthKnown && snapshot.currentHealth==90);
    assert(snapshot.maxHealthKnown && snapshot.maxHealth==100);
    assert(memory.Calls(Memory::target+Memory::oldSpeedSlot)==0 && memory.Calls(Memory::target+V139::人物高度)==0);
    assert(actor::HasSkeleton(snapshot) && actor::HasHealthBar(snapshot));
    for (const auto& call : memory.requests)
        if (call.address==Memory::target+V139::人机判断) assert(call.size==1);
    assert(actor::Identify("UnlistedCharacter_C",snapshot)==actor::Identification::Character);
    actor::Filters filters{1,true,true,true};
    assert(!actor::ShouldSkip(snapshot,filters));
    Pass("non-479.5 characters use corroborated metadata and AI reads exactly one byte");

    memory=Memory();
    snapshot=actor::ReadSnapshot(Memory::target,memory,false);
    assert(actor::Identify("UnlistedCharacter_C",snapshot)==actor::Identification::Character);
    assert(actor::Identify("BP_PlayerPawn_Impostors_C",snapshot)==actor::Identification::Character);
    assert(memory.Calls(Memory::target+Memory::oldSpeedSlot)==0 && memory.Calls(Memory::target+V139::人物高度)==0);
    memory.failures[Memory::target+V139::人物高度]=10;
    snapshot=actor::ReadSnapshot(Memory::target,memory,false);
    assert(actor::Identify("UnlistedCharacter_C",snapshot)==actor::Identification::Character);
    assert(actor::Identify("BP_PlayerPawn_Impostors_C",snapshot)==actor::Identification::Character);
    assert(memory.Calls(Memory::target+Memory::oldSpeedSlot)==0 && memory.Calls(Memory::target+V139::人物高度)==0);
    Pass("old-slot 479.5 with different or unreadable new-slot values never affects character identification");

    memory=Memory();
    memory.failures[Memory::target+Memory::oldSpeedSlot]=10;
    memory.failures[Memory::target+V139::人物高度]=10;
    snapshot=actor::ReadSnapshot(Memory::target,memory,false);
    assert(actor::Identify("UnlistedCharacter_C",snapshot)==actor::Identification::Character);
    assert(!actor::ShouldSkip(snapshot,filters));
    memory.Set(Memory::target+V139::骨骼Mesh,std::uintptr_t(0));
    snapshot=actor::ReadSnapshot(Memory::target,memory,true);
    assert(!actor::HasSkeleton(snapshot) && actor::HasHealthBar(snapshot));
    assert(actor::Identify("BP_PlayerPawn_Impostors_C",snapshot)==actor::Identification::Character);
    assert(!actor::ShouldSkip(snapshot,filters));
    assert(memory.Calls(Memory::target+Memory::oldSpeedSlot)==0 && memory.Calls(Memory::target+V139::人物高度)==0);
    Pass("both speed slots unreadable still preserve structural and previously confirmed characters");

    memory=Memory(); memory.Set(Memory::target+V139::骨骼Mesh,std::uintptr_t(0));
    snapshot=actor::ReadSnapshot(Memory::target,memory,false);
    assert(memory.requests.size()==1 && memory.requests[0].address==Memory::target+V139::骨骼Mesh);
    assert(!snapshot.teamKnown && !snapshot.botKnown && !snapshot.stateKnown && !snapshot.currentHealthKnown && !snapshot.maxHealthKnown);
    assert(actor::Identify("UnknownPickup_C",snapshot)==actor::Identification::Unknown);
    memory=Memory(); memory.Set(Memory::mesh+V139::骨骼数组+sizeof(std::uintptr_t),0);
    snapshot=actor::ReadSnapshot(Memory::target,memory,false);
    assert(memory.requests.size()==3 && memory.Calls(Memory::target+V139::血量)==0 && memory.Calls(Memory::target+V139::队伍ID)==0);
    assert(!actor::HasSkeleton(snapshot) && !snapshot.teamKnown && !snapshot.currentHealthKnown);
    Pass("unconfirmed objects without a usable skeleton skip all character metadata reads");

    memory=Memory(); memory.Set(Memory::target+V139::骨骼Mesh,std::uintptr_t(0));
    snapshot=actor::ReadSnapshot(Memory::target,memory);
    assert(!actor::HasSkeleton(snapshot) && snapshot.teamKnown && snapshot.botKnown && actor::HasHealthBar(snapshot));
    assert(memory.requests.front().address==Memory::target+V139::骨骼Mesh);
    assert(actor::Identify("BPPawn_Escape_Soldier_C",snapshot)==actor::Identification::Character);
    Pass("default confirmed-character reads retain metadata despite missing bones and probe skeleton first");

    memory=Memory();
    memory.failures[Memory::target+V139::血量]=1;
    snapshot=actor::ReadSnapshot(Memory::target,memory);
    assert(snapshot.currentHealthKnown && snapshot.currentHealth==90);
    assert(memory.Calls(Memory::target+V139::血量)==2);
    assert(memory.Calls(Memory::target+V139::队伍ID)==1);
    Pass("a transient read failure retries once and only commits a successful payload");

    memory=Memory();
    memory.failures[Memory::target+V139::最大血量]=10;
    snapshot=actor::ReadSnapshot(Memory::target,memory);
    assert(!snapshot.maxHealthKnown && snapshot.maxHealth==0 && snapshot.currentHealthKnown);
    assert(!actor::HasHealthBar(snapshot) && !actor::ShouldSkip(snapshot,filters));
    assert(actor::Identify("UnlistedCharacter_C",snapshot)==actor::Identification::Character);
    assert(memory.Calls(Memory::target+V139::最大血量)==actor::ReadAttempts);
    Pass("unknown maximum health suppresses only the blood bar and retries remain bounded");

    memory=Memory(); memory.failures[Memory::target+V139::血量]=10;
    snapshot=actor::ReadSnapshot(Memory::target,memory);
    assert(!snapshot.currentHealthKnown && snapshot.currentHealth==0);
    assert(!actor::ShouldSkip(snapshot,filters));
    assert(actor::Identify("UnlistedCharacter_C",snapshot)==actor::Identification::Character);
    Pass("failed current-health reads cannot masquerade as confirmed downed state");

    memory=Memory();
    memory.failures[Memory::target+V139::队伍ID]=10;
    memory.failures[Memory::target+V139::人机判断]=10;
    memory.failures[Memory::target+V139::人物状态]=10;
    snapshot=actor::ReadSnapshot(Memory::target,memory);
    filters.selfTeam=0;
    assert(!snapshot.teamKnown && !snapshot.botKnown && !snapshot.stateKnown);
    assert(!actor::ShouldSkip(snapshot,filters));
    assert(actor::Identify("BP_PlayerPawn_Impostors_C",snapshot)==actor::Identification::Character);
    assert(memory.Calls(Memory::state)==0);
    Pass("unknown team AI and state do not trigger teammate bot or death filters");

    memory=Memory(); snapshot=actor::ReadSnapshot(Memory::target,memory);
    filters={2,true,false,false}; assert(actor::ShouldSkip(snapshot,filters));
    filters.selfTeamKnown=false; assert(!actor::ShouldSkip(snapshot,filters));
    filters={1,true,false,false};
    for (int state : {1048592,1048576}) {
        memory.Set(Memory::state,state); snapshot=actor::ReadSnapshot(Memory::target,memory);
        assert(snapshot.stateKnown && actor::ShouldSkip(snapshot,filters));
    }
    memory.Set(Memory::state,0);
    memory.Set(Memory::target+V139::血量,0.0f);
    snapshot=actor::ReadSnapshot(Memory::target,memory);
    assert(!actor::ShouldSkip(snapshot,filters));
    filters.ignoreDowned=true; assert(actor::ShouldSkip(snapshot,filters));
    Pass("successfully confirmed teammates deaths and optional downed filtering take effect immediately");

    memory=Memory(); memory.Set<std::uint8_t>(Memory::target+V139::人机判断,1);
    snapshot=actor::ReadSnapshot(Memory::target,memory);
    filters={1,true,true,false}; assert(snapshot.botKnown && snapshot.bot && actor::ShouldSkip(snapshot,filters));
    filters.ignoreBots=false; assert(!actor::ShouldSkip(snapshot,filters));
    memory.Set(Memory::target+V139::队伍ID,-1);
    memory.failures[Memory::target+V139::人机判断]=10;
    snapshot=actor::ReadSnapshot(Memory::target,memory);
    assert(snapshot.botKnown && snapshot.bot);
    Pass("known AI and the existing team-minus-one convention retain explicit bot filters");

    actor::Snapshot empty;
    assert(actor::Identify("UnknownPickup_C",empty)==actor::Identification::Unknown);
    assert(actor::Identify(nullptr,empty)==actor::Identification::Unknown);
    assert(actor::Identify("BPPawn_Escape_Soldier_C",empty)==actor::Identification::Character);
    assert(actor::Identify("BP_PlayerPawn_Impostors_C",empty)==actor::Identification::Character);
    assert(actor::Identify("STExtraCharacter",empty)==actor::Identification::Character);
    assert(actor::Identify("UAECharacter",empty)==actor::Identification::Character);
    assert(actor::Identify("Pawn_Escape_BOSS_Claws_C",empty)==actor::Identification::Character);
    assert(actor::Identify("ConfirmedBoss_C",empty,true)==actor::Identification::Character);
    Pass("documented character families survive missing metadata without identifying unknown pickups");

    memory=Memory(); snapshot=actor::ReadSnapshot(Memory::target,memory);
    for (const char* name : {"BPPawn_Escape_Raven_C","BPPawn_Escape_UAV_C",
            "CharacterDeadInventoryBox_C","EscapeBox_SupplyBox_C","AirDropBox_C",
            "VH_UAZ_C","BP_VH_Buggy_C","Ammo_Flare_C"})
        assert(actor::Identify(name,snapshot,true)==actor::Identification::Excluded);
    assert(actor::Identify("KnownPickup_C",snapshot,false,true)==actor::Identification::Excluded);
    snapshot.botKnown=false;
    assert(actor::Identify("UnknownActor_C",snapshot)==actor::Identification::Unknown);
    Pass("explicit noncharacters override matching metadata and unlisted classes require independent evidence");

    memory=Memory();
    memory.Set<std::uint8_t>(Memory::target+V139::人机判断,8);
    memory.Set(Memory::target+V139::队伍ID,-2);
    memory.Set(Memory::target+V139::血量,std::numeric_limits<float>::quiet_NaN());
    memory.Set(Memory::target+V139::最大血量,0.0f);
    memory.Set(Memory::mesh+V139::骨骼数组+sizeof(std::uintptr_t),513);
    snapshot=actor::ReadSnapshot(Memory::target,memory);
    assert(!snapshot.botKnown && !snapshot.teamKnown && !snapshot.currentHealthKnown && !snapshot.maxHealthKnown);
    assert(!actor::HasSkeleton(snapshot) && !actor::HasHealthBar(snapshot));
    filters={0,true,true,true}; assert(!actor::ShouldSkip(snapshot,filters));
    assert(actor::Identify("UnknownActor_C",snapshot)==actor::Identification::Unknown);
    Pass("invalid values remain unknown under fast-math without false filtering or invalid skeleton reads");

    memory=Memory(); memory.Set(Memory::target+V139::骨骼Mesh,std::uintptr_t(0));
    snapshot=actor::ReadSnapshot(Memory::target,memory);
    assert(!snapshot.meshKnown && !snapshot.bonesKnown && !snapshot.boneCountKnown);
    assert(!actor::HasSkeleton(snapshot) && actor::Identify("BP_PlayerPawn_Impostors_C",snapshot)==actor::Identification::Character);
    assert(memory.Calls(V139::骨骼数组)==0);
    memory.requests.clear();
    std::uint64_t value=77;
    assert(!actor::CheckedRead(0,0,value,memory) && value==0);
    assert(!actor::CheckedRead(std::numeric_limits<std::uintptr_t>::max()-4,0,value,memory));
    assert(!actor::CheckedRead(std::numeric_limits<std::uintptr_t>::max()-4,8,value,memory));
    assert(memory.requests.empty());
    Pass("missing meshes isolate skeleton metadata and invalid address arithmetic never reaches the reader");
    return 0;
}

#include "DrawOnlyFrameStability.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
#include <limits>

namespace stable = draw_only_stability;
using Address = std::uintptr_t;
static_assert(sizeof(Address) == 8, "The production actor array uses 64-bit pointers");
static constexpr Address Level = 0x10000, Array = 0x20000, Array2 = 0x30000;
static int checks = 0;
static void Pass(const char* text) { ++checks; std::printf("PASS %s\n", text); }

struct Script {
    struct Step { Address address; std::size_t size; std::vector<unsigned char> bytes; bool success; };
    std::vector<Step> steps;
    std::size_t cursor = 0;
    void Bytes(Address address, const void* source, std::size_t size, bool success = true,
               std::size_t written = std::numeric_limits<std::size_t>::max()) {
        written = std::min(written, size);
        const auto* first = static_cast<const unsigned char*>(source);
        steps.push_back({address, size, std::vector<unsigned char>(first, first + written), success});
    }
    void Header(Address array, int count, Address level = Level) {
        unsigned char bytes[12]{};
        std::memcpy(bytes, &array, 8); std::memcpy(bytes + 8, &count, 4);
        Bytes(level + 0xA0, bytes, sizeof(bytes));
    }
    void FailHeader(Address level = Level) {
        unsigned char partial[12]; std::memset(partial, 0xff, sizeof(partial));
        Bytes(level + 0xA0, partial, sizeof(partial), false, 7);
    }
    void Actors(Address array, const std::vector<Address>& actors) {
        for (std::size_t first = 0; first < actors.size(); first += 256) {
            const auto size = std::min<std::size_t>(256, actors.size() - first);
            Bytes(array + first * 8, actors.data() + first, size * 8);
        }
    }
    void Stable(const std::vector<Address>& actors, Address array = Array, Address level = Level) {
        Header(array, static_cast<int>(actors.size()), level);
        if (!actors.empty()) { Actors(array, actors); Header(array, static_cast<int>(actors.size()), level); }
    }
    void FailedBlocks(Address array, int count) {
        // Both outer snapshots fail on their first block; every partial payload is poison.
        std::vector<Address> poison(std::min(count, 256), std::numeric_limits<Address>::max());
        for (int attempt = 0; attempt < 2; ++attempt) {
            Header(array, count);
            Bytes(array, poison.data(), poison.size() * 8, false, 7);
            Bytes(array, poison.data(), poison.size() * 8, false, 11);
            Header(array, count);
        }
    }
    void FailedHeaders(Address level = Level) { for (int i = 0; i < 4; ++i) FailHeader(level); }
    bool operator()(Address address, void* output, std::size_t size) {
        assert(cursor < steps.size());
        const auto& step = steps[cursor++];
        assert(address == step.address && size == step.size);
        if (!step.bytes.empty()) std::memcpy(output, step.bytes.data(), step.bytes.size());
        return step.success;
    }
    void Done() const { assert(cursor == steps.size()); }
};

static void Seed(stable::ActorList& actors, const std::vector<Address>& values = {0x50000,0x60000}) {
    Script read; read.Stable(values);
    assert(actors.Refresh(read, Level, 0)); read.Done();
    assert(!actors.reused && actors.Values() == values);
}

int main() {
    struct Payload { std::uint32_t first, second; };
    const Payload original{0x11223344,0x55667788}, expected{0xabcdef12,0x34567890};
    Payload value = original;
    Script immediate; immediate.Bytes(0x1000, &expected, sizeof(expected));
    assert(stable::ReadValue(immediate, 0x1000, value)); immediate.Done();
    assert(value.first == expected.first && value.second == expected.second);
    Pass("ReadValue publishes complete values and stops after its first success");

    Script retry; retry.Bytes(0x1000, &expected, sizeof(expected), false, 3);
    retry.Bytes(0x1000, &expected, sizeof(expected)); value = original;
    assert(stable::ReadValue(retry, 0x1000, value)); retry.Done();
    assert(value.first == expected.first && value.second == expected.second);
    Script failed; failed.Bytes(0x1000, &expected, sizeof(expected), false, 3);
    failed.Bytes(0x1000, &expected, sizeof(expected), false, 5); value = original;
    assert(!stable::ReadValue(failed, 0x1000, value)); failed.Done();
    assert(value.first == original.first && value.second == original.second);
    Pass("ReadValue retries at most twice and failed partial reads preserve the caller value");

    Script invalid;
    assert(!stable::ReadValue(invalid, 0xfff, value));
    assert(!stable::ReadValue(invalid, std::numeric_limits<Address>::max() - 3, value));
    invalid.Done();
    assert(value.first == original.first && value.second == original.second);
    Pass("ReadValue rejects low and overflowing addresses without invoking the reader");

    stable::Recent<int> recent; int restored = 91;
    assert(!recent.Restore(restored, 0) && restored == 91);
    recent.Store(7, 0);
    assert(recent.Restore(restored, .05) && restored == 7);
    assert(recent.Restore(restored, .119));
    assert(recent.Restore(restored, stable::GraceSeconds));
    restored = 91;
    assert(!recent.Restore(restored, .120001) && restored == 91 && recent.sampledAt == 0);
    assert(!recent.Restore(restored, -.001));
    recent.Store(8, 1); assert(recent.Restore(restored, 1.119) && restored == 8);
    recent.Clear(); assert(!recent.Restore(restored, 1.12) && !recent.available);
    Pass("Recent uses a 120ms sample TTL that restores cannot extend and rejects reversed time");

    stable::ArrayHeader header{Array, 2}; Script headerFailure;
    headerFailure.FailHeader(); headerFailure.FailHeader();
    assert(!stable::ReadHeader(headerFailure, Level, header)); headerFailure.Done();
    assert(header.data == Array && header.count == 2);
    Script headerRetry; headerRetry.FailHeader(); headerRetry.Header(Array2, 3);
    assert(stable::ReadHeader(headerRetry, Level, header)); headerRetry.Done();
    assert(header.data == Array2 && header.count == 3);
    Pass("partial header failures preserve the previous header and a second complete read replaces it");

    stable::ActorList unique; Script dedup;
    dedup.Stable({0x50000,0,0x60000,0x50000,0xfff,0x70000,0x60000});
    assert(unique.Refresh(dedup, Level, 0)); dedup.Done();
    assert((unique.Values() == std::vector<Address>{0x50000,0x60000,0x70000}) && !unique.reused);
    Pass("ActorList deduplicates valid pointers while preserving their first-seen order");

    std::vector<Address> many;
    for (int i = 0; i < 300; ++i) many.push_back(0x40000 + i * 16);
    many[256] = many[0]; many[299] = 0;
    Script chunked; chunked.Stable(many);
    stable::ActorList chunks; assert(chunks.Refresh(chunked, Level, 0)); chunked.Done();
    assert(chunks.Values().size() == 298 && chunks.Values().front() == many[0] &&
           chunks.Values().back() == many[298]);
    Pass("ActorList reads all 256-entry chunks and deduplicates across chunk boundaries");

    stable::ActorList cached; Seed(cached);
    for (double now : {.01, .119, .120001}) {
        Script unavailable; unavailable.FailedHeaders();
        const bool fresh = now <= stable::GraceSeconds;
        assert(cached.Refresh(unavailable, Level, now) == fresh); unavailable.Done();
        assert(cached.reused == fresh);
        assert(fresh ? cached.Values().size() == 2 : cached.Values().empty());
    }
    Pass("header outages reuse the last snapshot briefly without extending its 120ms lifetime");

    stable::ActorList partial; Seed(partial);
    Script blockFailure; blockFailure.FailedBlocks(Array, 2);
    assert(partial.Refresh(blockFailure, Level, .05)); blockFailure.Done();
    assert(partial.reused && (partial.Values() == std::vector<Address>{0x50000,0x60000}));
    Script blockExpiry; blockExpiry.FailedBlocks(Array, 2);
    assert(!partial.Refresh(blockExpiry, Level, .121)); blockExpiry.Done();
    assert(!partial.reused && partial.Values().empty());
    Pass("failed partial array blocks cannot publish poison or keep the cached list alive indefinitely");

    for (auto changed : {stable::ArrayHeader{Array2,2}, stable::ArrayHeader{Array,3}}) {
        stable::ActorList stale; Seed(stale);
        Script changedFailure; changedFailure.FailedBlocks(changed.data, changed.count);
        assert(!stale.Refresh(changedFailure, Level, .01)); changedFailure.Done();
        assert(!stale.reused && stale.Values().empty());
    }
    Pass("known array reallocation or count changes forbid falling back to the previous header identity");

    stable::ActorList changed; Seed(changed);
    Script reallocated; reallocated.Stable({0x80000,0x90000,0xa0000}, Array2);
    assert(changed.Refresh(reallocated, Level, .01)); reallocated.Done();
    assert(!changed.reused && changed.Header().data == Array2 && changed.Header().count == 3 &&
           (changed.Values() == std::vector<Address>{0x80000,0x90000,0xa0000}));
    Pass("a stable new array replaces its previous allocation and count atomically");

    stable::ActorList raced; Seed(raced); Script mutation;
    mutation.Header(Array, 2); mutation.Actors(Array,{0xb0000,0xc0000}); mutation.Header(Array2, 3);
    mutation.Stable({0xd0000,0xe0000,0xf0000}, Array2);
    assert(raced.Refresh(mutation, Level, .02)); mutation.Done();
    assert(!raced.reused && (raced.Values() == std::vector<Address>{0xd0000,0xe0000,0xf0000}));
    Pass("a header race discards its candidate and publishes only the second stable snapshot");

    stable::ActorList unstable; Seed(unstable); Script continuous;
    continuous.Header(Array, 2); continuous.Actors(Array,{0xb0000,0xc0000}); continuous.Header(Array2, 2);
    continuous.Header(Array2, 2); continuous.Actors(Array2,{0xd0000,0xe0000}); continuous.Header(Array, 2);
    assert(!unstable.Refresh(continuous, Level, .02)); continuous.Done();
    assert(!unstable.reused && unstable.Values().empty());
    Pass("two concurrent header mutations publish neither mixed candidates nor the old cached list");

    stable::ActorList empty; Seed(empty); Script clear;
    clear.Stable({}, 0);
    assert(empty.Refresh(clear, Level, .01)); clear.Done();
    assert(!empty.reused && empty.Values().empty() && empty.Header().count == 0);
    Script afterEmpty; afterEmpty.FailedHeaders();
    assert(empty.Refresh(afterEmpty, Level, .02)); afterEmpty.Done();
    assert(empty.Values().empty());
    Pass("a successful empty list authoritatively clears actors and later failures cannot revive them");

    stable::ActorList newLevel; Seed(newLevel); Script levelFailure;
    levelFailure.FailedHeaders(Level + 0x1000);
    assert(!newLevel.Refresh(levelFailure, Level + 0x1000, .01)); levelFailure.Done();
    assert(!newLevel.reused && newLevel.Values().empty());
    Script newLevelRead; newLevelRead.Stable({0x91000}, Array2, Level + 0x1000);
    assert(newLevel.Refresh(newLevelRead, Level + 0x1000, .02)); newLevelRead.Done();
    assert((newLevel.Values() == std::vector<Address>{0x91000}));
    Script noLevel;
    assert(!newLevel.Refresh(noLevel, 0, .03) && newLevel.Values().empty()); noLevel.Done();
    Pass("changing or losing the level identity clears cache before any possible fallback");

    for (auto invalidHeader : {stable::ArrayHeader{Array,-1}, stable::ArrayHeader{Array,200001},
                               stable::ArrayHeader{0,1},
                               stable::ArrayHeader{std::numeric_limits<Address>::max()-3,2}}) {
        stable::ActorList bad; Seed(bad); Script badRead;
        badRead.Header(invalidHeader.data, invalidHeader.count);
        assert(!bad.Refresh(badRead, Level, .01)); badRead.Done();
        assert(!bad.reused && bad.Values().empty());
    }
    Pass("invalid counts pointers and overflowing array ranges clear cache without reading array memory");

    struct Point { float X=0,Y=0,Z=0; };
    stable::CharacterHistory<Point> history;
    assert(history.Match(0x5000,true,12,true));
    history.position.Store({1,2,3},0); history.Confirm(0);
    assert(history.Match(0x5000,true,12,true) && history.Confirmed(.119));
    assert(history.Match(0,false,0,false) && history.component==0x5000 && history.classId==12);
    assert(history.position.Fresh(.119) && !history.Confirmed(.120001) && !history.Confirmed(-.001));
    assert(!history.Match(0x6000,true,12,true));
    assert(history.component==0x6000 && history.classKnown && history.classId==12 &&
           !history.identified && !history.position.available);
    Pass("CharacterHistory preserves matching or unknown identity but a changed root clears position and confirmation");

    history.position.Store({4,5,6},1); history.Confirm(1);
    assert(!history.Match(0x6000,true,13,true));
    assert(history.component==0x6000 && history.classKnown && history.classId==13 &&
           !history.identified && !history.position.available);
    history.position.Store({7,8,9},2); history.Confirm(2);
    assert(!history.Match(0,true,13,true));
    assert(history.component==0 && !history.identified && !history.position.available);
    Pass("class replacement or a known null root cannot inherit an old character position");
    std::printf("Frame stability checks: %d\n", checks);
}

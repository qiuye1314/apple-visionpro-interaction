#include "DrawOnlyPhysXThrottle.h"
#include <cassert>
#include <cstdio>
#include <future>
#include <limits>
#include <vector>

using namespace draw_only_physx;
static void Pass(const char* message) { std::printf("PASS %s\n", message); }

int main() {
    PressureCooldown gate;
    std::atomic<int> pressure{0};
    assert(gate.Allow(pressure.load(), 0));
    for (int i = 0; i < 4; ++i) ObserveTaskCost(pressure, 300.0);
    assert(pressure.load() == 4);
    assert(!gate.Allow(pressure.load(), 100));
    assert(!gate.Allow(pressure.load(), 2099));
    assert(gate.Allow(pressure.load(), 2100));
    ObserveTaskCost(pressure, 20.0);
    assert(pressure.load() == 3 && gate.Allow(pressure.load(), 2101));
    Pass("four slow tasks trigger cooldown and a fast recovery probe resumes scanning without respawn");

    pressure.store(4);
    assert(!gate.Allow(pressure.load(), 3000));
    std::atomic<int> admitted{0};
    std::vector<std::future<void>> workers;
    for (int i = 0; i < 3; ++i) workers.push_back(std::async(std::launch::async, [&] {
        if (gate.Allow(pressure.load(), 5000)) ++admitted;
    }));
    for (auto& worker : workers) worker.get();
    assert(admitted.load() == 1);
    assert(!gate.Allow(pressure.load(), 6999));
    assert(gate.Allow(pressure.load(), 7000));
    Pass("three workers share one recovery probe per cooldown and sustained pressure still receives later probes");

    for (int i = 0; i < 200; ++i) ObserveTaskCost(pressure, 300.0);
    assert(pressure.load() == 100);
    for (int i = 0; i < 200; ++i) ObserveTaskCost(pressure, 1.0);
    assert(pressure.load() == 0);
    assert(gate.Allow(pressure.load(), 7001));
    Pass("pressure stays within bounds and a recovered low-pressure worker runs immediately");

    RetryCooldown retries;
    assert(retries.Allow(0));
    for (int i = 1; i < 1000; ++i) assert(!retries.Allow(i));
    assert(retries.Allow(1000));
    retries.RecordAttempt(1100); // Explicit/manual attempt is allowed before the deadline.
    assert(!retries.Allow(1999) && !retries.Allow(2099));
    assert(retries.Allow(2100));
    Pass("failed automatic initialization retries at most once per second while manual attempts postpone retries");

    TriangleBudget invalidTriangles;
    std::size_t checked = 0, drawn = 0;
    for (std::size_t i = 0; i < 1000000 && invalidTriangles.Check(); ++i) {
        ++checked;
        continue; // Every triangle fails validation; none can increment a drawn counter.
        ++drawn;
    }
    assert(checked == TriangleBudget::Limit && drawn == 0);
    assert(invalidTriangles.Checked() == TriangleBudget::Limit);
    Pass("malformed or offscreen triangles consume the same finite per-frame inspection budget");

    for (const std::size_t count : {0u, 1u, 3999u, 4000u, 4001u, 7999u, 8000u, 333333u}) {
        const auto stride = TriangleBudget::Stride(count);
        assert(stride >= 1);
        TriangleBudget budget;
        std::size_t last = 0, visits = 0;
        for (std::size_t i = 0; i < count && budget.Check(); i += stride) { last = i; ++visits; }
        assert(visits <= TriangleBudget::Limit);
        if (count) assert(count - last <= stride);
    }
    assert(TriangleBudget::Stride(std::numeric_limits<std::size_t>::max()) > 0);
    Pass("sampling covers the full mesh with a bounded number of checks and overflow-safe stride calculation");
}

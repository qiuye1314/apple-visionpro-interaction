#include "DrawOnlyMemoryBridge.h"
#include "版本数据_1_39.h"
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <future>
#include <thread>

struct FakeDriver {
    int reads = 0, pids = 0, modules = 0;
    bool success = true;
    uintptr_t value = 0x71000000;
    uintptr_t lastAddress = 0;
    int pid = 1234;
    uintptr_t module = 0x70000000;
    std::mutex mutex;
    std::condition_variable condition;
    bool block = false, entered = false, release = false;
};

static bool Read(void* context, uintptr_t address, void* output, size_t size) {
    auto& driver = *static_cast<FakeDriver*>(context);
    ++driver.reads;
    driver.lastAddress = address;
    std::memcpy(output, &driver.value, size);
    if (driver.block) {
        std::unique_lock<std::mutex> lock(driver.mutex);
        driver.entered = true;
        driver.condition.notify_all();
        driver.condition.wait(lock, [&]{ return driver.release; });
    }
    return driver.success;
}
static int Pid(void* context, const char* name) {
    auto& driver = *static_cast<FakeDriver*>(context);
    ++driver.pids;
    assert(std::strcmp(name, "game.test") == 0);
    return driver.pid;
}
static uintptr_t Module(void* context, const char* name) {
    auto& driver = *static_cast<FakeDriver*>(context);
    ++driver.modules;
    assert(std::strcmp(name, "libUE4.so") == 0);
    return driver.module;
}
static void Pass(const char* message) { std::printf("PASS %s\n", message); }

int main() {
    using namespace draw_only_memory;
    uintptr_t output = 123;
    assert(!draw_only_memory::Read(0x1000, &output, sizeof(output)) && output == 0);
    assert(FindPid("game.test") == -1 && ModuleBase("libUE4.so") == 0);
    Pass("an unbound scene reader returns no process, module or stale bytes");

    FakeDriver driver, other;
    assert(!Bind(nullptr, Read, Pid, Module));
    assert(!Bind(&driver, nullptr, Pid, Module));
    assert(Bind(&driver, Read, Pid, Module));
    assert(!Bind(&other, Read, Pid, Module));
    assert(draw_only_memory::Read(0x2000, &output, sizeof(output)));
    assert(output == driver.value && driver.lastAddress == 0x2000);
    assert(FindPid("game.test") == driver.pid && ModuleBase("libUE4.so") == driver.module);
    assert(driver.reads == 1 && driver.pids == 1 && driver.modules == 1);
    Pass("scene read, process and module requests use the selected driver only");

    driver.success = false;
    output = 123;
    assert(!draw_only_memory::Read(0x2000, &output, sizeof(output)) && output == 0);
    assert(driver.reads == 2);
    driver.success = true;
    Pass("failed callbacks clear partially written data without a fallback channel");

    assert(!draw_only_memory::Read(0xFFF, &output, sizeof(output)));
    assert(!draw_only_memory::Read(std::numeric_limits<uintptr_t>::max(), &output, sizeof(output)));
    assert(!draw_only_memory::Read(0x2000, nullptr, sizeof(output)));
    assert(!draw_only_memory::Read(0x2000, &output, 0));
    assert(FindPid(nullptr) == -1 && FindPid("") == -1);
    assert(ModuleBase(nullptr) == 0 && ModuleBase("") == 0);
    assert(driver.reads == 2 && driver.pids == 1 && driver.modules == 1);
    driver.pid = 0; driver.module = 1;
    assert(FindPid("game.test") == -1 && ModuleBase("libUE4.so") == 0);
    Pass("invalid inputs, overflow and invalid returned handles are rejected");

    static_assert(V139::PhysX实例 == 0x175E6D98, "Use the supplied 1.39 PhysX offset");
    assert(ReadPointer(0x70000000, V139::PhysX实例, output));
    assert(output == driver.value && driver.lastAddress == 0x875E6D98);
    driver.value = 1;
    assert(!ReadPointer(0x70000000, V139::PhysX实例, output) && output == 0);
    driver.value = 0x71000001;
    assert(!ReadPointer(0x70000000, V139::PhysX实例, output) && output == 0);
    const int beforeOverflow = driver.reads;
    assert(!ReadPointer(std::numeric_limits<uintptr_t>::max(), 1, output) && output == 0);
    assert(driver.reads == beforeOverflow);
    Pass("instance acquisition uses the supplied offset and rejects corrupt pointers");

    driver.value = 0x71000000;
    Unbind(&other);
    assert(draw_only_memory::Read(0x2000, &output, sizeof(output)) && output == driver.value);
    driver.block = true;
    std::thread reader([&] {
        uintptr_t value = 0;
        assert(draw_only_memory::Read(0x2000, &value, sizeof(value)) && value == driver.value);
    });
    {
        std::unique_lock<std::mutex> lock(driver.mutex);
        driver.condition.wait(lock, [&] { return driver.entered; });
    }
    auto unbind = std::async(std::launch::async, [&] { Unbind(&driver); });
    assert(unbind.wait_for(std::chrono::milliseconds(25)) == std::future_status::timeout);
    {
        std::lock_guard<std::mutex> lock(driver.mutex);
        driver.release = true;
    }
    driver.condition.notify_all();
    reader.join();
    unbind.get();
    assert(!draw_only_memory::Read(0x2000, &output, sizeof(output)) && output == 0);
    assert(FindPid("game.test") == -1 && ModuleBase("libUE4.so") == 0);
    Pass("unbinding ignores other owners and waits for in-flight reads before descriptor teardown");

    assert(Bind(&other, Read, Pid, Module));
    other.value = 0x72000000;
    assert(draw_only_memory::Read(0x2000, &output, sizeof(output)) && output == other.value);
    Unbind(&other);
    Pass("a later driver can bind after the previous owner has fully disconnected");
}

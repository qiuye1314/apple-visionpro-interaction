#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>
#include <shared_mutex>

// Scene and character reads share the selected driver. The bridge owns no
// descriptor and never opens another process-reading channel.
namespace draw_only_memory {
using ReadCallback = bool (*)(void*, uintptr_t, void*, size_t);
using PidCallback = int (*)(void*, const char*);
using ModuleCallback = uintptr_t (*)(void*, const char*);

struct Registry {
    std::shared_mutex mutex;
    void* context = nullptr;
    ReadCallback read = nullptr;
    PidCallback pid = nullptr;
    ModuleCallback module = nullptr;
};

inline Registry& State() {
    // PhysX workers are process-lifetime threads. Keep the empty registry alive
    // during static teardown after the owning driver has unbound its callbacks.
    static Registry* registry = new Registry;
    return *registry;
}

inline bool Bind(void* context, ReadCallback read, PidCallback pid, ModuleCallback module) {
    if (!context || !read || !pid || !module) return false;
    auto& state = State();
    std::unique_lock<std::shared_mutex> lock(state.mutex);
    if (state.context && state.context != context) return false;
    state.context = context;
    state.read = read;
    state.pid = pid;
    state.module = module;
    return true;
}

inline void Unbind(void* context) {
    auto& state = State();
    std::unique_lock<std::shared_mutex> lock(state.mutex);
    if (state.context != context) return;
    state.context = nullptr;
    state.read = nullptr;
    state.pid = nullptr;
    state.module = nullptr;
}

inline bool Read(uintptr_t address, void* buffer, size_t size) {
    if (!buffer || !size) return false;
    std::memset(buffer, 0, size);
    if (address < 0x1000 || size - 1 > std::numeric_limits<uintptr_t>::max() - address)
        return false;
    auto& state = State();
    std::shared_lock<std::shared_mutex> lock(state.mutex);
    if (state.context && state.read && state.read(state.context, address, buffer, size))
        return true;
    // A driver may have partially modified the output before reporting failure.
    std::memset(buffer, 0, size);
    return false;
}

inline int FindPid(const char* name) {
    if (!name || !*name) return -1;
    auto& state = State();
    std::shared_lock<std::shared_mutex> lock(state.mutex);
    const int value = state.context && state.pid ? state.pid(state.context, name) : -1;
    return value > 0 ? value : -1;
}

inline uintptr_t ModuleBase(const char* name) {
    if (!name || !*name) return 0;
    auto& state = State();
    std::shared_lock<std::shared_mutex> lock(state.mutex);
    const uintptr_t value = state.context && state.module ? state.module(state.context, name) : 0;
    return value >= 0x1000 ? value : 0;
}

inline bool ReadPointer(uintptr_t base, uintptr_t offset, uintptr_t& value) {
    value = 0;
    if (base < 0x1000 || offset > std::numeric_limits<uintptr_t>::max() - base)
        return false;
    if (!Read(base + offset, &value, sizeof(value)) || value < 0x1000 ||
        value % alignof(uintptr_t) != 0) {
        value = 0;
        return false;
    }
    return true;
}
} // namespace draw_only_memory

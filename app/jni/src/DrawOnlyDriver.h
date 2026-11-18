#pragma once
// Read-side ABI extracted from the supplied TWT kernel.h and the existing RT
// transport. No driver-side input injection or game-memory mutation is exposed.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <istream>
#include <limits>
#include <sstream>
#include <string>
#if !defined(_WIN32)
#include <sys/ioctl.h>
#endif

namespace draw_only_driver {
enum class Backend : int { Twt = 1, Rt = 2 };
struct Request {
    std::int32_t pid;
    std::uintptr_t addr;
    void* buffer;
    std::size_t size;
};
static_assert(sizeof(void*) == 8 && sizeof(Request) == 32, "64-bit driver request ABI");
static_assert(offsetof(Request, pid) == 0 && offsetof(Request, addr) == 8 &&
              offsetof(Request, buffer) == 16 && offsetof(Request, size) == 24,
              "TWT and RT request field offsets");

// Linux arm64 _IOW('T', command, Request), verified against platform headers.
constexpr unsigned long TwtPid = 0x40205400UL;
constexpr unsigned long TwtBase = 0x40205401UL;
constexpr unsigned long TwtBss = 0x40205403UL;
constexpr unsigned long TwtRead = 0x40205404UL;
constexpr unsigned long RtRead = 601;
constexpr std::uintptr_t TwtAddressMask = 0xFFFFFFFFFFFFULL;
#if !defined(_WIN32)
static_assert(TwtPid == _IOW('T', 0, Request) && TwtBase == _IOW('T', 1, Request) &&
              TwtBss == _IOW('T', 3, Request) && TwtRead == _IOW('T', 4, Request),
              "TWT ioctl numbers must match the target kernel ABI");
#endif

inline bool ValidBackend(Backend backend) {
    return backend == Backend::Twt || backend == Backend::Rt;
}
inline bool CopyName(const char* name, char (&buffer)[256]) {
    if (!name || !*name) return false;
    const std::size_t length = std::strlen(name);
    if (length >= sizeof(buffer)) return false;
    std::memcpy(buffer, name, length + 1);
    return true;
}

template<class Ioctl>
bool Read(Backend backend, int fd, std::int32_t pid, std::uintptr_t address,
          void* buffer, std::size_t size, Ioctl&& call) {
    if (!buffer || size == 0) return false;
    if (backend == Backend::Twt) address &= TwtAddressMask;
    const auto limit = backend == Backend::Twt ? TwtAddressMask :
        std::numeric_limits<std::uintptr_t>::max();
    if (!ValidBackend(backend) || fd < 0 || pid <= 0 || address < 0x1000 ||
        size - 1 > limit - address) {
        std::memset(buffer, 0, size);
        return false;
    }
    Request request{};
    request.pid = pid; request.addr = address; request.buffer = buffer; request.size = size;
    if (call(fd, backend == Backend::Twt ? TwtRead : RtRead, &request) != 0) {
        std::memset(buffer, 0, size);
        return false;
    }
    return true;
}

template<class Ioctl>
std::int32_t FindTwtPid(int fd, const char* name, Ioctl&& call) {
    char buffer[256]{};
    if (fd < 0 || !CopyName(name, buffer)) return -1;
    Request request{}; request.buffer = buffer;
    return call(fd, TwtPid, &request) == 0 && request.pid > 0 ? request.pid : -1;
}

template<class Ioctl>
std::uintptr_t FindTwtModule(int fd, std::int32_t pid, const char* name, bool bss, Ioctl&& call) {
    char buffer[256]{};
    if (fd < 0 || pid <= 0 || !CopyName(name, buffer)) return 0;
    Request request{}; request.pid = pid; request.buffer = buffer;
    if (call(fd, bss ? TwtBss : TwtBase, &request) != 0) return 0;
    const auto result = request.addr & TwtAddressMask;
    return result >= 0x1000 ? result : 0;
}

struct MapEntry {
    std::uintptr_t begin = 0, end = 0, offset = 0;
    std::string permissions, path;
};
inline bool ParseMap(const std::string& line, MapEntry& out) {
    std::istringstream input(line);
    std::string range, permissions, fileOffset, device, inode;
    if (!(input >> range >> permissions >> fileOffset >> device >> inode)) return false;
    unsigned long long begin = 0, end = 0, offset = 0;
    char trailing = 0;
    if (std::sscanf(range.c_str(), "%llx-%llx%c", &begin, &end, &trailing) != 2 ||
        std::sscanf(fileOffset.c_str(), "%llx%c", &offset, &trailing) != 1 ||
        begin < 0x1000 || end <= begin || permissions.size() != 4) return false;
    std::string path;
    std::getline(input, path);
    const auto first = path.find_first_not_of(" \t");
    path = first == std::string::npos ? std::string() : path.substr(first);
    constexpr const char* deleted = " (deleted)";
    if (path.size() >= std::strlen(deleted) &&
        path.compare(path.size() - std::strlen(deleted), std::strlen(deleted), deleted) == 0)
        path.resize(path.size() - std::strlen(deleted));
    out = {static_cast<std::uintptr_t>(begin), static_cast<std::uintptr_t>(end),
           static_cast<std::uintptr_t>(offset), permissions, path};
    return true;
}
inline bool MatchesModule(const std::string& path, const char* module) {
    if (!module || !*module || path.empty() || path.front() == '[') return false;
    if (std::strchr(module, '/')) return path == module;
    const auto slash = path.find_last_of('/');
    return path.substr(slash == std::string::npos ? 0 : slash + 1) == module;
}
inline bool WritablePrivate(const MapEntry& entry) {
    return entry.permissions == "rw-p";
}
inline std::uintptr_t ModuleFromMaps(std::istream& maps, const char* module, bool bss) {
    if (!module || !*module) return 0;
    MapEntry previous;
    bool previousWasModuleData = false;
    std::string line;
    while (std::getline(maps, line)) {
        MapEntry entry;
        if (!ParseMap(line, entry)) { previousWasModuleData = false; continue; }
        const bool matches = MatchesModule(entry.path, module);
        if (!bss && matches && entry.offset == 0 && entry.begin != 0x8000)
            return entry.begin;
        // BSS here means the anonymous continuation used by the supplied chain,
        // not an ELF symbol or the beginning of the file-backed data segment.
        const bool anonymousBss = entry.path.empty() || entry.path == "[anon:.bss]";
        if (bss && previousWasModuleData && previous.end == entry.begin &&
            anonymousBss && WritablePrivate(entry)) return entry.begin;
        previous = entry;
        previousWasModuleData = matches && WritablePrivate(entry);
    }
    return 0;
}
} // namespace draw_only_driver

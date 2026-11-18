#include "DrawOnlyDriver.h"
#include <cassert>
#include <cstdio>
#include <string>

namespace driver = draw_only_driver;
using driver::Backend;
static void Pass(const char* label) { std::printf("PASS %s\n", label); }
struct MockIoctl {
    int calls = 0, result = 0;
    int seenFd = -1;
    unsigned long seenCommand = 0;
    driver::Request seen{};
    std::string name;
    std::int32_t returnedPid = 321;
    std::uintptr_t returnedAddress = 0x7100000000ULL;
    int operator()(int fd, unsigned long command, driver::Request* request) {
        ++calls; seenFd = fd; seenCommand = command; seen = *request;
        if (command == driver::TwtRead || command == driver::RtRead) {
            // Partial writes followed by failure must never leak to the caller.
            std::memset(request->buffer, 0x5A, request->size);
        } else {
            assert(request->size == 0 && request->addr == 0);
            name = static_cast<char*>(request->buffer);
            request->pid = returnedPid; request->addr = returnedAddress;
        }
        return result;
    }
};
static std::uintptr_t Maps(const std::string& text, bool bss, const char* name = "libUE4.so") {
    std::istringstream source(text); return driver::ModuleFromMaps(source, name, bss);
}
int main() {
    static_assert(sizeof(driver::Request) == 32 && offsetof(driver::Request, size) == 24, "ABI");
    assert(driver::TwtPid == 0x40205400 && driver::TwtBase == 0x40205401 &&
           driver::TwtBss == 0x40205403 && driver::TwtRead == 0x40205404 && driver::RtRead == 601);
    Pass("TWT and RT retain supplied 64-bit request layout and command numbers");

    MockIoctl mock;
    unsigned value = 0;
    assert(driver::Read(Backend::Twt, 9, 123, 0xABCD000000012345ULL, &value, sizeof(value), mock));
    assert(value == 0x5A5A5A5A && mock.seenFd == 9 && mock.seen.pid == 123 &&
           mock.seen.addr == 0x12345 && mock.seenCommand == driver::TwtRead);
    Pass("TWT reads normalize tagged addresses and select the supplied read ioctl");

    assert(driver::Read(Backend::Rt, 10, 456, 0x7000001235ULL, &value, sizeof(value), mock));
    assert(mock.seenFd == 10 && mock.seen.pid == 456 && mock.seen.addr == 0x7000001235ULL &&
           mock.seenCommand == 601 && mock.seen.size == sizeof(value));
    Pass("RT retains socket read operation and unaligned read addresses");

    mock.result = -1;
    value = 123;
    assert(!driver::Read(Backend::Twt, 9, 123, 0x12345, &value, sizeof(value), mock) && value == 0);
    value = 123;
    assert(!driver::Read(Backend::Rt, 9, 123, 0x12345, &value, sizeof(value), mock) && value == 0);
    mock.result = 0;
    Pass("failed ioctls clear partially written output on both backends");

    const int calls = mock.calls;
    for (Backend backend : {Backend::Twt, Backend::Rt, static_cast<Backend>(4)}) {
        value = 123;
        assert(!driver::Read(backend, -1, 123, 0x12345, &value, sizeof(value), mock) && value == 0);
        value = 123;
        assert(!driver::Read(backend, 9, 0, 0x12345, &value, sizeof(value), mock) && value == 0);
        value = 123;
        assert(!driver::Read(backend, 9, 123, 0xFFF, &value, sizeof(value), mock) && value == 0);
    }
    value = 123;
    assert(!driver::Read(static_cast<Backend>(4), 9, 123, 0x12345, &value, sizeof(value), mock) && value == 0);
    assert(!driver::Read(Backend::Twt, 9, 123, 0x12345, nullptr, 4, mock));
    assert(!driver::Read(Backend::Rt, 9, 123, 0x12345, &value, 0, mock));
    assert(mock.calls == calls);
    Pass("invalid backend descriptors processes and buffers never invoke a driver");

    value = 123;
    assert(!driver::Read(Backend::Twt, 9, 123, 0xFF00000000000000ULL, &value, 4, mock) && value == 0);
    value = 123;
    assert(!driver::Read(Backend::Twt, 9, 123, driver::TwtAddressMask - 1, &value, 4, mock) && value == 0);
    value = 123;
    assert(!driver::Read(Backend::Rt, 9, 123, UINTPTR_MAX - 1, &value, 4, mock) && value == 0);
    assert(mock.calls == calls);
    Pass("address validation follows tag removal and rejects overflowing ranges");

    assert(driver::FindTwtPid(9, "com.tencent.tmgp.pubgmhd", mock) == 321);
    assert(mock.seenCommand == driver::TwtPid && mock.seen.pid == 0 && mock.name == "com.tencent.tmgp.pubgmhd");
    mock.returnedPid = 0;
    assert(driver::FindTwtPid(9, "game", mock) == -1);
    mock.returnedPid = 321; mock.result = -1;
    assert(driver::FindTwtPid(9, "game", mock) == -1);
    mock.result = 0;
    Pass("TWT PID requests initialize fields and reject failed or empty PID results");

    const std::string longest(255, 'a'), tooLong(256, 'b');
    assert(driver::FindTwtPid(9, longest.c_str(), mock) == 321 && mock.name == longest);
    const int nameCalls = mock.calls;
    assert(driver::FindTwtPid(9, tooLong.c_str(), mock) == -1);
    assert(driver::FindTwtPid(9, "", mock) == -1);
    assert(driver::FindTwtPid(9, nullptr, mock) == -1);
    assert(driver::FindTwtModule(9, 321, tooLong.c_str(), false, mock) == 0);
    assert(mock.calls == nameCalls);
    Pass("TWT names are terminated within the 256-byte ABI and never silently truncated");

    mock.returnedAddress = 0xABCD007100000000ULL;
    assert(driver::FindTwtModule(9, 321, "libUE4.so", false, mock) == 0x7100000000ULL);
    assert(mock.seenCommand == driver::TwtBase && mock.seen.pid == 321 && mock.name == "libUE4.so");
    assert(driver::FindTwtModule(9, 321, "libUE4.so", true, mock) == 0x7100000000ULL);
    assert(mock.seenCommand == driver::TwtBss);
    mock.returnedAddress = 0;
    assert(driver::FindTwtModule(9, 321, "libUE4.so", true, mock) == 0);
    mock.returnedAddress = 0x7100000000ULL; mock.result = -1;
    assert(driver::FindTwtModule(9, 321, "libUE4.so", false, mock) == 0);
    Pass("TWT module base and BSS use separate commands and validate return addresses");

    const std::string prefix =
        "8000-9000 r--p 00000000 08:01 7 /data/libUE4.so\n"
        "100000-102000 r--p 00000000 08:01 8 /data/libUE4.so.backup\n"
        "7000000000-7000001000 r--p 00000000 08:01 9 /data/libUE4.so\n"
        "7000001000-7000010000 r-xp 00001000 08:01 9 /data/libUE4.so\n"
        "7000010000-7000012000 rw-p 00010000 08:01 9 /data/libUE4.so\n";
    assert(Maps(prefix, false) == 0x7000000000ULL);
    assert(Maps(prefix, true) == 0);
    assert(Maps(prefix + "7000012000-7000080000 rw-p 00000000 00:00 0 [anon:.bss]\n", true) == 0x7000012000ULL);
    Pass("maps module lookup matches the basename and keeps BSS distinct from module base");

    assert(Maps(prefix + "7000012000-7000080000 rw-p 00000000 00:00 0\n", true) == 0x7000012000ULL);
    assert(Maps("7000000000-7000001000 r--p 00000000 08:01 9 /data/libUE4.so (deleted)\n", false) == 0x7000000000ULL);
    assert(Maps(prefix, false, "/data/libUE4.so") == 0x7000000000ULL);
    assert(Maps(prefix, false, "libUE4") == 0);
    Pass("maps supports unnamed BSS deleted modules and exact full paths");

    for (const char* suffix : {
        "7000013000-7000080000 rw-p 00000000 00:00 0 [anon:.bss]\n",
        "7000012000-7000080000 r--p 00000000 00:00 0 [anon:.bss]\n",
        "7000012000-7000080000 rw-s 00000000 00:00 0 [anon:.bss]\n",
        "7000012000-7000080000 rw-p 00000000 00:00 0 [anon:scudo:primary]\n",
        "7000012000-7000080000 rw-p 00000000 00:00 1 /data/other.so\n",
        "invalid map line\n7000012000-7000080000 rw-p 00000000 00:00 0 [anon:.bss]\n"
    }) assert(Maps(prefix + suffix, true) == 0);
    assert(Maps("7000010000-7000012000 r--p 00010000 08:01 9 /data/libUE4.so\n"
                "7000012000-7000080000 rw-p 00000000 00:00 0 [anon:.bss]\n", true) == 0);
    Pass("BSS lookup rejects gaps readonly regions unrelated mappings and malformed lines");
    return 0;
}

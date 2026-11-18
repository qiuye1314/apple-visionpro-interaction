#include "SpatialConsole.h"
#include <cassert>
#include <cstdio>
#include <string>

struct Result { int choice; std::string output; };
static Result DriverChoice(const std::string& text) {
    FILE* input = std::tmpfile(); FILE* output = std::tmpfile();
    assert(input && output);
    assert(std::fwrite(text.data(), 1, text.size(), input) == text.size());
    std::rewind(input);
    const int choice = spatial_console::ChooseDriver(spatial_console::Terminal(input, output, 0));
    std::fflush(output); std::rewind(output);
    std::string contents; char buffer[1024]; std::size_t count;
    while ((count = std::fread(buffer, 1, sizeof(buffer), output))) contents.append(buffer, count);
    std::fclose(input); std::fclose(output);
    return {choice, contents};
}
static void Pass(const char* label) { std::printf("PASS %s\n", label); }
int main() {
    const auto twt = DriverChoice("1\n"), rt = DriverChoice("2\n");
    assert(twt.choice == 1 && rt.choice == 2);
    assert(twt.output.find("TWT") != std::string::npos && twt.output.find("RT") != std::string::npos);
    for (const char* removed : {"DEV", "HK", "QX", "GT"})
        assert(twt.output.find(removed) == std::string::npos);
    assert(std::string(spatial_console::DriverName(1)).find("TWT") != std::string::npos);
    assert(std::string(spatial_console::DriverName(2)).find("RT") != std::string::npos);
    assert(std::string(spatial_console::DriverName(4)) == spatial_console::DriverName(0));
    Pass("startup exposes only TWT and RT with the new backend numbering");

    assert(DriverChoice("0\n").choice == 0 && DriverChoice("").choice == 0);
    Pass("explicit exit and exhausted input cancel driver startup");

    assert(DriverChoice("4\n3\n-1\n1junk\n2\n").choice == 2);
    assert(DriverChoice(std::string(300, '9') + "\n1\n").choice == 1);
    assert(DriverChoice("  2  \n").choice == 2);
    Pass("removed choices malformed input and overlong lines cannot select a backend");
    return 0;
}

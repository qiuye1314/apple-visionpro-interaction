#pragma once
// Lightweight preset metadata shared by the UI and image loader.
namespace spatial_backdrop_presets {
constexpr int Count=3;
inline const char* Name(int index){
    static const char* const names[]={"星眸","泡影","蓝蝶"};
    return index>=0&&index<Count?names[index]:"内置背景";
}
inline const char* Key(int index){
    static const char* const keys[]={"builtin:star-eyes","builtin:bubbles","builtin:blue-butterfly"};
    return index>=0&&index<Count?keys[index]:"builtin:invalid";
}
} // namespace spatial_backdrop_presets

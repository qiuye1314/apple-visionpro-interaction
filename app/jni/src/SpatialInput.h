#pragma once
#include <mutex>
#include <vector>

// The input thread publishes screen coordinates only. ImGui stays on its render thread.
namespace spatial_input {
struct Point { float x=0,y=0; int id=0; };
struct Sample { Point points[2]; int count=0; };
struct Channel { std::mutex mutex; std::vector<Sample> samples; bool connected=false; };
inline Channel& Get() { static Channel channel; return channel; }
inline void Publish(const Sample& sample) {
    Channel& c=Get();std::lock_guard<std::mutex> lock(c.mutex);c.connected=true;
    if(c.samples.size()>128)c.samples.erase(c.samples.begin());
    c.samples.push_back(sample);
}
inline bool Read(std::vector<Sample>& samples) {
    Channel& c=Get();std::lock_guard<std::mutex> lock(c.mutex);
    samples.clear();samples.swap(c.samples);return c.connected;
}
}

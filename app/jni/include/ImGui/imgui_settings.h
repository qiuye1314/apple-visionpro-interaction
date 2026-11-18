#include "imgui.h"

namespace c {

	inline ImVec4 accent = ImColor(0, 62, 255, 255); // 蓝色 RGB(0, 62, 255)

	namespace bg
	{
		inline ImVec4 background = ImColor(255, 255, 255, 255); // 纯白背景
		inline ImVec2 size = ImVec2(960, 610);
		inline float rounding = 18.f;
	}

	namespace child
	{
		inline ImVec4 background = ImColor(255, 255, 255, 255); // 纯白背景
		inline ImVec4 outline = ImColor(0, 62, 255, 100); // 蓝色边框
		inline float rounding = 10.f;
	}

	namespace checkbox
	{
		inline ImVec4 background = ImColor(255, 255, 255, 255); // 纯白背景
		inline ImVec4 circle_inactive = ImColor(200, 220, 255, 255); // 淡蓝色
		inline float rounding = 2.5f;
	}

	namespace slider
	{
		inline ImVec4 background = ImColor(255, 255, 255, 255); // 纯白背景
		inline ImVec4 circle_inactive = ImColor(200, 220, 255, 255); // 淡蓝色
		inline float rounding = 2.5f;
	}

	namespace tabs
	{
		inline ImVec4 background = ImColor(255, 255, 255, 255); // 纯白背景
		inline ImVec4 outline = ImColor(0, 62, 255, 100); // 蓝色边框
		inline float rounding = 5.f;
	}

	namespace combo
	{
		inline ImVec4 background = ImColor(255, 255, 255, 255); // 纯白背景
		inline ImVec4 outline = ImColor(0, 62, 255, 100); // 蓝色边框
		inline ImVec4 icon = ImColor(0, 62, 255, 150); // 蓝色图标
		inline float rounding = 5.f;
	}

	namespace picker
	{
		inline ImVec4 background = ImColor(255, 255, 255, 255); // 纯白背景
		inline ImVec4 outline = ImColor(0, 62, 255, 100); // 蓝色边框
		inline float rounding = 2.5f;
	}

	namespace button
	{
		inline ImVec4 background_active = ImColor(0, 62, 255, 120); // 蓝色激活状态
		inline ImVec4 background_inactive = ImColor(255, 255, 255, 255); // 纯白非激活状态
		inline ImVec4 outline = ImColor(0, 62, 255, 80); // 蓝色边框
		inline float rounding = 5.f;
	}

	namespace keybind
	{
		inline ImVec4 background_active = ImColor(0, 62, 255, 100); // 蓝色激活状态
		inline ImVec4 background_inactive = ImColor(255, 255, 255, 255); // 纯白背景
		inline ImVec4 outline = ImColor(0, 62, 255, 100); // 蓝色边框
		inline float rounding = 2.f;
	}

	namespace input
	{
		inline ImVec4 background_active = ImColor(255, 255, 255, 255); // 纯白激活状态
		inline ImVec4 background_inactive = ImColor(255, 255, 255, 255); // 纯白非激活状态
		inline ImVec4 outline = ImColor(0, 62, 255, 100); // 蓝色边框
		inline ImVec4 icon = ImColor(0, 62, 255, 150); // 蓝色图标
		inline float rounding = 5.f;
	}

	namespace scroll
	{
		inline float rounding = 30.f;
	}

	namespace text
	{
		inline ImVec4 text_active = ImColor(0, 62, 255, 255); // 蓝色激活文字
		inline ImVec4 text_hov = ImColor(0, 62, 255, 225); // 蓝色悬停文字
		inline ImVec4 text = ImColor(0, 62, 255, 150); // 蓝色普通文字
		inline float glow_size = 15.f;
	}
}

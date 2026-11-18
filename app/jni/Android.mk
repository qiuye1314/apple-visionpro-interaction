LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := lsrael_DrawOnly_1_39.sh

LOCAL_CFLAGS := -O2 -ffast-math -fno-math-errno -fvisibility=hidden -ffunction-sections -fdata-sections -w
LOCAL_CPPFLAGS := $(LOCAL_CFLAGS) -std=c++17 -fexceptions -fpermissive -Wno-error=c++11-narrowing
LOCAL_CFLAGS += -DVK_USE_PLATFORM_ANDROID_KHR
LOCAL_CPPFLAGS += -DVK_USE_PLATFORM_ANDROID_KHR
LOCAL_LDFLAGS += -Wl,--gc-sections,--strip-all,-z,max-page-size=16384

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH) \
    $(LOCAL_PATH)/include \
    $(LOCAL_PATH)/include/ImGui \
    $(LOCAL_PATH)/include/My_Utils \
    $(LOCAL_PATH)/src \
    $(LOCAL_PATH)/src/ImGui \
    $(LOCAL_PATH)/include/embree3 \
    $(LOCAL_PATH)/include/foundation \
    $(LOCAL_PATH)/include/vulkan

# Only the drawing entry point, renderer, scene reader and UI are compiled.
LOCAL_SRC_FILES := \
    src/main.cpp \
    src/悬浮窗.cpp \
    src/绘制.cpp \
    src/绘图.cpp \
    src/计算.cpp \
    src/ReadGame.cpp \
    src/struct.cpp \
    src/VectorHelper.cpp \
    src/ImGui/imgui.cpp \
    src/ImGui/imgui_draw.cpp \
    src/ImGui/imgui_tables.cpp \
    src/ImGui/imgui_widgets.cpp \
    src/ImGui/imgui_tricks.cpp \
    src/ImGui/imgui_freetype.cpp \
    src/ImGui/imgui_impl_android.cpp \
    src/ImGui/imgui_impl_opengl3.cpp \
    src/ImGui/imgui_impl_vulkan.cpp \
    src/ImGui/TouchHelperA.cpp \
    src/My_Utils/imgui_image.cpp \
    src/My_Utils/stb_image.cpp \
    src/vulkan/VulkanUtils.cpp \
    src/vulkan/vulkan_wrapper.cpp

LOCAL_LDLIBS := -llog -landroid -lEGL -lGLESv1_CM -lGLESv2 -lGLESv3 -lvulkan
LOCAL_STATIC_LIBRARIES := prebuilt_embree prebuilt_sys prebuilt_math prebuilt_tasking prebuilt_simd prebuilt_lexers
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := prebuilt_embree
LOCAL_SRC_FILES := lib/libembree3.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := prebuilt_sys
LOCAL_SRC_FILES := lib/libsys.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := prebuilt_math
LOCAL_SRC_FILES := lib/libmath.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := prebuilt_tasking
LOCAL_SRC_FILES := lib/libtasking.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := prebuilt_simd
LOCAL_SRC_FILES := lib/libsimd.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := prebuilt_lexers
LOCAL_SRC_FILES := lib/liblexers.a
include $(PREBUILT_STATIC_LIBRARY)

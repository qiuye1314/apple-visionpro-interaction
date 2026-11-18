APP_ABI := arm64-v8a
APP_PLATFORM := android-31
APP_STL := c++_static
APP_CPPFLAGS += -fexceptions

# 强制优化编译（原为默认 debug=-O0，导致帧率极低）
APP_OPTIM := release

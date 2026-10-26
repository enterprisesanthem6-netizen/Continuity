// Host-only stub so the SDK headers can be syntax-checked off-device.
#pragma once
enum { ANDROID_LOG_DEBUG = 3, ANDROID_LOG_INFO = 4, ANDROID_LOG_WARN = 5, ANDROID_LOG_ERROR = 6 };
extern "C" int __android_log_write(int prio, const char *tag, const char *text);
extern "C" int __android_log_print(int prio, const char *tag, const char *fmt, ...);

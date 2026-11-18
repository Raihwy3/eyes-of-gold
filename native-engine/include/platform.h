#ifndef PORT_PLATFORM_H
#define PORT_PLATFORM_H
#include <stdint.h>
#include <stddef.h>
#define PLATFORM_3DS 1
#define PLATFORM_ARM 1
#define PD_CONSTRUCTOR __attribute__((constructor))
#define PD_EXPORT __attribute__((visibility("default")))
enum LogLevel {LOG_ERROR,LOG_WARNING,LOG_NOTE,LOG_INFO,LOG_DEBUG};
#endif

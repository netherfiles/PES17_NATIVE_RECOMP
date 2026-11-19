#include "build_info.h"

namespace pes17 {

std::string BuildInfo::architecture() {
#if defined(__aarch64__)
    return "ARM64";
#elif defined(__arm__)
    return "ARM32";
#elif defined(__x86_64__)
    return "x86_64";
#elif defined(__i386__)
    return "x86";
#else
    return "unknown";
#endif
}

std::string BuildInfo::platform() {
#if defined(__ANDROID__)
    return "Android";
#elif defined(__linux__)
    return "Linux";
#else
    return "Unknown";
#endif
}

}

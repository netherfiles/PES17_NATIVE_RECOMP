#pragma once

#include <string>

namespace pes17 {

struct BuildInfo {
    static constexpr const char* kRuntimeVersion = "0.0.1-dev";
    static constexpr const char* kTargetBuild = "PES2017-PC-Optimized";

    static constexpr const char* kExpectedExeSha256 =
        "771d69d42cea26cffa5ab54d28985a99207d093fcac054c8a2c24297ca64dc45";

    static std::string architecture();
    static std::string platform();
};

}

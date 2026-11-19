#pragma once

#include <cstdint>
#include <string>

#include "../ir/ir.h"

namespace pes17::codegen {

struct GeneratedCode {
    bool success = false;
    std::string source;
    std::string error;
};

class CppGenerator {
public:
    GeneratedCode generate(
        const ir::Function& function,
        const std::string& functionName,
        std::uint64_t startAddress = 0) const;
};

}

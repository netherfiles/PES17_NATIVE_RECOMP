#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "ir.h"

namespace pes17::ir {

struct VerificationResult {
    bool valid = true;
    std::size_t errors = 0;
    std::vector<std::string> messages;
};

class Verifier {
public:
    VerificationResult verify(
        const Function& function) const;
};

}

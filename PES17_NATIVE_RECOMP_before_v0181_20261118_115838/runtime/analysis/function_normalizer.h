#pragma once

#include <optional>

#include "function.h"

namespace pes17::analysis {

class FunctionNormalizer {
public:
    std::optional<Function> normalize(
        const Function& function) const;
};

}

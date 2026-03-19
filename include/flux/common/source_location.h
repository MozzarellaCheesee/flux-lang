#pragma once
#include <cstdint>

namespace flux {
    struct SourceLocation {
        const char* filepath = "";
        uint32_t    line     = 0;
        uint32_t    col      = 0;
    };
}

#pragma once
#include "./source_location.h"
#include <string>
#include <vector>

namespace flux {
    enum class DiagLevel {
        Error,
        Warning,
        Note,
    };

    struct Diagnostic {
        DiagLevel      level;
        SourceLocation loc;
        std::string    message;
    };

    class DiagEngine {
    public:
        void emit(DiagLevel level, SourceLocation loc, std::string message);
        bool has_errors() const;
        size_t error_count() const;
        const std::vector<Diagnostic>& all() const;
        void print_all() const;
    private:
        std::vector<Diagnostic> diags_;
        size_t error_count_ = 0;
    };
} // namespace flux

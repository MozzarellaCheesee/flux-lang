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
        void emit(DiagLevel level, SourceLocation loc, std::string message) {
            diags_.push_back({level, loc, std::move(message)});
            if (level == DiagLevel::Error) error_count_++;
        }

        bool has_errors() const { return error_count_ > 0; }
        size_t error_count() const { return error_count_; }

        const std::vector<Diagnostic>& all() const { return diags_; }

        void print_all() const {
            for (const auto& d : diags_) {
                const char* prefix = (d.level == DiagLevel::Error)   ? "error"
                                : (d.level == DiagLevel::Warning) ? "warning"
                                                                    : "note";
                fprintf(stderr, "%s:%u:%u: %s: %s\n",
                    d.loc.filepath, d.loc.line, d.loc.col,
                    prefix, d.message.c_str());
            }
        }

    private:
        std::vector<Diagnostic> diags_;
        size_t error_count_ = 0;
    };

} // namespace flux

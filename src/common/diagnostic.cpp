#include "flux/common/diagnostic.h"

namespace flux {
    void DiagEngine::emit(DiagLevel level, SourceLocation loc, std::string message) {
        diags_.push_back({level, loc, std::move(message)});
        if (level == DiagLevel::Error) error_count_++;
    }

    bool DiagEngine::has_errors() const { return error_count_ > 0; }

    size_t DiagEngine::error_count() const { return error_count_; }

    const std::vector<Diagnostic>& DiagEngine::all() const { return diags_; }

    void DiagEngine::print_all() const {
            for (const auto& d : diags_) {
                const char* prefix = (d.level == DiagLevel::Error)   ? "error"
                                : (d.level == DiagLevel::Warning) ? "warning"
                                                                    : "note";
                fprintf(stderr, "%s:%u:%u: %s: %s\n",
                    d.loc.filepath, d.loc.line, d.loc.col,
                    prefix, d.message.c_str());
            }
        }
}
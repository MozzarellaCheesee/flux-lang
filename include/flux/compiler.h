#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <optional>

namespace flux {

    // ── Уровень оптимизации ───────────────────────────────
    enum class OptLevel { O0, O1, O2, O3, Os };

    // ── Режим вывода ──────────────────────────────────────
    enum class EmitMode {
        Executable,  // по умолчанию — компилировать и слинковать
        ObjectFile,  // -c  → только .o
        LLVMIR,      // --emit-ir → .ll (текстовый IR)
        Assembly,    // --emit-asm → .s
    };

    // ── Опции компилятора ─────────────────────────────────
    struct CompilerOptions {
        std::filesystem::path       input_file;
        std::string                 output_name    = "a.out";
        EmitMode                    emit_mode      = EmitMode::Executable;
        OptLevel                    opt_level      = OptLevel::O0;
        bool                        verbose        = false;   // -v
        bool                        dump_ast       = false;   // --dump-ast
        bool                        dump_ir        = false;   // --dump-ir (печать в stderr)
        bool                        no_sema        = false;   // --no-sema (отладка)
        std::vector<std::string>    include_dirs;             // -I<dir>
        std::string                 target_triple  = "";      // --target=<triple>
    };

    // ── Результат компиляции ──────────────────────────────
    struct CompileResult {
        bool        success = false;
        std::string error;            // сообщение при неудаче

        explicit operator bool() const { return success; }
    };

    // ── Парсинг аргументов командной строки ──────────────
    // Возвращает nullopt и печатает usage при ошибке
    std::optional<CompilerOptions> parse_args(int argc, char** argv);

    // ── Главная точка компиляции ──────────────────────────
    CompileResult compile(const CompilerOptions& opts);

    // ── Вспомогательная: печать справки ──────────────────
    void print_usage(const char* argv0);
    void print_version();

}

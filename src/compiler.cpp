#include "flux/compiler.h"
#include "flux/lexer/lexer.h"
#include "flux/common/diagnostic.h"
#include "flux/parser/parser.h"
#include "flux/preprocessor/preprocessor.h"
#include "flux/sema/sema.h"
#include "flux/codegen/codegen.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdlib>

namespace flux {

    // ═══════════════════════════════════════════════════
    // Утилиты
    // ═══════════════════════════════════════════════════

    static std::string find_linker() {
        for (auto* cmd : {"clang", "clang-18", "clang-17", "clang-16", "gcc", "cc"}) {
            std::string test = std::string("which ") + cmd + " > /dev/null 2>&1";
            if (std::system(test.c_str()) == 0)
                return cmd;
        }
        return "";
    }

    static std::string opt_flag(OptLevel lvl) {
        switch (lvl) {
        case OptLevel::O0: return "-O0";
        case OptLevel::O1: return "-O1";
        case OptLevel::O2: return "-O2";
        case OptLevel::O3: return "-O3";
        case OptLevel::Os: return "-Os";
        }
        return "-O0";
    }

    // ═══════════════════════════════════════════════════
    // Справка и версия
    // ═══════════════════════════════════════════════════

    void print_version() {
        std::cout << "fluxc 0.1.0  (LLVM backend)\n";
    }

    void print_usage(const char* argv0) {
        std::cout <<
            "Usage: " << argv0 << " <file.flx> [options]\n"
            "\n"
            "Options:\n"
            "  -o <file>          Output file name (default: a.out)\n"
            "  -c                 Compile only, do not link (emit .o)\n"
            "  --emit-ir          Emit LLVM IR to <stem>.ll\n"
            "  --emit-asm         Emit assembly to <stem>.s\n"
            "  -O0 / -O1 / -O2 / -O3 / -Os\n"
            "                     Optimization level (default: -O0)\n"
            "  -I<dir>            Add include directory\n"
            "  --target=<triple>  Override target triple\n"
            "  --dump-ast         Print AST to stderr\n"
            "  --dump-ir          Print LLVM IR to stderr\n"
            "  --no-sema          Skip semantic analysis (debug)\n"
            "  -v / --verbose     Verbose output\n"
            "  --version          Print compiler version\n"
            "  -h / --help        Print this help\n";
    }

    // ═══════════════════════════════════════════════════
    // Парсинг аргументов
    // ═══════════════════════════════════════════════════

    std::optional<CompilerOptions> parse_args(int argc, char** argv) {
        if (argc < 2) {
            print_usage(argv[0]);
            return std::nullopt;
        }

        CompilerOptions opts;
        bool input_set = false;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            // ── Справка / версия ──────────────────────────
            if (arg == "-h" || arg == "--help") {
                print_usage(argv[0]);
                return std::nullopt;
            }
            if (arg == "--version") {
                print_version();
                return std::nullopt;
            }

            // ── Выходной файл ─────────────────────────────
            if (arg == "-o") {
                if (i + 1 >= argc) {
                    std::cerr << "Error: -o requires an argument\n";
                    return std::nullopt;
                }
                opts.output_name = argv[++i];
                continue;
            }

            // ── Режим вывода ──────────────────────────────
            if (arg == "-c") {
                opts.emit_mode = EmitMode::ObjectFile;
                continue;
            }
            if (arg == "--emit-ir") {
                opts.emit_mode = EmitMode::LLVMIR;
                continue;
            }
            if (arg == "--emit-asm") {
                opts.emit_mode = EmitMode::Assembly;
                continue;
            }

            // ── Оптимизация ───────────────────────────────
            if (arg == "-O0") { opts.opt_level = OptLevel::O0; continue; }
            if (arg == "-O1") { opts.opt_level = OptLevel::O1; continue; }
            if (arg == "-O2") { opts.opt_level = OptLevel::O2; continue; }
            if (arg == "-O3") { opts.opt_level = OptLevel::O3; continue; }
            if (arg == "-Os") { opts.opt_level = OptLevel::Os; continue; }

            // ── Include директории ────────────────────────
            if (arg.substr(0, 2) == "-I") {
                opts.include_dirs.push_back(arg.substr(2));
                continue;
            }

            // ── Target triple ─────────────────────────────
            if (arg.substr(0, 9) == "--target=") {
                opts.target_triple = arg.substr(9);
                continue;
            }

            // ── Отладочные флаги ──────────────────────────
            if (arg == "--dump-ast") { opts.dump_ast  = true; continue; }
            if (arg == "--dump-ir")  { opts.dump_ir   = true; continue; }
            if (arg == "--no-sema")  { opts.no_sema   = true; continue; }
            if (arg == "-v" || arg == "--verbose") {
                opts.verbose = true;
                continue;
            }

            // ── Входной файл ──────────────────────────────
            if (arg[0] != '-') {
                if (input_set) {
                    std::cerr << "Error: multiple input files not supported\n";
                    return std::nullopt;
                }
                opts.input_file = arg;
                input_set = true;
                continue;
            }

            std::cerr << "Error: unknown option '" << arg << "'\n";
            print_usage(argv[0]);
            return std::nullopt;
        }

        if (!input_set) {
            std::cerr << "Error: no input file specified\n";
            print_usage(argv[0]);
            return std::nullopt;
        }

        // Автовывод имени выходного файла если не задан -o
        if (opts.output_name == "a.out") {
            auto stem = opts.input_file.stem().string();
            switch (opts.emit_mode) {
            case EmitMode::ObjectFile:  opts.output_name = stem + ".o";  break;
            case EmitMode::LLVMIR:      opts.output_name = stem + ".ll"; break;
            case EmitMode::Assembly:    opts.output_name = stem + ".s";  break;
            case EmitMode::Executable:  opts.output_name = stem;         break;
            }
        }

        return opts;
    }

    // ═══════════════════════════════════════════════════
    // Главная точка компиляции
    // ═══════════════════════════════════════════════════

    CompileResult compile(const CompilerOptions& opts) {
        // ── 1. Читаем файл ────────────────────────────────
        std::ifstream file(opts.input_file);
        if (!file.is_open())
            return {false, "Cannot open file: " + opts.input_file.string()};

        std::ostringstream ss;
        ss << file.rdbuf();
        std::string src = ss.str();

        if (opts.verbose)
            std::cerr << "[fluxc] Compiling: " << opts.input_file << "\n";

        // ── 2. Лексер ─────────────────────────────────────
        flux::DiagEngine diag;
        flux::Lexer lexer(src, opts.input_file.string().c_str(), diag);
        auto tokens = lexer.tokenize();

        // ── 3. Препроцессор ───────────────────────────────
        flux::Preprocessor preprocessor(diag, opts.input_file.parent_path());
        auto expanded = preprocessor.process(tokens, opts.input_file);
        if (diag.has_errors()) {
            diag.print_all();
            return {false, "Preprocessing failed"};
        }

        // ── 4. Парсер ─────────────────────────────────────
        flux::Parser parser(std::move(expanded), diag);
        auto ast = parser.parse_program();
        if (diag.has_errors()) {
            diag.print_all();
            return {false, "Parsing failed"};
        }

        if (opts.dump_ast) {
            // Заглушка — реализуйте ASTPrinter аналогично LLVMCodegen
            std::cerr << "[fluxc] --dump-ast: ASTPrinter not implemented yet\n";
        }

        // ── 5. Семантический анализ ───────────────────────
        if (!opts.no_sema) {
            flux::SemanticAnalyzer sema(diag);
            sema.analyze(*ast);
            if (diag.has_errors()) {
                diag.print_all();
                return {false, "Semantic analysis failed"};
            }
            if (opts.verbose)
                std::cerr << "[fluxc] Sema OK\n";
        }

        // ── 6. Кодогенерация ──────────────────────────────
        flux::LLVMCodegen codegen(opts.input_file.stem().string());
        if (!opts.target_triple.empty())
            codegen.set_target_triple(opts.target_triple);

        ast->accept(codegen);

        if (opts.dump_ir)
            codegen.dump_ir();

        // ── 7. Эмиссия ────────────────────────────────────
        switch (opts.emit_mode) {

        case EmitMode::LLVMIR: {
            if (!codegen.emit_llvm_ir(opts.output_name))
                return {false, "Failed to emit LLVM IR"};
            if (opts.verbose)
                std::cerr << "[fluxc] IR written to: " << opts.output_name << "\n";
            break;
        }

        case EmitMode::Assembly: {
            if (!codegen.emit_assembly(opts.output_name))
                return {false, "Failed to emit assembly"};
            if (opts.verbose)
                std::cerr << "[fluxc] Assembly written to: " << opts.output_name << "\n";
            break;
        }

        case EmitMode::ObjectFile: {
            if (!codegen.emit_object_file(opts.output_name))
                return {false, "Codegen failed"};
            if (opts.verbose)
                std::cerr << "[fluxc] Object file: " << opts.output_name << "\n";
            break;
        }

        case EmitMode::Executable: {
            // Сначала объектный файл во временную директорию
            auto obj_path = std::filesystem::temp_directory_path()
                            / (opts.input_file.stem().string() + ".o");

            if (!codegen.emit_object_file(obj_path.string()))
                return {false, "Codegen failed"};

            if (opts.verbose)
                std::cerr << "[fluxc] Object file: " << obj_path << "\n";

            // Линковка
            std::string linker = find_linker();
            if (linker.empty()) {
                std::filesystem::remove(obj_path);
                return {false, "No linker found. Install clang or gcc."};
            }

            std::string link_cmd = linker
                + " " + obj_path.string()
                + " " + opt_flag(opts.opt_level)
                + " -o " + opts.output_name;

            if (opts.verbose)
                std::cerr << "[fluxc] Linking: " << link_cmd << "\n";

            int rc = std::system(link_cmd.c_str());
            std::filesystem::remove(obj_path);

            if (rc != 0)
                return {false, "Linking failed (linker: " + linker + ")"};

            if (opts.verbose)
                std::cerr << "[fluxc] Executable: " << opts.output_name
                        << "  (linked with " << linker << ")\n";
            break;
        }
        }

        return {true, {}};
    }

}

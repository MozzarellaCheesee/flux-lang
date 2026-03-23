#include <iostream>
#include <filesystem>
#include <string>
#include <fstream>
#include <optional>
#include <vector>
#include <cstdlib>

#include "flux/codegen/codegen.h"
#include "flux/lexer/lexer.h"
#include "flux/common/diagnostic.h"
#include "flux/preprocessor/preprocessor.h"
#include "flux/parser/parser.h"
#include "flux/sema/sema.h"
#include "flux/ast/ast_printer.h"

static const char* FLUXC_VERSION = "0.1.0";

enum class CompilerKind { 
    Auto, GCC, Clang, MSVC 
};

struct Options {
    std::filesystem::path input;
    std::filesystem::path output;
    CompilerKind          compiler = CompilerKind::Auto;
    bool emit_cpp   = false;
    bool no_compile = false;
    bool print_ast  = false;
    bool verbose    = false;
};

static std::string read_file(const std::filesystem::path& p) {
    std::ifstream f(p);
    if (!f.is_open()) {
        std::cerr << "error: cannot open file: " << p << '\n';
        return {};
    }
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

static bool command_exists(const std::string& cmd) {
#ifdef _WIN32
    std::string check = "where " + cmd + " >nul 2>&1";
#else
    std::string check = "which " + cmd + " >/dev/null 2>&1";
#endif
    return std::system(check.c_str()) == 0;
}

static std::optional<std::string> resolve_compiler(CompilerKind kind) {
    auto try_find = [](const std::vector<std::string>& candidates)
        -> std::optional<std::string>
    {
        for (const auto& c : candidates)
            if (command_exists(c)) return c;
        return std::nullopt;
    };

    switch (kind) {
        case CompilerKind::GCC:
            return try_find({"g++", "g++-14", "g++-13", "g++-12"});
        case CompilerKind::Clang:
            return try_find({"clang++", "clang++-18", "clang++-17", "clang++-16"});
        case CompilerKind::MSVC:
            return try_find({"cl"});
        case CompilerKind::Auto:
        default:
            // Preference: gcc > clang > msvc
            for (const auto& c : {"g++", "clang++", "cl"})
                if (command_exists(c)) return std::string(c);
            return std::nullopt;
    }
}

static std::string build_compile_cmd(const std::string& compiler,
                                    const std::string& cpp_file,
                                    const std::string& out_file) {
    bool is_msvc = (compiler == "cl");

    if (is_msvc) {
        return compiler + " /std:c++20 /EHsc /Fe:" + out_file + " " + cpp_file;
    } else {
        return compiler + " -std=c++20 -o " + out_file + " " + cpp_file;
    }
}

static void print_usage(const char* prog) {
    std::cout <<
        "Usage: " << prog << " [options] <source.flx>\n"
        "\n"
        "Options:\n"
        "  -o, --output <file>        Output executable path\n"
        "                             (default: <source_stem>" <<
#ifdef _WIN32
        ".exe"
#else
        ""
#endif
        << ")\n"
        "  --compiler <gcc|clang|msvc>\n"
        "                             C++ compiler to use for backend compilation\n"
        "                             (default: auto-detect, preference: gcc > clang > msvc)\n"
        "  --emit-cpp                 Keep the generated intermediate .cpp file\n"
        "  --no-compile               Only emit the generated .cpp, do not compile it\n"
        "  --print-ast                Print the AST to stdout after parsing\n"
        "  -v, --verbose              Print extra progress information\n"
        "  --version                  Print compiler version and exit\n"
        "  -h, --help                 Show this help message and exit\n"
        "\n"
        "Examples:\n"
        "  " << prog << " main.flx\n"
        "  " << prog << " main.flx -o build/main\n"
        "  " << prog << " main.flx --compiler clang\n"
        "  " << prog << " main.flx --emit-cpp --no-compile\n";
}

static std::optional<Options> parse_args(int argc, char** argv) {
    Options opts;
    std::vector<std::string_view> args(argv + 1, argv + argc);

    for (std::size_t i = 0; i < args.size(); ++i) {
        auto arg = args[i];

        auto next_arg = [&](std::string_view flag) -> std::optional<std::string_view> {
            if (i + 1 >= args.size()) {
                std::cerr << "error: " << flag << " requires an argument\n";
                return std::nullopt;
            }
            return args[++i];
        };

        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            std::exit(0);
        } else if (arg == "--version") {
            std::cout << "fluxc " << FLUXC_VERSION << "\n";
            std::exit(0);
        } else if (arg == "-o" || arg == "--output") {
            auto val = next_arg(arg);
            if (!val) return std::nullopt;
            opts.output = *val;
        } else if (arg == "--compiler") {
            auto val = next_arg(arg);
            if (!val) return std::nullopt;
            if (*val == "gcc")        opts.compiler = CompilerKind::GCC;
            else if (*val == "clang") opts.compiler = CompilerKind::Clang;
            else if (*val == "msvc")  opts.compiler = CompilerKind::MSVC;
            else {
                std::cerr << "error: unknown compiler '" << *val
                          << "'. Choose: gcc, clang, msvc\n";
                return std::nullopt;
            }
        } else if (arg == "--emit-cpp") {
            opts.emit_cpp = true;
        } else if (arg == "--no-compile") {
            opts.no_compile = true;
        } else if (arg == "--print-ast") {
            opts.print_ast = true;
        } else if (arg == "-v" || arg == "--verbose") {
            opts.verbose = true;
        } else if (arg.starts_with('-')) {
            std::cerr << "error: unknown option '" << arg << "'\n";
            return std::nullopt;
        } else {
            if (!opts.input.empty()) {
                std::cerr << "error: multiple input files are not supported\n";
                return std::nullopt;
            }
            opts.input = arg;
        }
    }

    if (opts.input.empty()) {
        std::cerr << "error: no input file specified\n";
        print_usage(argv[0]);
        return std::nullopt;
    }

    if (!std::filesystem::exists(opts.input)) {
        std::cerr << "error: file not found: " << opts.input << "\n";
        return std::nullopt;
    }

    // Default output path
    if (opts.output.empty() && !opts.no_compile) {
        opts.output = opts.input.parent_path() /
                      opts.input.stem()
#ifdef _WIN32
                      += ".exe"
#endif
            ;
    }

    return opts;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(int argc, char** argv) {
    auto opts_opt = parse_args(argc, argv);
    if (!opts_opt) return 1;
    const Options& opts = *opts_opt;

    // ── Read source ──────────────────────────────────────────────────────────
    std::string source = read_file(opts.input);
    if (source.empty()) return 1;

    if (opts.verbose)
        std::cout << "Compiling: " << opts.input << "\n";

    flux::DiagEngine diag;

    // ── 1. Lexer ─────────────────────────────────────────────────────────────
    flux::Lexer lexer(source, opts.input.string(), diag);
    auto tokens = lexer.tokenize();

    if (diag.has_errors()) { diag.print_all(); return 1; }

    // ── 2. Preprocessor ──────────────────────────────────────────────────────
    flux::Preprocessor preprocessor(diag, opts.input.parent_path());
    auto processed = preprocessor.process(tokens, opts.input);

    if (diag.has_errors()) { diag.print_all(); return 2; }

    // ── 3. Parser ────────────────────────────────────────────────────────────
    flux::Parser parser(std::move(processed), diag);
    auto program = parser.parse_program();

    if (diag.has_errors()) { diag.print_all(); return 3; }

    if (!program) {
        std::cerr << "error: parse failed\n";
        return 4;
    }

    // ── 4. Semantic analysis ─────────────────────────────────────────────────
    flux::SemanticAnalyzer sema(diag);
    sema.analyze(*program);

    if (diag.has_errors()) { diag.print_all(); return 5; }

    // ── 5. AST dump (optional) ───────────────────────────────────────────────
    if (opts.print_ast) {
        flux::ASTPrinter printer(std::cout);
        program->accept(printer);
    }

    // ── 6. Code generation ───────────────────────────────────────────────────
    flux::CodeGen codegen;
    std::string cpp_code = codegen.generate(*program);

    // Place the intermediate .cpp next to the output (or input if no-compile)
    std::filesystem::path cpp_file = opts.no_compile
        ? opts.input.parent_path() / (opts.input.stem().string() + ".cpp")
        : opts.output.parent_path() / (opts.output.stem().string() + ".cpp");

    {
        std::ofstream out(cpp_file);
        if (!out) {
            std::cerr << "error: cannot write " << cpp_file << "\n";
            return 6;
        }
        out << cpp_code;
    }

    if (opts.verbose)
        std::cout << "Generated C++: " << cpp_file << "\n";

    if (opts.no_compile) {
        std::cout << "Emitted: " << cpp_file << "\n";
        return 0;
    }

    // ── 7. Resolve C++ compiler ───────────────────────────────────────────────
    auto compiler = resolve_compiler(opts.compiler);
    if (!compiler) {
        std::cerr << "error: no suitable C++ compiler found.\n"
                     "       Install g++, clang++, or MSVC and make sure it is in PATH.\n"
                     "       Alternatively, specify one with --compiler <gcc|clang|msvc>\n";
        std::filesystem::remove(cpp_file);
        return 7;
    }

    if (opts.verbose)
        std::cout << "Using compiler: " << *compiler << "\n";

    // ── 8. Compile generated C++ ──────────────────────────────────────────────
    std::string cmd = build_compile_cmd(*compiler,
                                        cpp_file.string(),
                                        opts.output.string());
    if (opts.verbose)
        std::cout << "Running: " << cmd << "\n";

    int rc = std::system(cmd.c_str());

    if (rc != 0) {
        std::cerr << "error: C++ compilation failed (exit code " << rc << ")\n";
        if (!opts.emit_cpp) std::filesystem::remove(cpp_file);
        return 8;
    }

    // ── 9. Cleanup ────────────────────────────────────────────────────────────
    if (!opts.emit_cpp)
        std::filesystem::remove(cpp_file);

    std::cout << "Built: " << opts.output << "\n";
    return 0;
}

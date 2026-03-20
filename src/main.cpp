#include "flux/lexer/lexer.h"
#include "flux/common/diagnostic.h"
#include "flux/parser/parser.h"
#include "flux/preprocessor/preprocessor.h"
#include "flux/sema/sema.h"
#include "flux/codegen/codegen.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdlib>

using namespace flux;

static std::string find_linker() {
    for (auto* cmd : {"clang", "clang-18", "clang-17", "clang-16", "gcc", "cc"}) {
        std::string test = std::string("which ") + cmd + " > /dev/null 2>&1";
        if (std::system(test.c_str()) == 0)
            return cmd;
    }
    return "";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: fluxc <file.flx> [-o output]\n";
        return 1;
    }

    std::filesystem::path filepath = argv[1];
    std::string output_name = "a.out";
    for (int i = 2; i < argc - 1; ++i)
        if (std::string(argv[i]) == "-o")
            output_name = argv[i + 1];

    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Cannot open file: " << filepath << "\n";
        return 1;
    }
    std::ostringstream ss; ss << file.rdbuf();
    std::string src = ss.str();

    flux::DiagEngine diag;

    flux::Lexer lexer(src, filepath.string().c_str(), diag);
    auto tokens = lexer.tokenize();

    flux::Preprocessor preprocessor(diag, filepath.parent_path());
    auto expanded = preprocessor.process(tokens, filepath);
    if (diag.has_errors()) { diag.print_all(); return 1; }

    flux::Parser parser(std::move(expanded), diag);
    auto ast = parser.parse_program();

    flux::SemanticAnalyzer sema(diag);
    sema.analyze(*ast);
    if (diag.has_errors()) { diag.print_all(); return 1; }

    // ── Кодогенерация ─────────────────────────────────────
    flux::LLVMCodegen codegen(filepath.stem().string());
    ast->accept(codegen);

    std::string obj_path = filepath.stem().string() + ".o";
    if (!codegen.emit_object_file(obj_path)) {
        std::cerr << "Codegen failed\n";
        return 1;
    }
    std::cout << "Object file: " << obj_path << "\n";

    // ── Линковка через системный линкер ───────────────────
    std::string linker = find_linker();
    if (linker.empty()) {
        std::cerr << "Linking failed: no linker found.\n"
                  << "Install one: sudo apt install clang\n";
        return 1;
    }

    std::string link_cmd = linker + " " + obj_path + " -o " + output_name;
    if (std::system(link_cmd.c_str()) != 0) {
        std::cerr << "Linking failed (linker: " << linker << ")\n";
        return 1;
    }
    std::cout << "Executable: " << output_name
              << "  (linked with " << linker << ")\n";
    return 0;
}

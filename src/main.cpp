#include "flux/lexer/lexer.h"
#include "flux/common/diagnostic.h"
#include "flux/parser/parser.h"
#include "flux/preprocessor/preprocessor.h"
#include "flux/sema/sema.h"
#include <iostream>
#include <filesystem>
#include <fstream>

using namespace flux;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: fluxc <file.flx>\n";
        return 1;
    }

    std::filesystem::path filepath = argv[1];
    // Читаем файл
    std::ifstream file(filepath);
    
    if (!file.is_open()) {
        std::cerr << "Cannot open file: " << filepath << "\n";
        return 1;
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    std::string src = ss.str();

    flux::DiagEngine diag;
    // Лексер
    flux::Lexer lexer(src, filepath.string().c_str(), diag);
    auto tokens = lexer.tokenize();

    // Препроцессор — base_dir = директория входного файла
    flux::Preprocessor preprocessor(diag, filepath.parent_path());
    auto expanded = preprocessor.process(tokens, filepath);

    if (diag.has_errors()) { diag.print_all(); return 1; }

    // Парсер
    flux::Parser parser(std::move(expanded), diag);
    auto ast = parser.parse_program();

    // Семантический анализатор
    flux::SemanticAnalyzer sema(diag);
    sema.analyze(*ast);

    if (diag.has_errors()) {
        diag.print_all();
        return 1;
    }

    std::cout << "OK: " << ast->decls.size() << " top-level decls\n";
    std::cout << "Sema OK\n";
    return 0;
}
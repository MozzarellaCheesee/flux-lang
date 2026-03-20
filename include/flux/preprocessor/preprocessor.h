#pragma once
#include "flux/lexer/token.h"
#include "flux/common/diagnostic.h"
#include <vector>
#include <string>
#include <unordered_set>
#include <filesystem>

namespace flux {

class Preprocessor {
public:
    Preprocessor(DiagEngine& diag, std::filesystem::path base_dir);

    // Принимает токены одного файла, возвращает развёрнутый поток
    // со всеми #import подставленными рекурсивно
    std::vector<Token> process(const std::vector<Token>& tokens, const std::filesystem::path& current_file);

private:
    // Читает файл и токенизирует его
    std::vector<Token> load_and_tokenize(const std::filesystem::path& path);

    DiagEngine&                     diag_;
    std::filesystem::path           base_dir_;      // корневая директория проекта
    std::unordered_set<std::string> visited_; // защита от циклических импортов
    std::vector<std::string>        source_store_;
};

}

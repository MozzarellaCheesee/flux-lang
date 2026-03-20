#include "flux/preprocessor/preprocessor.h"
#include "flux/lexer/lexer.h"
#include <fstream>
#include <sstream>

namespace flux {

    Preprocessor::Preprocessor(DiagEngine& diag, std::filesystem::path base_dir)
        : diag_(diag), base_dir_(std::move(base_dir)) {}

    std::vector<Token> Preprocessor::process(const std::vector<Token>& tokens, const std::filesystem::path& current_file) {
        std::vector<Token> result;

        for (size_t i = 0; i < tokens.size(); ++i) {
            const Token& tok = tokens[i];

            // Не PP_IMPORT — копируем токен как есть
            if (tok.kind != TokenKind::PP_IMPORT) {
                result.push_back(tok);
                continue;
            }

            // PP_IMPORT — следующий токен должен быть LIT_STRING с путём
            ++i;
            if (i >= tokens.size() || tokens[i].kind != TokenKind::LIT_STRING) {
                diag_.emit(DiagLevel::Error, tok.loc, "Expected file path after #import");
                continue;
            }

            // Извлекаем путь из "..." — убираем кавычки
            std::string raw = std::string(tokens[i].lexeme);
            std::string path_str = raw.substr(1, raw.size() - 2); // убираем " "

            // Путь относительно текущего файла
            std::filesystem::path import_path =
                current_file.parent_path() / path_str;
            import_path = std::filesystem::weakly_canonical(import_path);

            std::string canonical = import_path.string();

            // Защита от циклических импортов
            if (visited_.count(canonical)) {
                diag_.emit(DiagLevel::Warning, tok.loc, "Circular import ignored: " + canonical);
                continue;
            }
            visited_.insert(canonical);

            // Проверяем существование файла
            if (!std::filesystem::exists(import_path)) {
                diag_.emit(DiagLevel::Error, tok.loc, "Import file not found: " + canonical);
                continue;
            }

            // Загружаем, токенизируем и рекурсивно обрабатываем импортируемый файл
            auto imported_tokens = load_and_tokenize(import_path);
            auto expanded = process(imported_tokens, import_path); // рекурсия

            // Вставляем все токены кроме END_OF_FILE
            for (auto& t : expanded) {
                if (t.kind != TokenKind::END_OF_FILE)
                    result.push_back(t);
            }
        }

        return result;
    }

    std::vector<Token> Preprocessor::load_and_tokenize(const std::filesystem::path& path) {
        // Читаем файл целиком в строку
        std::ifstream file(path);
        if (!file.is_open()) {
            diag_.emit(DiagLevel::Error, {}, "Cannot open file: " + path.string());
            return {};
        }

        std::ostringstream ss;
        ss << file.rdbuf();
        std::string source = ss.str();

        // Храним source чтобы string_view внутри токенов не протух
        // Лексер принимает string_view — source должен жить дольше токенов.
        // Поэтому передаём через source_store_.
        source_store_.push_back(std::move(source));
        const std::string& stored = source_store_.back();

        Lexer lexer(stored, path.string().c_str(), diag_);
        return lexer.tokenize();
    }
} 

#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace flux {

// VS Code CompletionItemKind numbers
namespace ck {
    constexpr int Method   = 2;
    constexpr int Function = 3;
    constexpr int Field    = 5;
    constexpr int Variable = 6;
    constexpr int Class    = 7;
    constexpr int Keyword  = 14;
    constexpr int Struct   = 22;
}

struct CompletionItem {
    std::string label;
    int         kind;    // VS Code CompletionItemKind
    std::string detail;  // тип переменной / сигнатура функции

    CompletionItem(std::string l, int k, std::string d)
        : label(std::move(l)), kind(k), detail(std::move(d)) {}
};

// Возвращает список вариантов автодополнения для позиции (line, col) (1-based).
// source  — полный текст файла
// filepath — путь к файлу (для #import и диагностики)
std::vector<CompletionItem> compute_completions(
    const std::string& source,
    const std::string& filepath,
    uint32_t line,
    uint32_t col
);

struct HoverResult {
    std::string markdown; // empty string = no hover info
};

HoverResult compute_hover(
    const std::string& source,
    const std::string& filepath,
    uint32_t line,   // 1-based
    uint32_t col     // 1-based
);

struct DefinitionResult {
    std::string filepath; // пустая строка = не найдено
    uint32_t    line = 0; // 1-based
    uint32_t    col  = 0; // 1-based
};

DefinitionResult compute_definition(
    const std::string& source,
    const std::string& filepath,
    uint32_t line,   // 1-based
    uint32_t col     // 1-based
);

} // namespace flux

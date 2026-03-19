#pragma once
#include "./token_kind.h"
#include "../common/source_location.h"
#include <string_view>

namespace flux {
    struct Token {
        TokenKind      kind;
        std::string_view lexeme;   // view в исходный буфер, без копирования
        SourceLocation loc;
    };

    std::string_view token_kind_name(TokenKind kind);
}

#pragma once
#include "flux/lexer/token.h"
#include "flux/common/diagnostic.h"
#include "token_kind.h"
#include <string_view>
#include <unordered_map>
#include <vector>

namespace flux {

    static const std::unordered_map<std::string_view, TokenKind> KEYWORDS = {
        {"fnc",      TokenKind::KW_FNC},
        {"let",      TokenKind::KW_LET},
        {"return",   TokenKind::KW_RETURN},
        {"for",      TokenKind::KW_FOR},
        {"while",    TokenKind::KW_WHILE},
        {"if",       TokenKind::KW_IF},
        {"else",     TokenKind::KW_ELSE},
        {"continue", TokenKind::KW_CONTINUE},
        {"break",    TokenKind::KW_BREAK},
        {"true",     TokenKind::KW_TRUE},
        {"false",    TokenKind::KW_FALSE},
        {"bool",     TokenKind::KW_BOOL},
        {"int8",     TokenKind::KW_INT8},
        {"int16",    TokenKind::KW_INT16},
        {"int32",    TokenKind::KW_INT32},
        {"int64",    TokenKind::KW_INT64},
        {"int128",   TokenKind::KW_INT128},
        {"uint8",    TokenKind::KW_UINT8},
        {"uint16",   TokenKind::KW_UINT16},
        {"uint32",   TokenKind::KW_UINT32},
        {"uint64",   TokenKind::KW_UINT64},
        {"uint128",  TokenKind::KW_UINT128},
        {"float",    TokenKind::KW_FLOAT},
        {"double",   TokenKind::KW_DOUBLE},
        {"isize_t",  TokenKind::KW_ISIZE_T},
        {"usize_t",  TokenKind::KW_USIZE_T},
        {"str",      TokenKind::KW_STR},
        {"string",   TokenKind::KW_STRING},
        {"match",    TokenKind::KW_MATCH},
        {"_",        TokenKind::UNDERSCORE},
        {"pub",      TokenKind::KW_PUB},
        {"struct",   TokenKind::KW_STRUCT},
        {"class",    TokenKind::KW_CLASS},
        {"impl",     TokenKind::KW_IMPL},
        {"self",     TokenKind::KW_SELF},
    };

    class Lexer {
    public:
        Lexer(std::string_view source, const char* filepath, DiagEngine& diag);
        std::vector<Token> tokenize();
    private:
        Token next_token();
        Token make_token(TokenKind kind, size_t start);

        char advance();
        char peek() const;
        char peek_next() const;
        bool at_end() const;

        void skip_whitespace_and_comments();

        Token lex_ident_or_keyword(size_t start);
        Token lex_number(size_t start);
        Token lex_string(size_t start);
        Token lex_hash_directive(size_t start);

        std::string_view source_;
        const char*      filepath_;
        DiagEngine&      diag_;
        size_t           cursor_ = 0;
        uint32_t         line_   = 1;
        uint32_t         col_    = 1;
    };
}

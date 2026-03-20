#pragma once
namespace flux {
    enum class TokenKind {
        // ── Ключевые слова ──────────────────────────────
        KW_FNC, KW_LET, KW_RETURN, KW_FOR, KW_WHILE,
        KW_IF, KW_ELSE, KW_CONTINUE, KW_BREAK,
        KW_TRUE, KW_FALSE, KW_MATCH, KW_PUB,
        KW_STRUCT, KW_CLASS, KW_IMPL, KW_SELF,

        // ── Встроенные типы ─────────────────────────────
        KW_BOOL,
        KW_INT8, KW_INT16, KW_INT32, KW_INT64, KW_INT128,
        KW_UINT8, KW_UINT16, KW_UINT32, KW_UINT64, KW_UINT128,
        KW_FLOAT, KW_DOUBLE,
        KW_ISIZE_T,
        KW_USIZE_T,
        KW_STR,     // &str — ключевое слово
        KW_STRING,

        // ── Литералы ────────────────────────────────────
        LIT_INT,    // 5, 15
        LIT_FLOAT,  // 3.14
        LIT_STRING, // "abcd"

        // ── Идентификатор ───────────────────────────────
        IDENT,

        // ── Арифметические операторы ────────────────────
        PLUS,       // +
        MINUS,      // -
        STAR,       // *
        SLASH,      // /
        PERCENT,    // %
        TILDE,      // ~

        // ── Унарные операторы ───────────────────────────
        BANG,       // !

        // ── Побитовые операторы ─────────────────────────
        AMP_AMP,    // &&
        PIPE,       // |
        PIPE_PIPE,  // ||
        CARET,      // ^  (XOR)
        LSHIFT,     // <<
        RSHIFT,     // >>

        // ── Составное присваивание ──────────────────────
        PLUS_EQ,    // +=
        MINUS_EQ,   // -=
        STAR_EQ,    // *=
        SLASH_EQ,   // /=
        PERCENT_EQ, // %=


        // ── Инкремент / декремент ───────────────────────
        PLUS_PLUS,   // ++
        MINUS_MINUS, // --

        // ── Операторы сравнения ─────────────────────────
        EQ_EQ,   // ==
        BANG_EQ, // !=
        LT,      // <
        GT,      // >
        LT_EQ,   // <=
        GT_EQ,   // >=

        // ── Присваивание ────────────────────────────────
        EQ,      // =

        // ── Ссылки/указатели ────────────────────────────
        AMP,     // &
        DOT,     // .

        // ── Пунктуация ──────────────────────────────────
        LPAREN, RPAREN,     // ( )
        LBRACKET, RBRACKET, // [ ]
        LBRACE, RBRACE,     // { }
        COLON,              // :
        COMMA,              // ,
        SEMICOLON,          // ;
        ARROW,              // ->
        FAT_ARROW,          // =>
        

        // ── Препроцессор ────────────────────────────────
        PP_IMPORT,   // #import

        // ── Служебные ───────────────────────────────────
        END_OF_FILE, 
        HASH,        // #
        COLON_COLON, // ::
        QUESTION,    // ?
        UNDERSCORE,  // _ 
    };
}
#include "flux/lexer/lexer.h"

namespace flux {
    Lexer::Lexer(std::string_view source, const char* filepath, DiagEngine& diag) 
        : source_(source), filepath_(filepath), diag_(diag) {}

    std::vector<Token> Lexer::tokenize() {
        std::vector<Token> tokens;
        while (!at_end()) {
            skip_whitespace_and_comments();
            if (at_end()) break;
            tokens.push_back(next_token());
        }
        tokens.push_back(make_token(TokenKind::END_OF_FILE, cursor_));
        return tokens;
    }

    Token Lexer::next_token() {
        size_t start = cursor_;
        char c = advance();

        switch (c) {
            case '(': return make_token(TokenKind::LPAREN,    start);
            case ')': return make_token(TokenKind::RPAREN,    start);
            case '{': return make_token(TokenKind::LBRACE,    start);
            case '}': return make_token(TokenKind::RBRACE,    start);
            case '[': return make_token(TokenKind::LBRACKET,  start);
            case ']': return make_token(TokenKind::RBRACKET,  start);
            case ';': return make_token(TokenKind::SEMICOLON, start);
            case ',': return make_token(TokenKind::COMMA,     start);
            case ':':
                if (peek() == ':') { advance(); return make_token(TokenKind::COLON_COLON, start); }
                return make_token(TokenKind::COLON, start);
            case '.': return make_token(TokenKind::DOT, start);
            case '?': return make_token(TokenKind::QUESTION, start);
            case '~': return make_token(TokenKind::TILDE, start);
            case '^': return make_token(TokenKind::CARET, start);
            case '%':
                if (peek() == '=') { advance(); return make_token(TokenKind::PERCENT_EQ, start); }
                return make_token(TokenKind::PERCENT, start);
            case '*':
                if (peek() == '=') { advance(); return make_token(TokenKind::STAR_EQ, start); }
                return make_token(TokenKind::STAR, start);
            case '/':
                if (peek() == '=') { advance(); return make_token(TokenKind::SLASH_EQ, start); }
                // комментарии обрабатываются в skip_whitespace_and_comments
                return make_token(TokenKind::SLASH, start);
            case '+':
                if (peek() == '+') { advance(); return make_token(TokenKind::PLUS_PLUS,  start); }
                if (peek() == '=') { advance(); return make_token(TokenKind::PLUS_EQ,    start); }
                return make_token(TokenKind::PLUS, start);
            case '-':
                if (peek() == '-') { advance(); return make_token(TokenKind::MINUS_MINUS, start); }
                if (peek() == '=') { advance(); return make_token(TokenKind::MINUS_EQ,    start); }
                if (peek() == '>') { advance(); return make_token(TokenKind::ARROW,        start); }
                return make_token(TokenKind::MINUS, start);
            case '=':
                if (peek() == '=') { advance(); return make_token(TokenKind::EQ_EQ, start); }
                return make_token(TokenKind::EQ, start);
            case '!':
                if (peek() == '=') { advance(); return make_token(TokenKind::BANG_EQ, start); }
                return make_token(TokenKind::BANG, start);
            case '<':
                if (peek() == '<') { advance(); return make_token(TokenKind::LSHIFT, start); }
                if (peek() == '=') { advance(); return make_token(TokenKind::LT_EQ,  start); }
                return make_token(TokenKind::LT, start);
            case '>':
                if (peek() == '>') { advance(); return make_token(TokenKind::RSHIFT, start); }
                if (peek() == '=') { advance(); return make_token(TokenKind::GT_EQ,  start); }
                return make_token(TokenKind::GT, start);
            case '&':
                if (peek() == '&') { advance(); return make_token(TokenKind::AMP_AMP, start); }
                return make_token(TokenKind::AMP, start);
            case '|':
                if (peek() == '|') { advance(); return make_token(TokenKind::PIPE_PIPE, start); }
                return make_token(TokenKind::PIPE, start);
            case '#':
                return lex_hash_directive(start);  // разбирает #import
            case '"':
                return lex_string(start);
            default:
                if (std::isdigit(c)) return lex_number(start);
                if (std::isalpha(c) || c == '_') return lex_ident_or_keyword(start);
                diag_.emit(DiagLevel::Error, {filepath_, line_, col_}, "Unexpected character");
                return make_token(TokenKind::END_OF_FILE, start); // recovery
        }
    }

    Token Lexer::make_token(TokenKind kind, size_t start) {
        return Token {
            .kind   = kind,
            .lexeme = source_.substr(start, cursor_ - start),
            .loc    = SourceLocation { filepath_, line_, col_ }
        };
    }
    
    char Lexer::advance() {
        char c = source_[cursor_++];
        if (c == '\n') { line_++; col_ = 1; }
        else           { col_++; }
        return c;
    }

    char Lexer::peek() const {
        if (at_end()) return '\0';
        return source_[cursor_];
    }

    char Lexer::peek_next() const {
        if (cursor_ + 1 >= source_.size()) return '\0';
        return source_[cursor_ + 1];
    }

    bool Lexer::at_end() const {
        return cursor_ >= source_.size();
    }

    void Lexer::skip_whitespace_and_comments() {
        while (!at_end()) {
            char c = peek();
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                advance();
            } else if (c == '/' && peek_next() == '/') {
                // однострочный комментарий — до конца строки
                while (!at_end() && peek() != '\n')
                    advance();
            } else if (c == '/' && peek_next() == '*') {
                // многострочный комментарий /* ... */
                advance(); advance(); // съедаем /*
                while (!at_end()) {
                    if (peek() == '*' && peek_next() == '/') {
                        advance(); advance(); // съедаем */
                        break;
                    }
                    advance();
                }
            } else {
                break;
            }
        }
    }

    Token Lexer::lex_ident_or_keyword(size_t start) {
        while (!at_end() && (std::isalnum(peek()) || peek() == '_')) advance();
        std::string_view lexeme = source_.substr(start, cursor_ - start);
        auto it = KEYWORDS.find(lexeme);
        TokenKind kind = (it != KEYWORDS.end()) ? it->second : TokenKind::IDENT;
        return make_token(kind, start);
    }

    Token Lexer::lex_number(size_t start) {
        while (!at_end() && std::isdigit(peek()))
            advance();

        // проверяем: это float?
        if (!at_end() && peek() == '.' && std::isdigit(peek_next())) {
            advance(); // съедаем '.'
            while (!at_end() && std::isdigit(peek()))
                advance();
            return make_token(TokenKind::LIT_FLOAT, start);
        }

        return make_token(TokenKind::LIT_INT, start);
    }

    Token Lexer::lex_string(size_t start) {
        // курсор уже после открывающей "
        while (!at_end() && peek() != '"') {
            if (peek() == '\\') advance(); // escape-последовательность — съедаем оба символа
            advance();
        }
        if (at_end()) {
            diag_.emit(DiagLevel::Error, {filepath_, line_, col_}, "Unterminated string literal");
            return make_token(TokenKind::END_OF_FILE, start);
        }
        advance(); // закрывающая "
        return make_token(TokenKind::LIT_STRING, start);
    }

    Token Lexer::lex_hash_directive(size_t start) {
        // курсор уже после #, пропускаем пробелы
        while (!at_end() && peek() == ' ') advance();

        size_t kw_start = cursor_;
        while (!at_end() && std::isalpha(peek())) advance();

        std::string_view directive = source_.substr(kw_start, cursor_ - kw_start);
        if (directive == "import")
            return make_token(TokenKind::PP_IMPORT, start);

        diag_.emit(DiagLevel::Error, {filepath_, line_, col_},
                "Unknown preprocessor directive");
        return make_token(TokenKind::END_OF_FILE, start);
    }

}

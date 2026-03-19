#include "flux/parser/parser.h"

namespace flux {

    Parser::Parser(std::vector<Token> tokens, DiagEngine& diag)
        : tokens_(std::move(tokens)), diag_(diag) {}

    const Token& Parser::current() const {
        return tokens_[pos_];
    }

    const Token& Parser::previous() const {
        return tokens_[pos_ - 1];
    }

    bool Parser::check(TokenKind kind) const {
        return current().kind == kind;
    }

    Token Parser::consume() {
        if (!check(TokenKind::END_OF_FILE)) pos_++;
        return tokens_[pos_ - 1];
    }

    bool Parser::match(TokenKind kind) {
        if (!check(kind)) return false;
        consume();
        return true;
    }

    Token Parser::expect(TokenKind kind, const char* msg) {
        if (!check(kind)) {
            diag_.emit(DiagLevel::Error, current().loc,
                std::string("Expected '") + msg +
                "', got '" + std::string(current().lexeme) + "'");
            // не двигаемся — даём шанс вышестоящему коду восстановиться
            return current();
        }
        return consume();
    }

    // Точка входа — читает весь файл и возвращает корневой узел
    std::unique_ptr<ProgramNode> Parser::parse_program() {
        auto prog = std::make_unique<ProgramNode>();
        prog->loc = current().loc;

        while (!check(TokenKind::END_OF_FILE)) {
            auto node = parse_top_level();
            if (node) prog->decls.push_back(std::move(node));
        }
        return prog;
    }

    // Определяет тип верхнеуровневой конструкции
    std::unique_ptr<ASTNode> Parser::parse_top_level() {
        // pub — необязательный модификатор видимости
        bool is_pub = match(TokenKind::KW_PUB);

        if (check(TokenKind::KW_FNC))    return parse_func_decl(is_pub);
        if (check(TokenKind::KW_STRUCT)) return parse_struct_decl(is_pub);
        if (check(TokenKind::KW_CLASS))  return parse_class_decl(is_pub);

        if (check(TokenKind::KW_IMPL)) {
            if (is_pub)
                diag_.emit(DiagLevel::Error, current().loc, "impl cannot be pub");
            return parse_impl_decl();
        }

        // #import "path" — пропускаем, препроцессор обработает отдельно
        if (check(TokenKind::PP_IMPORT)) {
            consume();
            expect(TokenKind::LIT_STRING, "import path");
            return nullptr;
        }

        diag_.emit(DiagLevel::Error, current().loc,
                "Expected top-level declaration (fnc, struct, class, impl)");
        consume(); // recovery — съедаем неожиданный токен
        return nullptr;
    }

    // fnc name(params) -> ReturnType { body }
    std::unique_ptr<FuncDecl> Parser::parse_func_decl(bool is_pub) { // Парсинг объявления функции
        auto loc = current().loc;
        expect(TokenKind::KW_FNC, "fnc");
        auto name = std::string(expect(TokenKind::IDENT, "function name").lexeme);
        expect(TokenKind::LPAREN, "(");

        bool has_self = false;
        std::vector<std::unique_ptr<ParamDecl>> params;

        // self как первый параметр — только в методах impl/class
        if (check(TokenKind::KW_SELF)) {
            consume();
            has_self = true;
            match(TokenKind::COMMA); // запятая после self необязательна если нет других параметров
        }

        // остальные параметры: name: Type, name: Type, ...
        while (!check(TokenKind::RPAREN) && !check(TokenKind::END_OF_FILE)) {
            auto ploc  = current().loc;
            auto pname = std::string(expect(TokenKind::IDENT, "parameter name").lexeme);
            expect(TokenKind::COLON, ":");
            auto ptype = parse_type();

            auto param = std::make_unique<ParamDecl>(pname, std::move(ptype));
            param->loc = ploc;
            params.push_back(std::move(param));

            if (!match(TokenKind::COMMA)) break;
        }
        expect(TokenKind::RPAREN, ")");
        expect(TokenKind::ARROW, "->");
        auto ret  = parse_type();
        auto body = parse_block();

        auto node = std::make_unique<FuncDecl>(
            is_pub, has_self, name,
            std::move(params), std::move(ret), std::move(body));
        node->loc = loc;
        return node;
    }

    // pub struct Name { pub field: Type, field: Type, }
    std::unique_ptr<StructDecl> Parser::parse_struct_decl(bool is_pub) { // Парсинг объявления структуры
        auto loc = current().loc;
        expect(TokenKind::KW_STRUCT, "struct");
        auto name = std::string(expect(TokenKind::IDENT, "struct name").lexeme);
        expect(TokenKind::LBRACE, "{");

        auto node = std::make_unique<StructDecl>(is_pub, name);
        node->loc = loc;

        while (!check(TokenKind::RBRACE) && !check(TokenKind::END_OF_FILE))
            node->fields.push_back(parse_field_decl());

        expect(TokenKind::RBRACE, "}");
        return node;
    }

    // pub field: Type,
    std::unique_ptr<FieldDecl> Parser::parse_field_decl() { // Парсмнг полей структуры
        auto loc    = current().loc;
        bool is_pub = match(TokenKind::KW_PUB);
        auto name   = std::string(expect(TokenKind::IDENT, "field name").lexeme);
        expect(TokenKind::COLON, ":");
        auto type   = parse_type();
        match(TokenKind::COMMA); // завершающая запятая

        auto node = std::make_unique<FieldDecl>(is_pub, name, std::move(type));
        node->loc = loc;
        return node;
    }

    // impl TypeName { methods }
    std::unique_ptr<ImplDecl> Parser::parse_impl_decl() { // Парсинг реализации структуры (её методов)
        auto loc = current().loc;
        expect(TokenKind::KW_IMPL, "impl");
        auto target = std::string(expect(TokenKind::IDENT, "type name").lexeme);
        expect(TokenKind::LBRACE, "{");

        auto node = std::make_unique<ImplDecl>(target);
        node->loc = loc;

        while (!check(TokenKind::RBRACE) && !check(TokenKind::END_OF_FILE)) {
            bool is_pub = match(TokenKind::KW_PUB);
            node->methods.push_back(parse_func_decl(is_pub));
        }
        expect(TokenKind::RBRACE, "}");
        return node;
    }

    // pub class Name { fields + methods вместе }
    std::unique_ptr<ClassDecl> Parser::parse_class_decl(bool is_pub) { // Парсинг класса
        auto loc = current().loc;
        expect(TokenKind::KW_CLASS, "class");
        auto name = std::string(expect(TokenKind::IDENT, "class name").lexeme);
        expect(TokenKind::LBRACE, "{");

        auto node = std::make_unique<ClassDecl>(is_pub, name);
        node->loc = loc;

        while (!check(TokenKind::RBRACE) && !check(TokenKind::END_OF_FILE)) {
            bool member_pub = match(TokenKind::KW_PUB);
            if (check(TokenKind::KW_FNC))
                node->methods.push_back(parse_func_decl(member_pub));
            else
                node->fields.push_back(parse_field_decl());
        }
        expect(TokenKind::RBRACE, "}");
        return node;
    }

    // { stmt* }
    std::unique_ptr<BlockStmt> Parser::parse_block() {
        auto loc = current().loc;
        expect(TokenKind::LBRACE, "{");

        auto block = std::make_unique<BlockStmt>();
        block->loc = loc;

        while (!check(TokenKind::RBRACE) && !check(TokenKind::END_OF_FILE))
            block->stmts.push_back(parse_stmt());

        expect(TokenKind::RBRACE, "}");
        return block;
    }

    // Диспетчер стейтментов по первому токену
    std::unique_ptr<ASTNode> Parser::parse_stmt() {
        switch (current().kind) {
            case TokenKind::KW_LET:
                return parse_let_stmt();
            case TokenKind::KW_RETURN:
                return parse_return_stmt();
            case TokenKind::KW_IF:
                return parse_if_stmt();
            case TokenKind::KW_FOR:
                return parse_for_stmt();
            case TokenKind::KW_WHILE:
                return parse_while_stmt();
            case TokenKind::KW_CONTINUE: {
                auto loc = current().loc;
                consume();
                expect(TokenKind::SEMICOLON, ";");
                auto node = std::make_unique<ContinueStmt>();
                node->loc = loc;
                return node;
            }
            case TokenKind::KW_BREAK: {
                auto loc = current().loc;
                consume();
                expect(TokenKind::SEMICOLON, ";");
                auto node = std::make_unique<BreakStmt>();
                node->loc = loc;
                return node;
            }
            default: {
                // любое выражение как стейтмент: вызов, присваивание, i++
                auto expr = parse_expr();
                expect(TokenKind::SEMICOLON, ";");
                return std::make_unique<ExprStmt>(std::move(expr));
            }
        }
    }

    // let name: Type = expr;  или  let name: Type;  или  let name = expr;
    std::unique_ptr<VarDecl> Parser::parse_let_stmt() {
        auto loc = current().loc;
        expect(TokenKind::KW_LET, "let");
        auto name = std::string(expect(TokenKind::IDENT, "variable name").lexeme);

        // тип необязателен если есть инициализатор
        std::unique_ptr<TypeNode> type;
        if (match(TokenKind::COLON))
            type = parse_type();

        // инициализатор необязателен
        std::unique_ptr<ASTNode> init;
        if (match(TokenKind::EQ))
            init = parse_expr();

        expect(TokenKind::SEMICOLON, ";");

        auto node = std::make_unique<VarDecl>(name, std::move(type), std::move(init));
        node->loc = loc;
        return node;
    }

    // return expr;  или  return;
    std::unique_ptr<ReturnStmt> Parser::parse_return_stmt() {
        auto loc = current().loc;
        expect(TokenKind::KW_RETURN, "return");

        std::unique_ptr<ASTNode> val;
        if (!check(TokenKind::SEMICOLON))
            val = parse_expr();

        expect(TokenKind::SEMICOLON, ";");
        auto node = std::make_unique<ReturnStmt>(std::move(val));
        node->loc = loc;
        return node;
    }

    // if (cond) { } else if (cond) { } else { }
    std::unique_ptr<IfStmt> Parser::parse_if_stmt() {
        auto loc = current().loc;
        expect(TokenKind::KW_IF, "if");
        expect(TokenKind::LPAREN, "(");
        auto cond = parse_expr();
        expect(TokenKind::RPAREN, ")");
        auto then = parse_block();

        std::unique_ptr<ASTNode> else_branch;
        if (match(TokenKind::KW_ELSE)) {
            if (check(TokenKind::KW_IF))
                else_branch = parse_if_stmt(); // рекурсия для else if
            else
                else_branch = parse_block();
        }

        auto node = std::make_unique<IfStmt>(
            std::move(cond), std::move(then), std::move(else_branch));
        node->loc = loc;
        return node;
    }

    // for (let x: T; cond; step) { }
    std::unique_ptr<ForStmt> Parser::parse_for_stmt() {
        auto loc = current().loc;
        expect(TokenKind::KW_FOR, "for");
        expect(TokenKind::LPAREN, "(");

        auto init = parse_let_stmt();      // let x: int8;  — уже съедает ;
        auto cond = parse_expr();
        expect(TokenKind::SEMICOLON, ";");
        auto step = parse_expr();          // x++
        expect(TokenKind::RPAREN, ")");
        auto body = parse_block();

        auto node = std::make_unique<ForStmt>(
            std::move(init), std::move(cond), std::move(step), std::move(body));
        node->loc = loc;
        return node;
    }

    // while (cond) { }
    std::unique_ptr<WhileStmt> Parser::parse_while_stmt() {
        auto loc = current().loc;
        expect(TokenKind::KW_WHILE, "while");
        expect(TokenKind::LPAREN, "(");
        auto cond = parse_expr();
        expect(TokenKind::RPAREN, ")");
        auto body = parse_block();

        auto node = std::make_unique<WhileStmt>(std::move(cond), std::move(body));
        node->loc = loc;
        return node;
    }

    // Приоритеты операторов: левая и правая сила притяжения
    // Чем больше число — тем сильнее оператор связывает операнды
    std::pair<int,int> Parser::infix_bp(TokenKind op) const {
        switch (op) {
            case TokenKind::PIPE_PIPE: return {1,  2};  // ||
            case TokenKind::AMP_AMP:   return {3,  4};  // &&
            case TokenKind::PIPE:      return {5,  6};  // |
            case TokenKind::CARET:     return {7,  8};  // ^
            case TokenKind::AMP:       return {9,  10}; // &
            case TokenKind::EQ_EQ:
            case TokenKind::BANG_EQ:   return {11, 12}; // == !=
            case TokenKind::LT:    
            case TokenKind::GT:
            case TokenKind::LT_EQ: 
            case TokenKind::GT_EQ:     return {13, 14};
            case TokenKind::LSHIFT:
            case TokenKind::RSHIFT:    return {15, 16}; // << >>
            case TokenKind::PLUS:
            case TokenKind::MINUS:     return {17, 18}; // + -
            case TokenKind::STAR:
            case TokenKind::SLASH:
            case TokenKind::PERCENT:   return {19, 20}; // * / %
            default:                   return {-1, -1}; // не инфикс
        }
    }

    // Парсинг выражения
    std::unique_ptr<ASTNode> Parser::parse_expr(int min_bp) {
        auto lhs = parse_prefix();

        while (true) {
            // составное присваивание: x += 1, x -= 1, ...
            // приоритет ниже всех бинарных операторов, правоассоциативно
            if (min_bp == 0 &&
                (check(TokenKind::PLUS_EQ)    || check(TokenKind::MINUS_EQ) ||
                check(TokenKind::STAR_EQ)    || check(TokenKind::SLASH_EQ) ||
                check(TokenKind::PERCENT_EQ)))
            {
                auto op  = std::string(consume().lexeme);
                auto val = parse_expr(0);
                lhs = std::make_unique<AssignExpr>(
                    std::move(lhs), op, std::move(val));
                break;
            }

            // простое присваивание: x = expr
            if (min_bp == 0 && check(TokenKind::EQ)) {
                consume();
                auto val = parse_expr(0);
                lhs = std::make_unique<AssignExpr>(
                    std::move(lhs), "=", std::move(val));
                break;
            }

            // бинарный оператор
            auto [lbp, rbp] = infix_bp(current().kind);
            if (lbp == -1 || lbp < min_bp) break;

            auto op  = std::string(consume().lexeme);
            auto rhs = parse_expr(rbp); // rbp > lbp — правый операнд связывается сильнее

            lhs = std::make_unique<BinaryExpr>(op, std::move(lhs), std::move(rhs));
        }
        return lhs;
    }

    // Префиксные унарные операторы и атомы
    std::unique_ptr<ASTNode> Parser::parse_prefix() {
        auto loc = current().loc;

        if (match(TokenKind::MINUS)) {
            auto operand = parse_prefix();
            auto node = std::make_unique<UnaryExpr>(UnaryExpr::Op::Neg, std::move(operand));
            node->loc = loc;
            return node;
        }
        if (match(TokenKind::BANG)) {
            auto node = std::make_unique<UnaryExpr>(UnaryExpr::Op::Not, parse_prefix());
            node->loc = loc;
            return node;
        }
        if (match(TokenKind::TILDE)) {
            auto node = std::make_unique<UnaryExpr>(UnaryExpr::Op::BitNot, parse_prefix());
            node->loc = loc;
            return node;
        }
        if (match(TokenKind::PLUS_PLUS)) {
            auto node = std::make_unique<UnaryExpr>(UnaryExpr::Op::PreInc, parse_prefix());
            node->loc = loc;
            return node;
        }
        if (match(TokenKind::MINUS_MINUS)) {
            auto node = std::make_unique<UnaryExpr>(UnaryExpr::Op::PreDec, parse_prefix());
            node->loc = loc;
            return node;
        }
        if (match(TokenKind::AMP)) {
            auto node = std::make_unique<UnaryExpr>(UnaryExpr::Op::AddrOf, parse_prefix());
            node->loc = loc;
            return node;
        }
        if (match(TokenKind::STAR)) {
            auto node = std::make_unique<UnaryExpr>(UnaryExpr::Op::Deref, parse_prefix());
            node->loc = loc;
            return node;
        }

        // нет префиксного оператора — атом + постфикс
        return parse_postfix(parse_atom());
    }
}
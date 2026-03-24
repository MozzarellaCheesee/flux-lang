#include "flux/parser/parser.h"
#include "flux/ast/type.h"


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

        // extern fnc — объявление без тела (реализация в C++ runtime)
        bool is_extern = false;
        if (check(TokenKind::KW_EXTERN)) {
            consume();
            is_extern = true;
        }

        if (check(TokenKind::KW_FNC))    return parse_func_decl(is_pub, is_extern);
        if (check(TokenKind::KW_STRUCT)) return parse_struct_decl(is_pub);
        if (check(TokenKind::KW_CLASS))  return parse_class_decl(is_pub);

        if (check(TokenKind::KW_IMPL)) {
            if (is_pub)
                diag_.emit(DiagLevel::Error, current().loc, "impl cannot be pub");
            return parse_impl_decl();
        }

        // #import "path" — пропускаем, препроцессор обработал заранее
        if (check(TokenKind::PP_IMPORT)) {
            consume();
            expect(TokenKind::LIT_STRING, "import path");
            return nullptr;
        }

        // #import <module> — пропускаем, препроцессор обработал заранее
        if (check(TokenKind::PP_IMPORT_SYS)) {
            consume();
            return nullptr;
        }

        diag_.emit(DiagLevel::Error, current().loc,
                "Expected top-level declaration (fnc, struct, class, impl)");
        consume(); // recovery — съедаем неожиданный токен
        return nullptr;
    }

    // fnc name(params) -> ReturnType { body }
    // extern fnc name(params) -> ReturnType;
    std::unique_ptr<FuncDecl> Parser::parse_func_decl(bool is_pub, bool is_extern) { // Парсинг объявления функции
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
            // ← self без типа — создаём ParamDecl
            auto self_param = std::make_unique<ParamDecl>("self", nullptr);
            self_param->loc = previous().loc;
            params.push_back(std::move(self_param));
            match(TokenKind::COMMA);
        }

        // остальные параметры: name: Type, name: Type, ...
        while (!check(TokenKind::RPAREN)) {
            auto ploc = current().loc;
            auto pname = std::string(expect(TokenKind::IDENT, "parameter name").lexeme);
            
            std::unique_ptr<TypeNode> ptype;
            if (match(TokenKind::COLON)) {
                ptype = parse_type();
            } else {
                diag_.emit(DiagLevel::Error, ploc, "Expected ':' after '" + pname + "'");
                ptype = nullptr;
            }
            
            auto param = std::make_unique<ParamDecl>(pname, std::move(ptype));
            param->loc = ploc;
            params.push_back(std::move(param));
            
            if (!match(TokenKind::COMMA)) break;
        }
        expect(TokenKind::RPAREN, ")");
        expect(TokenKind::ARROW, "->");
        auto ret = parse_type();

        std::unique_ptr<BlockStmt> body;
        if (is_extern) {
            match(TokenKind::SEMICOLON); // ; после extern fnc необязательная
        } else {
            body = parse_block();
        }

        auto node = std::make_unique<FuncDecl>(
            is_pub, is_extern, has_self, name,
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
        // match(TokenKind::COMMA); // завершающая запятая
        if (!match(TokenKind::COMMA))
            match(TokenKind::SEMICOLON);

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
            if (check(TokenKind::KW_FNC)) {
                node->methods.push_back(parse_func_decl(member_pub));
            } else {
                node->fields.push_back(parse_field_decl());
            }
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
            case TokenKind::KW_MATCH: {
                auto expr = parse_expr(); // parse_atom увидит KW_MATCH
                match(TokenKind::SEMICOLON); // ; после match необязательна
                return std::make_unique<ExprStmt>(std::move(expr));
            }
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

    // Постфиксные операторы: x++, x--, obj.field, obj.method(), arr[i]
    std::unique_ptr<ASTNode> Parser::parse_postfix(std::unique_ptr<ASTNode> lhs) {
        while (true) {
            auto loc = current().loc;

            if (match(TokenKind::PLUS_PLUS)) {
                auto node = std::make_unique<UnaryExpr>(
                    UnaryExpr::Op::PostInc, std::move(lhs));
                node->loc = loc;
                lhs = std::move(node);

            } else if (match(TokenKind::MINUS_MINUS)) {
                auto node = std::make_unique<UnaryExpr>(
                    UnaryExpr::Op::PostDec, std::move(lhs));
                node->loc = loc;
                lhs = std::move(node);

            } else if (match(TokenKind::DOT)) {
                // obj.member — поле или метод
                auto member = std::string(expect(TokenKind::IDENT, "member name").lexeme);
                if (check(TokenKind::LPAREN)) {
                    // obj.method(args)
                    consume(); // (
                    auto node = std::make_unique<MethodCallExpr>(std::move(lhs), member);
                    node->loc = loc;
                    while (!check(TokenKind::RPAREN) && !check(TokenKind::END_OF_FILE)) {
                        node->args.push_back(parse_expr());
                        if (!match(TokenKind::COMMA)) break;
                    }
                    expect(TokenKind::RPAREN, ")");
                    lhs = std::move(node);
                } else {
                    // obj.field
                    auto node = std::make_unique<FieldAccessExpr>(std::move(lhs), member);
                    node->loc = loc;
                    lhs = std::move(node);
                }

            } else if (match(TokenKind::LBRACKET)) {
                // arr[i]
                auto idx = parse_expr();
                expect(TokenKind::RBRACKET, "]");
                auto node = std::make_unique<IndexExpr>(std::move(lhs), std::move(idx));
                node->loc = loc;
                lhs = std::move(node);

            } else {
                break;
            }
        }
        return lhs;
    }

    // Неделимые единицы выражения: литералы, идентификаторы, вызовы, скобки
    std::unique_ptr<ASTNode> Parser::parse_atom() {
        auto loc = current().loc;

        // Целочисленный литерал
        if (check(TokenKind::LIT_INT)) {
            auto val = std::stoll(std::string(current().lexeme));
            consume();
            auto node = std::make_unique<IntLiteral>(val);
            node->loc = loc;
            return node;
        }

        // Вещественный литерал
        if (check(TokenKind::LIT_FLOAT)) {
            auto val = std::stod(std::string(current().lexeme));
            consume();
            auto node = std::make_unique<FloatLiteral>(val);
            node->loc = loc;
            return node;
        }

        // Строковый литерал
        if (check(TokenKind::LIT_STRING)) {
            auto val = std::string(current().lexeme);
            consume();
            auto node = std::make_unique<StrLiteral>(val);
            node->loc = loc;
            return node;
        }

        // Булевые литералы
        if (check(TokenKind::KW_TRUE)) {
            consume();
            auto node = std::make_unique<BoolLiteral>(true);
            node->loc = loc;
            return node;
        }
        if (check(TokenKind::KW_FALSE)) {
            consume();
            auto node = std::make_unique<BoolLiteral>(false);
            node->loc = loc;
            return node;
        }

        // self
        if (check(TokenKind::KW_SELF)) {
            consume();
            auto node = std::make_unique<SelfExpr>();
            node->loc = loc;
            return node;
        }

        // Массив: [1, 2, 3]
        if (match(TokenKind::LBRACKET)) {
            auto node = std::make_unique<ArrayLiteral>();
            node->loc = loc;
            while (!check(TokenKind::RBRACKET) && !check(TokenKind::END_OF_FILE)) {
                node->elements.push_back(parse_expr());
                if (!match(TokenKind::COMMA)) break;
            }
            expect(TokenKind::RBRACKET, "]");
            return node;
        }

        // Скобки: (expr)
        if (match(TokenKind::LPAREN)) {
            auto expr = parse_expr();
            expect(TokenKind::RPAREN, ")");
            return expr;
        }

        // match subject { pattern => { } ... }
        if (match(TokenKind::KW_MATCH)) {
            // parse_expr вызвал бы parse_struct_init для "func {"
            // используем parse_prefix напрямую — он не смотрит на {
            allow_struct_init_ = false; //  func { не будет StructInitExpr
            auto subject = parse_prefix();  //  заменяем parse_expr() на parse_prefix()
            allow_struct_init_ = true;  //  восстанавливаем для остального кода
            expect(TokenKind::LBRACE, "{");
            auto node = std::make_unique<MatchExpr>(std::move(subject));
            node->loc = loc;
            while (!check(TokenKind::RBRACE) && !check(TokenKind::END_OF_FILE)) {
                auto pattern = parse_pattern();
                expect(TokenKind::FAT_ARROW, "=>");
                // однострочный arm: => expr,
                std::unique_ptr<BlockStmt> body;
                if (check(TokenKind::LBRACE)) {
                    body = parse_block();
                } else {
                    auto expr = parse_expr();
                    body = std::make_unique<BlockStmt>();
                    body->stmts.push_back(std::make_unique<ExprStmt>(std::move(expr)));
                }
                match(TokenKind::COMMA);
                auto arm = std::make_unique<MatchArm>(std::move(pattern), std::move(body));
                arm->loc = loc;
                node->arms.push_back(std::move(arm));
            }
            expect(TokenKind::RBRACE, "}");
            return node;
        }

        // Идентификатор: переменная, вызов функции, инициализация структуры
        if (check(TokenKind::IDENT)) {
            auto name = std::string(consume().lexeme);
            if (check(TokenKind::LPAREN)) return parse_call(name);
            if (check(TokenKind::LBRACE) && allow_struct_init_) 
                return parse_struct_init(name);
            auto node = std::make_unique<IdentExpr>(name);
            node->loc = loc;
            return node;
        }

        // Ошибка — ничего не подошло
        diag_.emit(DiagLevel::Error, loc,
                "Expected expression, got '" + std::string(current().lexeme) + "'");
        consume(); // recovery
        auto dummy = std::make_unique<IntLiteral>(0);
        dummy->loc = loc;
        return dummy;
    }

    // foo(a, b, c)
    std::unique_ptr<ASTNode> Parser::parse_call(std::string callee) {
        auto loc = current().loc;
        consume(); // (
        auto node = std::make_unique<CallExpr>(std::move(callee));
        node->loc = loc;
        while (!check(TokenKind::RPAREN) && !check(TokenKind::END_OF_FILE)) {
            node->args.push_back(parse_expr());
            if (!match(TokenKind::COMMA)) break;
        }
        expect(TokenKind::RPAREN, ")");
        return node;
    }

    // Point { x: 1.0, y: 2.0 }
    std::unique_ptr<ASTNode> Parser::parse_struct_init(std::string type_name) {
        auto loc = current().loc;
        consume(); // {
        auto node = std::make_unique<StructInitExpr>(std::move(type_name));
        node->loc = loc;
        while (!check(TokenKind::RBRACE) && !check(TokenKind::END_OF_FILE)) {
            auto fname = std::string(expect(TokenKind::IDENT, "field name").lexeme);
            expect(TokenKind::COLON, ":");
            auto fval  = parse_expr();
            node->fields.push_back(FieldInit(fname, std::move(fval)));
            match(TokenKind::COMMA);
        }
        expect(TokenKind::RBRACE, "}");
        return node;
    }

    // Паттерны для match
    std::unique_ptr<PatternNode> Parser::parse_pattern() {
        auto loc = current().loc;

        // _ — wildcard, покрывает всё
        if (check(TokenKind::UNDERSCORE)) {
            consume();
            auto node = std::make_unique<WildcardPattern>();
            node->loc = loc;
            return node;
        }

        // Литеральные паттерны: 0, "str", true, false
        if (check(TokenKind::LIT_INT)    || check(TokenKind::LIT_FLOAT) ||
            check(TokenKind::LIT_STRING) ||
            check(TokenKind::KW_TRUE)    || check(TokenKind::KW_FALSE))
        {
            auto val  = parse_atom();
            auto node = std::make_unique<LiteralPattern>(std::move(val));
            node->loc = loc;
            return node;
        }

        // Идентификатор или конструктор: x, Ok(x), Err(e)
        if (check(TokenKind::IDENT)) {
            auto name = std::string(consume().lexeme);
            if (check(TokenKind::LPAREN)) {
                // конструктор с аргументами: Ok(x), Err(e)
                consume(); // (
                auto node = std::make_unique<ConstructorPattern>(name);
                node->loc = loc;
                while (!check(TokenKind::RPAREN) && !check(TokenKind::END_OF_FILE)) {
                    node->args.push_back(parse_pattern());
                    if (!match(TokenKind::COMMA)) break;
                }
                expect(TokenKind::RPAREN, ")");
                return node;
            }
            // просто переменная-паттерн: x
            auto node = std::make_unique<IdentPattern>(name);
            node->loc = loc;
            return node;
        }

        diag_.emit(DiagLevel::Error, loc,
                "Expected pattern, got '" + std::string(current().lexeme) + "'");
        return std::make_unique<WildcardPattern>(); // recovery
    }

    // Проверяет — начинается ли текущий токен тип
    bool Parser::is_type_start() const {
        switch (current().kind) {
            case TokenKind::KW_BOOL:
            case TokenKind::KW_INT8:   
            case TokenKind::KW_INT16:
            case TokenKind::KW_INT32:  
            case TokenKind::KW_INT64:  
            case TokenKind::KW_INT128:
            case TokenKind::KW_UINT8:  
            case TokenKind::KW_UINT16:
            case TokenKind::KW_UINT32: 
            case TokenKind::KW_UINT64: 
            case TokenKind::KW_UINT128:
            case TokenKind::KW_FLOAT:  
            case TokenKind::KW_DOUBLE:
            case TokenKind::KW_ISIZE_T: 
            case TokenKind::KW_USIZE_T:
            case TokenKind::KW_STR:    
            case TokenKind::KW_STRING:
            case TokenKind::IDENT: return true;
            default: return false;
        }
    }

    std::unique_ptr<TypeNode> Parser::parse_type() {
        auto loc = current().loc;

        // &T — ссылочный тип
        if (match(TokenKind::AMP)) {
            auto inner = parse_type();
            auto node  = std::make_unique<RefType>(std::move(inner));
            node->loc  = loc;
            return node;
        }

        // () — unit тип
        if (check(TokenKind::LPAREN)) {
            consume();
            expect(TokenKind::RPAREN, ")");
            auto node = std::make_unique<UnitType>();
            node->loc = loc;
            return node;
        }

        // Примитив, пользовательский тип, Generic или Array
        if (is_type_start()) {
            auto name = std::string(consume().lexeme);

            // Result<int32, string>, Option<T>
            if (match(TokenKind::LT)) {
                auto node = std::make_unique<GenericType>(name);
                node->loc = loc;
                while (!check(TokenKind::GT) && !check(TokenKind::END_OF_FILE)) {
                    node->args.push_back(parse_type());
                    if (!match(TokenKind::COMMA)) break;
                }
                expect(TokenKind::GT, ">");
                return node;
            }

            // Массив
            if (match(TokenKind::LBRACKET)) {
                auto elem = std::make_unique<PrimitiveType>(name);
                elem->loc = loc;
                
                // string[] — сразу закрывающая скобка, размер не указан
                if (match(TokenKind::RBRACKET)) { 
                    auto node = std::make_unique<SliceType>(std::move(elem));
                    node->loc = loc;
                    return node;
                }

                // int32[N] — есть выражение размера
                auto size = parse_expr();
                expect(TokenKind::RBRACKET, "]");
                elem->loc = loc;
                auto node = std::make_unique<ArrayType>(
                    std::move(elem), std::move(size));
                node->loc = loc;
                return node;
            }

            // просто примитив или имя типа
            auto node = std::make_unique<PrimitiveType>(name);
            node->loc = loc;
            return node;
        }

        diag_.emit(DiagLevel::Error, loc,
                "Expected type, got '" + std::string(current().lexeme) + "'");
        auto dummy = std::make_unique<PrimitiveType>("unknown");
        dummy->loc = loc;
        return dummy;
    }
}
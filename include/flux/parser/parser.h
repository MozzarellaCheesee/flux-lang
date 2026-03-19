#pragma once
#include "flux/lexer/token.h"
#include "flux/ast/expr.h"
#include "flux/ast/decl.h"
#include "flux/ast/stmt.h"
#include "flux/common/diagnostic.h"
#include <vector>
#include <memory>

namespace flux {
    class Parser {
    public:
        Parser(std::vector<Token> tokens, DiagEngine& diag);
        std::unique_ptr<ProgramNode> parse_program();

    private:
        // ── Top-level ────────────────────────────────────
        std::unique_ptr<ASTNode>    parse_top_level();
        std::unique_ptr<FuncDecl>   parse_func_decl(bool is_pub);
        std::unique_ptr<StructDecl> parse_struct_decl(bool is_pub);
        std::unique_ptr<ImplDecl>   parse_impl_decl();
        std::unique_ptr<ClassDecl>  parse_class_decl(bool is_pub);
        std::unique_ptr<FieldDecl>  parse_field_decl();

        // ── Statements ───────────────────────────────────
        std::unique_ptr<ASTNode>    parse_stmt();
        std::unique_ptr<BlockStmt>  parse_block();
        std::unique_ptr<VarDecl>    parse_let_stmt();
        std::unique_ptr<ReturnStmt> parse_return_stmt();
        std::unique_ptr<IfStmt>     parse_if_stmt();
        std::unique_ptr<ForStmt>    parse_for_stmt();
        std::unique_ptr<WhileStmt>  parse_while_stmt();

        // ── Expressions ──────────────────────────────────
        std::unique_ptr<ASTNode>     parse_expr(int min_bp = 0);
        std::unique_ptr<ASTNode>     parse_prefix();
        std::unique_ptr<ASTNode>     parse_postfix(std::unique_ptr<ASTNode> lhs);
        std::unique_ptr<ASTNode>     parse_atom();
        std::unique_ptr<ASTNode>     parse_call(std::string callee);
        std::unique_ptr<ASTNode>     parse_struct_init(std::string type_name);
        std::unique_ptr<PatternNode> parse_pattern();

        // ── Types ────────────────────────────────────────
        std::unique_ptr<TypeNode> parse_type();

        // ── Helpers ──────────────────────────────────────
        Token            consume();
        Token            expect(TokenKind kind, const char* msg = "");
        bool             check(TokenKind kind) const;
        bool             match(TokenKind kind);
        const Token&     current()  const;
        const Token&     previous() const;
        bool             is_type_start() const;
        std::pair<int,int> infix_bp(TokenKind op) const;

        std::vector<Token> tokens_;
        size_t             pos_ = 0;
        DiagEngine&        diag_;
    };
}
#pragma once
#include <string>
#include <sstream>
#include "flux/ast/ast_node.h"
#include "flux/ast/decl.h"
#include "flux/ast/expr.h"

namespace flux {

    class CodeGen {
    public:
        std::string generate(ProgramNode& program);

    private:
        std::string sanitize_name(const std::string& name);
        std::ostringstream out_;
        int indent_ = 0;

        void emit(const std::string& s)   { out_ << s; }
        void emitln(const std::string& s) { out_ << std::string(indent_ * 4, ' ') << s << "\n"; }
        void indent()   { indent_++; }
        void dedent()   { indent_--; }

        // Types
        std::string map_type(TypeNode* t);
        std::string map_type_str(const std::string& t);

        // Declarations
        void gen(ProgramNode& node);
        void gen(FuncDecl& node, const std::string& owner = "");
        void gen(StructDecl& node);
        void gen(ClassDecl& node);
        void gen(ImplDecl& node);

        // Statements
        void gen(ASTNode& node);
        void gen(BlockStmt& node);
        void gen(VarDecl& node);
        void gen(ReturnStmt& node);
        void gen(IfStmt& node);
        void gen(ForStmt& node);
        void gen(WhileStmt& node);
        void gen(ExprStmt& node);
        void gen(BreakStmt& node);
        void gen(ContinueStmt& node);

        // Expressions
        std::string expr(ASTNode& node);
        std::string expr(IntLiteral& node);
        std::string expr(FloatLiteral& node);
        std::string expr(StrLiteral& node);
        std::string expr(BoolLiteral& node);
        std::string expr(IdentExpr& node);
        std::string expr(SelfExpr& node);
        std::string expr(BinaryExpr& node);
        std::string expr(UnaryExpr& node);
        std::string expr(AssignExpr& node);
        std::string expr(CallExpr& node);
        std::string expr(MethodCallExpr& node);
        std::string expr(FieldAccessExpr& node);
        std::string expr(IndexExpr& node);
        std::string expr(ArrayLiteral& node);
        std::string expr(StructInitExpr& node);
        std::string expr(MatchExpr& node);
    };

} // namespace flux

#pragma once
#include "flux/ast/decl.h"
#include "flux/ast/stmt.h"
#include "flux/ast/expr.h"
#include "flux/ast/type.h"

namespace flux {

    struct ASTVisitor {
        virtual ~ASTVisitor() = default;

        // ── Types ────────────────────────────────────────
        virtual void visit(PrimitiveType&)       = 0;
        virtual void visit(RefType&)             = 0;
        virtual void visit(UnitType&)            = 0;
        virtual void visit(ArrayType&)           = 0;
        virtual void visit(GenericType&)         = 0;

        // ── Declarations ─────────────────────────────────
        virtual void visit(FuncDecl&)            = 0;
        virtual void visit(VarDecl&)             = 0;
        virtual void visit(ParamDecl&)           = 0;
        virtual void visit(FieldDecl&)           = 0;
        virtual void visit(StructDecl&)          = 0;
        virtual void visit(ImplDecl&)            = 0;
        virtual void visit(ClassDecl&)           = 0;

        // ── Statements ───────────────────────────────────
        virtual void visit(BlockStmt&)           = 0;
        virtual void visit(ReturnStmt&)          = 0;
        virtual void visit(ExprStmt&)            = 0;
        virtual void visit(IfStmt&)              = 0;
        virtual void visit(ForStmt&)             = 0;
        virtual void visit(WhileStmt&)           = 0;
        virtual void visit(ContinueStmt&)        = 0;
        virtual void visit(BreakStmt&)           = 0;

        // ── Expressions ──────────────────────────────────
        virtual void visit(IntLiteral&)          = 0;
        virtual void visit(FloatLiteral&)        = 0;
        virtual void visit(BoolLiteral&)         = 0;
        virtual void visit(StrLiteral&)          = 0;
        virtual void visit(ArrayLiteral&)        = 0;
        virtual void visit(IdentExpr&)           = 0;
        virtual void visit(BinaryExpr&)          = 0;
        virtual void visit(UnaryExpr&)           = 0;
        virtual void visit(CallExpr&)            = 0;
        virtual void visit(MethodCallExpr&)      = 0;
        virtual void visit(IndexExpr&)           = 0;
        virtual void visit(AssignExpr&)          = 0;
        virtual void visit(ImplicitCastExpr&)    = 0;
        virtual void visit(MatchExpr&)           = 0;
        virtual void visit(MatchArm&)            = 0;
        virtual void visit(ProgramNode&)         = 0;
        virtual void visit(FieldAccessExpr&) = 0;
        virtual void visit(StructInitExpr&)  = 0;
        virtual void visit(SelfExpr&)        = 0;
    };

}
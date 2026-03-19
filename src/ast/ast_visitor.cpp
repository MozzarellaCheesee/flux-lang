#include "flux/ast/ast_visitor.h"

namespace flux {

    // ── Types ────────────────────────────────────────────────
    void PrimitiveType::accept(ASTVisitor& v) { v.visit(*this); }
    void RefType::accept(ASTVisitor& v)       { v.visit(*this); }
    void UnitType::accept(ASTVisitor& v)      { v.visit(*this); }
    void ArrayType::accept(ASTVisitor& v)     { v.visit(*this); }
    void GenericType::accept(ASTVisitor& v)   { v.visit(*this); }

    // ── Declarations ─────────────────────────────────────────
    void ProgramNode::accept(ASTVisitor& v)   { v.visit(*this); }
    void FuncDecl::accept(ASTVisitor& v)      { v.visit(*this); }
    void VarDecl::accept(ASTVisitor& v)       { v.visit(*this); }
    void ParamDecl::accept(ASTVisitor& v)     { v.visit(*this); }
    void FieldDecl::accept(ASTVisitor& v)     { v.visit(*this); }
    void StructDecl::accept(ASTVisitor& v)    { v.visit(*this); }
    void ImplDecl::accept(ASTVisitor& v)      { v.visit(*this); }
    void ClassDecl::accept(ASTVisitor& v)     { v.visit(*this); }

    // ── Statements ───────────────────────────────────────────
    void BlockStmt::accept(ASTVisitor& v)     { v.visit(*this); }
    void ReturnStmt::accept(ASTVisitor& v)    { v.visit(*this); }
    void ExprStmt::accept(ASTVisitor& v)      { v.visit(*this); }
    void IfStmt::accept(ASTVisitor& v)        { v.visit(*this); }
    void ForStmt::accept(ASTVisitor& v)       { v.visit(*this); }
    void WhileStmt::accept(ASTVisitor& v)     { v.visit(*this); }
    void ContinueStmt::accept(ASTVisitor& v)  { v.visit(*this); }
    void BreakStmt::accept(ASTVisitor& v)     { v.visit(*this); }

    // ── Expressions ──────────────────────────────────────────
    void IntLiteral::accept(ASTVisitor& v)        { v.visit(*this); }
    void FloatLiteral::accept(ASTVisitor& v)      { v.visit(*this); }
    void BoolLiteral::accept(ASTVisitor& v)       { v.visit(*this); }
    void StrLiteral::accept(ASTVisitor& v)        { v.visit(*this); }
    void ArrayLiteral::accept(ASTVisitor& v)      { v.visit(*this); }
    void IdentExpr::accept(ASTVisitor& v)         { v.visit(*this); }
    void BinaryExpr::accept(ASTVisitor& v)        { v.visit(*this); }
    void UnaryExpr::accept(ASTVisitor& v)         { v.visit(*this); }
    void AssignExpr::accept(ASTVisitor& v)        { v.visit(*this); }
    void CallExpr::accept(ASTVisitor& v)          { v.visit(*this); }
    void MethodCallExpr::accept(ASTVisitor& v)    { v.visit(*this); }
    void IndexExpr::accept(ASTVisitor& v)         { v.visit(*this); }
    void ImplicitCastExpr::accept(ASTVisitor& v)  { v.visit(*this); }
    void MatchArm::accept(ASTVisitor& v)          { v.visit(*this); }
    void MatchExpr::accept(ASTVisitor& v)         { v.visit(*this); }
    void StructInitExpr::accept(ASTVisitor& v)    { v.visit(*this); }
    void FieldAccessExpr::accept(ASTVisitor& v)   { v.visit(*this); }
    void SelfExpr::accept(ASTVisitor& v)          { v.visit(*this); }

} // namespace flux
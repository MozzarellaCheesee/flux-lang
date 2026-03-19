#pragma once
#include "flux/ast/ast_node.h"
#include "flux/ast/ast_node.h"
#include "flux/ast/type.h"
#include "flux/ast/stmt.h"
#include <vector>
#include <memory>
#include <string>
#include <cstdint>

namespace flux {

    // ── Литералы ────────────────────────────────────────────

    struct IntLiteral : ASTNode { // Целочисленный литерал
        int64_t value;
        explicit IntLiteral(int64_t v) : value(v) {}
        void accept(ASTVisitor& v) override;
    };

    struct FloatLiteral : ASTNode { // Вещественный литерал
        double value;
        explicit FloatLiteral(double v) : value(v) {}
        void accept(ASTVisitor& v) override;
    };

    struct BoolLiteral : ASTNode { // Булевый литерал
        bool value;
        explicit BoolLiteral(bool v) : value(v) {}
        void accept(ASTVisitor& v) override;
    };

    struct StrLiteral : ASTNode { // Строковый литерал
        std::string value;
        explicit StrLiteral(std::string v) : value(std::move(v)) {}
        void accept(ASTVisitor& v) override;
    };

    struct ArrayLiteral : ASTNode { // Литерал списка
        std::vector<std::unique_ptr<ASTNode>> elements;
        ArrayLiteral() = default;
        explicit ArrayLiteral(std::vector<std::unique_ptr<ASTNode>> elems)
            : elements(std::move(elems)) {}
        void accept(ASTVisitor& v) override;
    };

    // ── Идентификатор ────────────────────────────────────────

    struct IdentExpr : ASTNode { // Идентификаторы (названия переменных, функций и тд)
        std::string name;
        explicit IdentExpr(std::string n) : name(std::move(n)) {}
        void accept(ASTVisitor& v) override;
    };

    // ── Операторы ────────────────────────────────────────────

    struct BinaryExpr : ASTNode { // Бинарные операции
        std::string              op;   // "+", "-", "==", "&&", ...
        std::unique_ptr<ASTNode> lhs;
        std::unique_ptr<ASTNode> rhs;

        BinaryExpr(std::string o,
                std::unique_ptr<ASTNode> l,
                std::unique_ptr<ASTNode> r)
            : op(std::move(o)), lhs(std::move(l)), rhs(std::move(r)) {}
        void accept(ASTVisitor& v) override;
    };

    struct UnaryExpr : ASTNode { // Унарные операции
        enum class Op {
            PreInc, PreDec,    // ++x, --x
            PostInc, PostDec,  // x++, x--
            Neg,               // -x
            Not,               // !x
            BitNot,            // ~x
            Deref,             // *x
            AddrOf             // &x
        };
        Op                       op;
        std::unique_ptr<ASTNode> operand;

        UnaryExpr(Op o, std::unique_ptr<ASTNode> operand)
            : op(o), operand(std::move(operand)) {}
        void accept(ASTVisitor& v) override;
    };

    struct AssignExpr : ASTNode { // Присваивание
        std::unique_ptr<ASTNode> target;  // IdentExpr или UnaryExpr{Deref}
        std::string              op;      // "=", "+=", "-=", ...
        std::unique_ptr<ASTNode> value;

        AssignExpr(std::unique_ptr<ASTNode> t,
                std::string o,
                std::unique_ptr<ASTNode> val)
            : target(std::move(t)), op(std::move(o)), value(std::move(val)) {}
        void accept(ASTVisitor& v) override;
    };

    // ── Вызовы ───────────────────────────────────────────────

    struct CallExpr : ASTNode { // Любой вызов вида имя(аргументы)
        std::string                           callee;
        std::vector<std::unique_ptr<ASTNode>> args;

        explicit CallExpr(std::string c) : callee(std::move(c)) {}
        void accept(ASTVisitor& v) override;
    };

    struct MethodCallExpr : ASTNode { // Вызов через точку, есть объект-получатель
        std::unique_ptr<ASTNode>              receiver;
        std::string                           method;
        std::vector<std::unique_ptr<ASTNode>> args;

        MethodCallExpr(std::unique_ptr<ASTNode> r, std::string m)
            : receiver(std::move(r)), method(std::move(m)) {}
        void accept(ASTVisitor& v) override;
    };

    // ── Индексация ───────────────────────────────────────────

    struct IndexExpr : ASTNode {
        std::unique_ptr<ASTNode> array;
        std::unique_ptr<ASTNode> index;

        IndexExpr(std::unique_ptr<ASTNode> a, std::unique_ptr<ASTNode> i)
            : array(std::move(a)), index(std::move(i)) {}
        void accept(ASTVisitor& v) override;
    };

    // ── Каст (вставляет семантик) ────────────────────────────

    struct ImplicitCastExpr : ASTNode {
        std::unique_ptr<ASTNode>  inner;
        std::unique_ptr<TypeNode> target_type;

        ImplicitCastExpr(std::unique_ptr<ASTNode> i, std::unique_ptr<TypeNode> t)
            : inner(std::move(i)), target_type(std::move(t)) {}
        void accept(ASTVisitor& v) override;
    };

    // ── Match ─────────────────────────────────────────────────

    struct WildcardPattern : PatternNode {};

    struct LiteralPattern : PatternNode {
        std::unique_ptr<ASTNode> value;
        explicit LiteralPattern(std::unique_ptr<ASTNode> v) : value(std::move(v)) {}
    };

    struct IdentPattern : PatternNode {
        std::string name;
        explicit IdentPattern(std::string n) : name(std::move(n)) {}
    };

    struct ConstructorPattern : PatternNode {
        std::string                               name;  // Ok, Err, Some...
        std::vector<std::unique_ptr<PatternNode>> args;
        explicit ConstructorPattern(std::string n) : name(std::move(n)) {}
    };

    struct MatchArm : ASTNode {
        std::unique_ptr<PatternNode> pattern;
        std::unique_ptr<BlockStmt>   body;

        MatchArm(std::unique_ptr<PatternNode> p, std::unique_ptr<BlockStmt> b)
            : pattern(std::move(p)), body(std::move(b)) {}
        void accept(ASTVisitor& v) override;
    };

    struct MatchExpr : ASTNode {
        std::unique_ptr<ASTNode>               subject;
        std::vector<std::unique_ptr<MatchArm>> arms;

        explicit MatchExpr(std::unique_ptr<ASTNode> s) : subject(std::move(s)) {}
        void accept(ASTVisitor& v) override;
    };

    // ── Корневой узел программы ──────────────────────────────

        struct ProgramNode : ASTNode {
            std::vector<std::unique_ptr<ASTNode>> decls;
            void accept(ASTVisitor& v) override;
        };
    }


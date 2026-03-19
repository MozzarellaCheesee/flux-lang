#pragma once
#include "flux/ast/ast_node.h"
#include <vector>
#include <memory>

namespace flux {

    struct BlockStmt : ASTNode {
        std::vector<std::unique_ptr<ASTNode>> stmts;

        void accept(ASTVisitor& v) override;
    };

    struct ReturnStmt : ASTNode {
        std::unique_ptr<ASTNode> value; // nullptr = return;

        explicit ReturnStmt(std::unique_ptr<ASTNode> val)
            : value(std::move(val)) {}
        void accept(ASTVisitor& v) override;
    };

    struct ExprStmt : ASTNode {
        std::unique_ptr<ASTNode> expr;

        explicit ExprStmt(std::unique_ptr<ASTNode> e)
            : expr(std::move(e)) {}
        void accept(ASTVisitor& v) override;
    };

    struct IfStmt : ASTNode {
        std::unique_ptr<ASTNode>   cond;
        std::unique_ptr<BlockStmt> then_block;
        std::unique_ptr<ASTNode>   else_branch; // IfStmt | BlockStmt | nullptr

        IfStmt(std::unique_ptr<ASTNode> c,
            std::unique_ptr<BlockStmt> t,
            std::unique_ptr<ASTNode> e)
            : cond(std::move(c)), then_block(std::move(t)), else_branch(std::move(e)) {}
        void accept(ASTVisitor& v) override;
    };

    struct ForStmt : ASTNode {
        std::unique_ptr<ASTNode>   init; // VarDecl или ExprStmt
        std::unique_ptr<ASTNode>   cond;
        std::unique_ptr<ASTNode>   step;
        std::unique_ptr<BlockStmt> body;

        ForStmt(std::unique_ptr<ASTNode> i,
                std::unique_ptr<ASTNode> c,
                std::unique_ptr<ASTNode> s,
                std::unique_ptr<BlockStmt> b)
            : init(std::move(i)), cond(std::move(c)),
            step(std::move(s)), body(std::move(b)) {}
        void accept(ASTVisitor& v) override;
    };

    struct WhileStmt : ASTNode {
        std::unique_ptr<ASTNode>   cond;
        std::unique_ptr<BlockStmt> body;

        WhileStmt(std::unique_ptr<ASTNode> c, std::unique_ptr<BlockStmt> b)
            : cond(std::move(c)), body(std::move(b)) {}
        void accept(ASTVisitor& v) override;
    };

    struct ContinueStmt : ASTNode {
        void accept(ASTVisitor& v) override;
    };

    struct BreakStmt : ASTNode {
        void accept(ASTVisitor& v) override;
    };

} // namespace flux

#pragma once
#include "flux/ast/ast_node.h"
#include <string>
#include <vector>
#include <memory>

namespace flux {

    struct BlockStmt; // forward declaration — тело функции

    struct ParamDecl : ASTNode {
        std::string               name;
        std::unique_ptr<TypeNode> type;

        ParamDecl(std::string n, std::unique_ptr<TypeNode> t)
            : name(std::move(n)), type(std::move(t)) {}
        void accept(ASTVisitor& v) override;
    };

    struct FuncDecl : ASTNode {
        bool                                    is_pub;
        bool                                    has_self;  // первый параметр self
        std::string                             name;
        std::vector<std::unique_ptr<ParamDecl>> params;    // без параметра self
        std::unique_ptr<TypeNode>               return_type;
        std::unique_ptr<BlockStmt>              body;

        FuncDecl(bool pub, bool self, std::string n,
                std::vector<std::unique_ptr<ParamDecl>> p,
                std::unique_ptr<TypeNode> rt,
                std::unique_ptr<BlockStmt> b)
            : is_pub(pub), has_self(self), name(std::move(n)), 
            params(std::move(p)), return_type(std::move(rt)), body(std::move(b)) {}
        void accept(ASTVisitor& v) override;
    };

    struct VarDecl : ASTNode {
        std::string               name;
        std::unique_ptr<TypeNode> type;  // nullptr = вывод типа
        std::unique_ptr<ASTNode>  init;  // nullptr = нет инициализатора

        VarDecl(std::string n,
                std::unique_ptr<TypeNode> t,
                std::unique_ptr<ASTNode> i)
            : name(std::move(n)), type(std::move(t)), init(std::move(i)) {}
        void accept(ASTVisitor& v) override;
    };

    // ── Поле структуры/класса ────────────────────────────

    struct FieldDecl : ASTNode {
        bool                      is_pub;
        std::string               name;
        std::unique_ptr<TypeNode> type;

        FieldDecl(bool pub, std::string n, std::unique_ptr<TypeNode> t)
            : is_pub(pub), name(std::move(n)), type(std::move(t)) {}
        void accept(ASTVisitor& v) override;
    };

    // ── Struct ───────────────────────────────────────────

    struct StructDecl : ASTNode {
        bool                                  is_pub;
        std::string                           name;
        std::vector<std::unique_ptr<FieldDecl>> fields;

        StructDecl(bool pub, std::string n)
            : is_pub(pub), name(std::move(n)) {}
        void accept(ASTVisitor& v) override;
    };

    // ── Impl (реализация методов для struct) ─────────────

    struct ImplDecl : ASTNode {
        std::string                            target;   // имя структуры
        std::vector<std::unique_ptr<FuncDecl>> methods;

        explicit ImplDecl(std::string t) : target(std::move(t)) {}
        void accept(ASTVisitor& v) override;
    };

    // ── Class (struct + impl вместе) ─────────────────────

    struct ClassDecl : ASTNode {
        bool                                   is_pub;
        std::string                            name;
        std::vector<std::unique_ptr<FieldDecl>> fields;
        std::vector<std::unique_ptr<FuncDecl>> methods;

        ClassDecl(bool pub, std::string n)
            : is_pub(pub), name(std::move(n)) {}
        void accept(ASTVisitor& v) override;
};

}

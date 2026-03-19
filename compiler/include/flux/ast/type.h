#pragma once
#include "flux/ast/ast_node.h"
#include <string>
#include <memory>
#include <vector>

namespace flux {

    struct ASTVisitor;

    struct PrimitiveType : TypeNode {
        std::string name;
        explicit PrimitiveType(std::string n) : name(std::move(n)) {}
    };

    struct RefType : TypeNode {
        std::unique_ptr<TypeNode> inner;
        explicit RefType(std::unique_ptr<TypeNode> i) : inner(std::move(i)) {}
    };

    struct UnitType : TypeNode {};

    struct ArrayType : TypeNode {
        std::unique_ptr<TypeNode> element;
        std::unique_ptr<ASTNode>  size_expr;
        ArrayType(std::unique_ptr<TypeNode> e, std::unique_ptr<ASTNode> s)
            : element(std::move(e)), size_expr(std::move(s)) {}
    };

    struct GenericType : TypeNode {
        std::string name;
        std::vector<std::unique_ptr<TypeNode>> args;
        explicit GenericType(std::string n) : name(std::move(n)) {}
    };
}

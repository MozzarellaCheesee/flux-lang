#pragma once
#include "flux/common/source_location.h"

namespace flux {

    struct ASTVisitor;

    struct TypeNode {
        SourceLocation loc;
        virtual ~TypeNode() = default;
    };

    struct ASTNode {
        SourceLocation loc;
        virtual void accept(ASTVisitor& v);
        virtual ~ASTNode() = default;
    };

    struct PatternNode { 
        SourceLocation loc; 
        virtual ~PatternNode() = default; 
    };
}

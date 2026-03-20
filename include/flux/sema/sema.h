#pragma once
#include "flux/ast/ast_visitor.h"
#include "flux/common/diagnostic.h"
#include <unordered_map>
#include <vector>
#include <string>

namespace flux {
    // Сигнатура функции для таблицы перегрузок
    struct FuncSignature {
        std::string              name;
        std::vector<std::string> param_types; // строковые имена типов: "int32", "str", ...
        std::string              return_type; // возвращаемый тип: "int32", "Result<int32, string>", ...
        FuncDecl*                decl;        // указатель на AST-ноду
    };

    // Описание поля
    struct FieldInfo {
        std::string name;
        std::string type;
        bool        is_pub;
    };

    // Описание структуры или класса
    struct TypeInfo {
        std::string              name;
        std::vector<FieldInfo>   fields;
        bool                     is_class; // true = class, false = struct
    };

    // Таблица всех объявленных функций (с учётом перегрузок)
    // ключ — имя функции, значение — список всех перегрузок
    using OverloadSet = std::vector<FuncSignature>;
    using FuncTable   = std::unordered_map<std::string, OverloadSet>;

    class SemanticAnalyzer : public ASTVisitor {
    public:
        explicit SemanticAnalyzer(DiagEngine& diag);

        // Точка входа — запускает анализ всего дерева
        void analyze(ProgramNode& program);

        // ── Types ────────────────────────────────────────
        void visit(PrimitiveType&)    override {}
        void visit(RefType&)          override {}
        void visit(UnitType&)         override {}
        void visit(ArrayType&)        override {}
        void visit(GenericType&)      override {}
        void visit(SliceType&)        override {}

        // ── Declarations ─────────────────────────────────
        void visit(ProgramNode&)      override;
        void visit(FuncDecl&)         override;
        void visit(VarDecl&)          override;
        void visit(ParamDecl&)        override {}
        void visit(FieldDecl&)        override {}
        void visit(StructDecl&)       override;
        void visit(ImplDecl&)         override;
        void visit(ClassDecl&)        override;

        // ── Statements ───────────────────────────────────
        void visit(BlockStmt&)        override;
        void visit(ReturnStmt&)       override;
        void visit(ExprStmt&)         override;
        void visit(IfStmt&)           override;
        void visit(ForStmt&)          override;
        void visit(WhileStmt&)        override;
        void visit(ContinueStmt&)     override {}
        void visit(BreakStmt&)        override {}

        // ── Expressions ──────────────────────────────────
        void visit(IntLiteral&)       override { last_type_ = "int32"; }
        void visit(FloatLiteral&)     override { last_type_ = "double"; }
        void visit(BoolLiteral&)      override { last_type_ = "bool"; }
        void visit(StrLiteral&)       override { last_type_ = "&str"; }
        void visit(IdentExpr&)        override;
        void visit(BinaryExpr&)       override;
        void visit(UnaryExpr&)        override;
        void visit(AssignExpr&)       override;
        void visit(CallExpr&)         override;
        void visit(MethodCallExpr&)   override;
        void visit(IndexExpr&)        override;
        void visit(ArrayLiteral&)     override;
        void visit(ImplicitCastExpr&) override;
        void visit(MatchExpr&)        override;
        void visit(MatchArm&)         override;
        void visit(StructInitExpr&)   override;
        void visit(FieldAccessExpr&)  override;
        void visit(SelfExpr&)         override {}

    private:
        // ── Проверки точки входа ─────────────────────────
        void check_main_entry_point();

        // ── Перегрузка ───────────────────────────────────
        void register_func(FuncDecl& decl);
        const FuncSignature* resolve_overload(const std::string& name, const std::vector<std::string>& arg_types, const SourceLocation& loc);

        // ── Вспомогательные ──────────────────────────────
        void push_scope();
        void pop_scope();
        void declare_var(const std::string& name, const std::string& type, const SourceLocation& loc);
        bool is_known_type(const std::string& name) const;
        bool is_builtin(const std::string& name) const;
        std::string type_node_to_string(TypeNode* t);
        std::string lookup_var(const std::string& name) const;
        static int         numeric_rank(const std::string& type);
        static bool        is_numeric(const std::string& type);
        static std::string wider_type(const std::string& a, const std::string& b);
        static bool        is_implicitly_convertible(const std::string& from, const std::string& to);
        // bool               is_result_compatible(const std::string& from, const std::string& to) const;
        static bool        is_assign_compatible(const std::string& from, const std::string& to);



        std::string last_type_; // тип последнего вычисленного выражения
        std::string eval_type(ASTNode& node); // вызывает accept и возвращает last_type_

        DiagEngine& diag_;

        // Таблица функций (все перегрузки)
        FuncTable func_table_;

        // Стек областей видимости: каждый scope — map имя->тип
        std::vector<std::unordered_map<std::string, std::string>> scopes_;
        std::unordered_map<std::string, TypeInfo> type_table_;

        // Текущая функция (для проверки return)
        FuncDecl* current_func_ = nullptr;
    };
}
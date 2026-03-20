#pragma once
#include "flux/ast/ast_visitor.h"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <map>
#include <stack>
#include <string>
#include <memory>

namespace flux {

    class LLVMCodegen : public ASTVisitor {
    public:
        explicit LLVMCodegen(const std::string& module_name);

        // Генерирует объектный файл (.o). Возвращает false при ошибке.
        bool emit_object_file(const std::string& output_path);
        bool emit_assembly(const std::string& output_path);
        bool emit_llvm_ir(const std::string& output_path);
        void dump_ir();
        void set_target_triple(const std::string& triple) { target_triple_ = triple; }

    private:
        // ── Visitor overrides ──────────────────────────────────
        void visit(ProgramNode&)      override;
        void visit(FuncDecl&)         override;
        void visit(VarDecl&)          override;
        void visit(ParamDecl&)        override;
        void visit(FieldDecl&)        override;
        void visit(StructDecl&)       override;
        void visit(ImplDecl&)         override;
        void visit(ClassDecl&)        override;

        void visit(BlockStmt&)        override;
        void visit(ReturnStmt&)       override;
        void visit(ExprStmt&)         override;
        void visit(IfStmt&)           override;
        void visit(ForStmt&)          override;
        void visit(WhileStmt&)        override;
        void visit(ContinueStmt&)     override;
        void visit(BreakStmt&)        override;

        void visit(IntLiteral&)       override;
        void visit(FloatLiteral&)     override;
        void visit(BoolLiteral&)      override;
        void visit(StrLiteral&)       override;
        void visit(ArrayLiteral&)     override;
        void visit(IdentExpr&)        override;
        void visit(BinaryExpr&)       override;
        void visit(UnaryExpr&)        override;
        void visit(CallExpr&)         override;
        void visit(MethodCallExpr&)   override;
        void visit(IndexExpr&)        override;
        void visit(AssignExpr&)       override;
        void visit(ImplicitCastExpr&) override;
        void visit(MatchExpr&)        override;
        void visit(MatchArm&)         override;
        void visit(FieldAccessExpr&)  override;
        void visit(StructInitExpr&)   override;
        void visit(SelfExpr&)         override;

        void visit(PrimitiveType&)    override;
        void visit(RefType&)          override;
        void visit(UnitType&)         override;
        void visit(ArrayType&)        override;
        void visit(GenericType&)      override;
        void visit(SliceType&)        override;
        void declare_builtins();

        llvm::LLVMContext                  ctx_;
        llvm::IRBuilder<>                  builder_;
        std::unique_ptr<llvm::Module>      module_;

        // Последнее вычисленное значение выражения
        llvm::Value*                       last_val_ = nullptr;
        // Последний вычисленный тип
        llvm::Type*                        last_type_ = nullptr;

        // Таблица переменных: имя → alloca
        std::map<std::string, llvm::AllocaInst*> locals_;

        // Для break/continue — стек блоков цикла
        std::stack<llvm::BasicBlock*>  loop_exit_blocks_;
        std::stack<llvm::BasicBlock*>  loop_cont_blocks_;

        // Таблица struct-типов: имя → StructType + порядок полей
        struct StructInfo {
            llvm::StructType*              type;
            std::vector<std::string>       field_names;
        };
        std::map<std::string, StructInfo>  structs_;

        // self-pointer для методов
        llvm::Value*                       self_ptr_ = nullptr;
        std::string target_triple_;

        // ── Helpers ────────────────────────────────────
        llvm::Type*        llvm_type(TypeNode* t);
        llvm::Type*        llvm_primitive(const std::string& name);
        llvm::AllocaInst*  create_entry_alloca(llvm::Function* f,
                                                const std::string& name,
                                                llvm::Type* ty);
        llvm::Value*       load_if_ptr(llvm::Value* v);
        llvm::Value*       emit_expr(ASTNode& n);  // хелпер: вызывает accept и возвращает last_val_
    };


}

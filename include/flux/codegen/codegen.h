#pragma once
#include "flux/ast/ast.h"
#include "flux/ast/decl.h"
#include "flux/ast/expr.h"
#include "flux/common/diagnostic.h"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/FileSystem.h>
#include <string>

namespace flux {

    class CodeGen {
    public:
        explicit CodeGen(DiagEngine& diag);
        ~CodeGen();

        // Главная точка входа
        bool generate(ProgramNode& program, const std::string& output_path);

    private:
        // Контекст LLVM — один на всю программу
        llvm::LLVMContext context_;
        
        // Модуль LLVM — аналог объектного файла
        std::unique_ptr<llvm::Module> module_;
        
        // Builder — для генерации инструкций
        std::unique_ptr<llvm::IRBuilder<>> builder_;
        
        // Target machine — для генерации машинного кода
        std::unique_ptr<llvm::TargetMachine> target_machine_;

        DiagEngine& diag_;

        // ── Генерация по типам AST ────────────────────────────────
        void codegen(ProgramNode& node);
        void codegen(FuncDecl& node);
        void codegen(VarDecl& node);
        void codegen(BlockStmt& node);
        void codegen(ReturnStmt& node);
        void codegen(ExprStmt& node);
        
        llvm::Type* llvm_type(const std::string& flux_type);
        llvm::Type* llvm_int_type(int bits);
        llvm::Type* llvm_float_type(const std::string& flux_type);
        
        // ── Генерация выражений ───────────────────────────────────
        llvm::Value* codegen_expr(ASTNode& node);
        llvm::Value* codegen(IntLiteral& node);
        llvm::Value* codegen(IdentExpr& node);
        llvm::Value* codegen(BinaryExpr& node);
        llvm::Value* codegen(CallExpr& node);
        
        // ── Вспомогательные ───────────────────────────────────────
        llvm::Function* get_or_declare_function(const std::string& name, llvm::FunctionType* type);
        llvm::AllocaInst* create_entry_block_alloca(llvm::Function* func, const std::string& name,llvm::Type* type);
        std::string mangle_name(const std::string& flux_name);
    };

} // namespace flux

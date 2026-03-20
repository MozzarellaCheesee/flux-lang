#include "flux/codegen/codegen.h"
#include "flux/ast/expr.h"
#include "flux/ast/stmt.h"
#include "flux/ast/decl.h"
#include "flux/ast/type.h"

#include <llvm/IR/Verifier.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>

#include <llvm/IR/DerivedTypes.h>
#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/APInt.h>

#include <iostream>

namespace flux {

    // ═══════════════════════════════════════════════════
    // Конструктор
    // ═══════════════════════════════════════════════════
    LLVMCodegen::LLVMCodegen(const std::string& module_name)
        : builder_(ctx_)
        , module_(std::make_unique<llvm::Module>(module_name, ctx_))
    {
        declare_builtins();
    }

    // ═══════════════════════════════════════════════════
    // emit_object_file  — главная точка выхода
    // ═══════════════════════════════════════════════════
    static llvm::TargetMachine* create_target_machine(llvm::Module& mod, const std::string& override_triple) {
        llvm::InitializeNativeTarget();
        llvm::InitializeNativeTargetAsmPrinter();
        llvm::InitializeNativeTargetAsmParser();

        std::string triple = override_triple.empty()
            ? llvm::sys::getDefaultTargetTriple()
            : override_triple;
        mod.setTargetTriple(triple);

        std::string err;
        const llvm::Target* target =
            llvm::TargetRegistry::lookupTarget(triple, err);
        if (!target) {
            llvm::errs() << "[codegen] Target lookup failed: " << err << "\n";
            return nullptr;
        }

        llvm::TargetOptions opt;
        auto rm = std::optional<llvm::Reloc::Model>(llvm::Reloc::PIC_);
        auto* tm = target->createTargetMachine(triple, "generic", "", opt, rm);
        mod.setDataLayout(tm->createDataLayout());
        return tm;
    }

    bool LLVMCodegen::emit_object_file(const std::string& output_path) {
        std::string err;
        llvm::raw_string_ostream es(err);
        if (llvm::verifyModule(*module_, &es)) {
            std::cerr << "[codegen] Module verification failed:\n" << err << "\n";
            return false;
        }
        auto* tm = create_target_machine(*module_, target_triple_);
        if (!tm) return false;

        std::error_code ec;
        llvm::raw_fd_ostream dest(output_path, ec, llvm::sys::fs::OF_None);
        if (ec) {
            std::cerr << "[codegen] Cannot open output file: " << ec.message() << "\n";
            return false;
        }
        llvm::legacy::PassManager pass;
        if (tm->addPassesToEmitFile(pass, dest, nullptr,
                                    llvm::CodeGenFileType::ObjectFile)) {
            std::cerr << "[codegen] Cannot emit object file\n";
            return false;
        }
        pass.run(*module_);
        dest.flush();
        return true;
    }

    bool LLVMCodegen::emit_assembly(const std::string& output_path) {
        std::string err;
        llvm::raw_string_ostream es(err);
        if (llvm::verifyModule(*module_, &es)) {
            std::cerr << "[codegen] Module verification failed:\n" << err << "\n";
            return false;
        }
        auto* tm = create_target_machine(*module_, target_triple_);
        if (!tm) return false;

        std::error_code ec;
        llvm::raw_fd_ostream dest(output_path, ec, llvm::sys::fs::OF_None);
        if (ec) {
            std::cerr << "[codegen] Cannot open output file: " << ec.message() << "\n";
            return false;
        }
        llvm::legacy::PassManager pass;
        if (tm->addPassesToEmitFile(pass, dest, nullptr,
                                    llvm::CodeGenFileType::AssemblyFile)) {
            std::cerr << "[codegen] Cannot emit assembly\n";
            return false;
        }
        pass.run(*module_);
        dest.flush();
        return true;
    }

    bool LLVMCodegen::emit_llvm_ir(const std::string& output_path) {
        std::error_code ec;
        llvm::raw_fd_ostream out(output_path, ec, llvm::sys::fs::OF_None);
        if (ec) {
            std::cerr << "[codegen] Cannot open file: " << ec.message() << "\n";
            return false;
        }
        module_->print(out, nullptr);
        return true;
    }

    void LLVMCodegen::dump_ir() {
        module_->print(llvm::errs(), nullptr);
    }

    // ───────────────────────────────────────────────────
    // Helpers
    // ───────────────────────────────────────────────────
    llvm::Value* LLVMCodegen::emit_expr(ASTNode& n) {
        n.accept(*this);
        return last_val_;
    }

    llvm::Value* LLVMCodegen::load_if_ptr(llvm::Value* v) {
        if (!v) return nullptr;
        if (auto* ai = llvm::dyn_cast<llvm::AllocaInst>(v))
            return builder_.CreateLoad(ai->getAllocatedType(), ai);
        return v;
    }

    llvm::AllocaInst* LLVMCodegen::create_entry_alloca(
            llvm::Function* f, const std::string& name, llvm::Type* ty) {
        llvm::IRBuilder<> tmp(&f->getEntryBlock(),
                            f->getEntryBlock().begin());
        return tmp.CreateAlloca(ty, nullptr, name);
    }

    llvm::Type* LLVMCodegen::llvm_primitive(const std::string& name) {
        // ── Boolean ──────────────────────────────────────────
        if (name == "bool")
            return llvm::Type::getInt1Ty(ctx_);

        // ── Signed integers ──────────────────────────────────
        if (name == "int8")   return llvm::Type::getInt8Ty(ctx_);
        if (name == "int16")  return llvm::Type::getInt16Ty(ctx_);
        if (name == "int32")  return llvm::Type::getInt32Ty(ctx_);
        if (name == "int64")  return llvm::Type::getInt64Ty(ctx_);
        if (name == "int128") return llvm::Type::getInt128Ty(ctx_);

        // ── Unsigned integers ────────────────────────────────
        if (name == "uint8")   return llvm::Type::getInt8Ty(ctx_);
        if (name == "uint16")  return llvm::Type::getInt16Ty(ctx_);
        if (name == "uint32")  return llvm::Type::getInt32Ty(ctx_);
        if (name == "uint64")  return llvm::Type::getInt64Ty(ctx_);
        if (name == "uint128") return llvm::Type::getInt128Ty(ctx_);

        // ── Float ────────────────────────────────────────────
        if (name == "float")  return llvm::Type::getFloatTy(ctx_);
        if (name == "double") return llvm::Type::getDoubleTy(ctx_);

        // ── Platform-sized integers ──────────────────────────
        if (name == "isize_t" || name == "usize_t") {
            auto dl = module_->getDataLayout();
            return llvm::Type::getIntNTy(ctx_, dl.getPointerSizeInBits());
        }
        // ── Strings ──────────────────────────────────────────
        if (name == "str" || name == "String")
            return llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(ctx_));

        // ── void ─────────────────────────────────────────────
        if (name == "void" || name == "()")
            return llvm::Type::getVoidTy(ctx_);

        // ── Пользовательский struct/class ────────────────────
        if (structs_.count(name))
            return structs_.at(name).type;

        // ── Неизвестный тип ──────────────────────────────────
        llvm::errs() << "[codegen] Unknown type: '" << name
                    << "', defaulting to i64\n";
        return llvm::Type::getInt64Ty(ctx_);
    }

    llvm::Type* LLVMCodegen::llvm_type(TypeNode* t) {
        if (!t) return llvm::Type::getVoidTy(ctx_);
        t->accept(*this);
        return last_type_;
    }

    // ═══════════════════════════════════════════════════
    // Types
    // ═══════════════════════════════════════════════════
    void LLVMCodegen::visit(PrimitiveType& n) {
        last_type_ = llvm_primitive(n.name);
    }
    void LLVMCodegen::visit(UnitType&) {
        last_type_ = llvm::Type::getVoidTy(ctx_);
    }
    void LLVMCodegen::visit(RefType& n) {
        n.inner->accept(*this);
        last_type_ = llvm::PointerType::getUnqual(last_type_);
    }
    void LLVMCodegen::visit(ArrayType& n) {
        // Размер массива должен быть IntLiteral после семантики
        n.element->accept(*this);
        auto* elem = last_type_;
        uint64_t sz = 0;
        if (n.size_expr) {
            if (auto* il = dynamic_cast<IntLiteral*>(n.size_expr.get()))
                sz = static_cast<uint64_t>(il->value);
        }
        last_type_ = llvm::ArrayType::get(elem, sz);
    }
    void LLVMCodegen::visit(SliceType& n) {
        n.element->accept(*this);
        // Слайс = { ptr, len } — простой указатель для начала
        last_type_ = llvm::PointerType::getUnqual(last_type_);
    }
    void LLVMCodegen::visit(GenericType& n) {
        last_type_ = llvm_primitive(n.name);
    }

    // ═══════════════════════════════════════════════════
    // Declarations
    // ═══════════════════════════════════════════════════
    void LLVMCodegen::visit(ProgramNode& n) {
        // Сначала объявляем все структуры
        for (auto& d : n.decls)
            if (auto* s = dynamic_cast<StructDecl*>(d.get()))
                s->accept(*this);
            else if (auto* c = dynamic_cast<ClassDecl*>(d.get()))
                c->accept(*this);

        // Затем функции
        for (auto& d : n.decls)
            if (!dynamic_cast<StructDecl*>(d.get()) &&
                !dynamic_cast<ClassDecl*>(d.get()))
                d->accept(*this);
    }

    void LLVMCodegen::visit(StructDecl& n) {
        std::vector<llvm::Type*> field_types;
        std::vector<std::string> field_names;
        for (auto& f : n.fields) {
            field_names.push_back(f->name);
            field_types.push_back(llvm_type(f->type.get()));
        }
        auto* st = llvm::StructType::create(ctx_, field_types, n.name);
        structs_[n.name] = {st, field_names};
    }

    void LLVMCodegen::visit(ClassDecl& n) {
        std::vector<llvm::Type*> field_types;
        std::vector<std::string> field_names;
        for (auto& f : n.fields) {
            field_names.push_back(f->name);
            field_types.push_back(llvm_type(f->type.get()));
        }
        auto* st = llvm::StructType::create(ctx_, field_types, n.name);
        structs_[n.name] = {st, field_names};

        // Эмитируем методы как обычные функции с именем "ClassName_method"
        for (auto& m : n.methods) {
            std::string mangled = n.name + "_" + m->name;
            auto saved_name = m->name;
            m->name = mangled;
            m->has_self = true;
            m->accept(*this);
            m->name = saved_name;
        }
    }

    void LLVMCodegen::visit(ImplDecl& n) {
        for (auto& m : n.methods) {
            auto saved = m->name;
            m->name = n.target + "_" + m->name;
            m->has_self = true;
            m->accept(*this);
            m->name = saved;
        }
    }

    void LLVMCodegen::visit(FuncDecl& n) {
        std::vector<llvm::Type*> param_types;
        llvm::Type* self_type = nullptr;

        if (n.has_self && structs_.count(
                n.name.substr(0, n.name.find('_')))) {
            auto struct_name = n.name.substr(0, n.name.find('_'));
            self_type = llvm::PointerType::getUnqual(structs_[struct_name].type);
            param_types.push_back(self_type);
        }

        for (auto& p : n.params)
            param_types.push_back(llvm_type(p->type.get()));

        llvm::Type* ret_type = n.return_type
            ? llvm_type(n.return_type.get())
            : llvm::Type::getVoidTy(ctx_);

        auto* ft = llvm::FunctionType::get(ret_type, param_types, false);

        auto linkage = (n.name == "main" || n.is_pub)
            ? llvm::Function::ExternalLinkage
            : llvm::Function::InternalLinkage;

        auto* func = module_->getFunction(n.name);
        if (!func)
            func = llvm::Function::Create(ft, linkage, n.name, *module_);

        if (!n.body) return;

        auto* bb = llvm::BasicBlock::Create(ctx_, "entry", func);
        builder_.SetInsertPoint(bb);

        locals_.clear();
        self_ptr_ = nullptr;

        auto arg_it = func->arg_begin();
        if (n.has_self && self_type) {
            self_ptr_ = &*arg_it++;
            self_ptr_->setName("self");
        }
        for (auto& p : n.params) {
            llvm::Value* arg = &*arg_it++;
            arg->setName(p->name);
            auto* alloca = create_entry_alloca(func, p->name,
                                            llvm_type(p->type.get()));
            builder_.CreateStore(arg, alloca);
            locals_[p->name] = alloca;
        }

        n.body->accept(*this);

        auto* current_bb = builder_.GetInsertBlock();
        if (current_bb && !current_bb->getTerminator()) {
            if (ret_type->isVoidTy())
                builder_.CreateRetVoid();
            else
                builder_.CreateRet(llvm::Constant::getNullValue(ret_type));
        }
    }


    void LLVMCodegen::visit(VarDecl& n) {
        auto* func = builder_.GetInsertBlock()->getParent();

        // Определяем тип переменной
        llvm::Type* ty = nullptr;
        if (n.type) {
            ty = llvm_type(n.type.get());
            if (!ty || ty->isVoidTy()) ty = llvm::Type::getInt64Ty(ctx_);
        } else if (n.init) {
            // Вывод типа из инициализатора
            auto* init_val = load_if_ptr(emit_expr(*n.init));
            ty = init_val ? init_val->getType() : llvm::Type::getInt64Ty(ctx_);
            auto* alloca = create_entry_alloca(func, n.name, ty);
            locals_[n.name] = alloca;
            if (init_val) builder_.CreateStore(init_val, alloca);
            last_val_ = alloca;
            return;
        } else {
            ty = llvm::Type::getInt64Ty(ctx_);
        }

        auto* alloca = create_entry_alloca(func, n.name, ty);
        locals_[n.name] = alloca;

        if (n.init) {
            if (auto* si = dynamic_cast<StructInitExpr*>(n.init.get())) {
                // ✅ Struct literal — инициализируем поля напрямую в alloca
                if (structs_.count(si->type_name)) {
                    auto& info = structs_[si->type_name];
                    for (auto& fi : si->fields) {
                        auto it = std::find(info.field_names.begin(),
                                            info.field_names.end(), fi.name);
                        if (it == info.field_names.end()) continue;
                        size_t idx = it - info.field_names.begin();
                        auto* gep = builder_.CreateStructGEP(info.type, alloca, idx);
                        auto* val = load_if_ptr(emit_expr(*fi.value));
                        auto* field_ty = info.type->getElementType(idx);
                        if (val && val->getType() != field_ty &&
                            val->getType()->isIntegerTy() && field_ty->isIntegerTy())
                            val = builder_.CreateIntCast(val, field_ty, true);
                        if (val) builder_.CreateStore(val, gep);
                    }
                }
            } else {
                auto* val = load_if_ptr(emit_expr(*n.init));
                if (val) {
                    // ✅ Привести тип инициализатора к типу переменной
                    if (val->getType() != ty) {
                        bool v_int = val->getType()->isIntegerTy();
                        bool t_int = ty->isIntegerTy();
                        bool v_fp  = val->getType()->isFloatingPointTy();
                        bool t_fp  = ty->isFloatingPointTy();

                        if (v_int && t_int)
                            val = builder_.CreateIntCast(val, ty, true);
                        else if (v_int && t_fp)
                            val = builder_.CreateSIToFP(val, ty);
                        else if (v_fp && t_int)
                            val = builder_.CreateFPToSI(val, ty);
                        else if (v_fp && t_fp)
                            val = builder_.CreateFPCast(val, ty);
                    }
                    builder_.CreateStore(val, alloca);
                }
            }
        }

        last_val_ = alloca;
    }

    void LLVMCodegen::visit(ParamDecl&) {} // !
    void LLVMCodegen::visit(FieldDecl&) {} // !

    // ═══════════════════════════════════════════════════
    // Statements
    // ═══════════════════════════════════════════════════
    void LLVMCodegen::visit(BlockStmt& n) {
        for (auto& s : n.stmts)
            s->accept(*this);
    }

    void LLVMCodegen::visit(ReturnStmt& n) {
        if (n.value) {
            auto* v = load_if_ptr(emit_expr(*n.value));
            auto* ret_ty = builder_.GetInsertBlock()
                            ->getParent()->getReturnType();

            // Привести тип возвращаемого значения к типу функции
            if (v && v->getType() != ret_ty && !ret_ty->isVoidTy()) {
                bool v_int  = v->getType()->isIntegerTy();
                bool r_int  = ret_ty->isIntegerTy();
                bool v_fp   = v->getType()->isFloatingPointTy();
                bool r_fp   = ret_ty->isFloatingPointTy();

                if (v_int && r_int)
                    v = builder_.CreateIntCast(v, ret_ty, /*isSigned=*/true);
                else if (v_int && r_fp)
                    v = builder_.CreateSIToFP(v, ret_ty);
                else if (v_fp && r_int)
                    v = builder_.CreateFPToSI(v, ret_ty);
                else if (v_fp && r_fp)
                    v = builder_.CreateFPCast(v, ret_ty);
            }

            builder_.CreateRet(v);
        } else {
            builder_.CreateRetVoid();
        }
    }


    void LLVMCodegen::visit(ExprStmt& n) { // !
        n.expr->accept(*this);
    }

    void LLVMCodegen::visit(IfStmt& n) {
        auto* func = builder_.GetInsertBlock()->getParent();
        auto* cond_val = load_if_ptr(emit_expr(*n.cond));

        // Привести к i1
        if (cond_val->getType()->isIntegerTy() &&
            !cond_val->getType()->isIntegerTy(1))
            cond_val = builder_.CreateICmpNE(
                cond_val,
                llvm::ConstantInt::get(cond_val->getType(), 0));

        auto* then_bb  = llvm::BasicBlock::Create(ctx_, "then", func);
        auto* else_bb  = llvm::BasicBlock::Create(ctx_, "else");
        auto* merge_bb = llvm::BasicBlock::Create(ctx_, "ifcont");

        builder_.CreateCondBr(cond_val, then_bb,
                            n.else_branch ? else_bb : merge_bb);

        builder_.SetInsertPoint(then_bb);
        n.then_block->accept(*this);
        if (!builder_.GetInsertBlock()->getTerminator())
            builder_.CreateBr(merge_bb);

        if (n.else_branch) {
            else_bb->insertInto(func);
            builder_.SetInsertPoint(else_bb);
            n.else_branch->accept(*this);
            if (!builder_.GetInsertBlock()->getTerminator())
                builder_.CreateBr(merge_bb);
        }

        merge_bb->insertInto(func); 
        builder_.SetInsertPoint(merge_bb);
    }

    void LLVMCodegen::visit(WhileStmt& n) {
        auto* func = builder_.GetInsertBlock()->getParent();
        auto* cond_bb = llvm::BasicBlock::Create(ctx_, "while.cond", func);
        auto* body_bb = llvm::BasicBlock::Create(ctx_, "while.body", func);
        auto* exit_bb = llvm::BasicBlock::Create(ctx_, "while.exit", func);

        loop_exit_blocks_.push(exit_bb);
        loop_cont_blocks_.push(cond_bb);

        builder_.CreateBr(cond_bb);
        builder_.SetInsertPoint(cond_bb);
        auto* cond_v = load_if_ptr(emit_expr(*n.cond));
        if (cond_v->getType()->isIntegerTy() &&
            !cond_v->getType()->isIntegerTy(1))
            cond_v = builder_.CreateICmpNE(
                cond_v,
                llvm::ConstantInt::get(cond_v->getType(), 0));
        builder_.CreateCondBr(cond_v, body_bb, exit_bb);

        builder_.SetInsertPoint(body_bb);
        n.body->accept(*this);
        if (!builder_.GetInsertBlock()->getTerminator())
            builder_.CreateBr(cond_bb);

        builder_.SetInsertPoint(exit_bb);
        loop_exit_blocks_.pop();
        loop_cont_blocks_.pop();
    }

    void LLVMCodegen::visit(ForStmt& n) {
        auto* func = builder_.GetInsertBlock()->getParent();
        if (n.init) n.init->accept(*this);

        auto* cond_bb = llvm::BasicBlock::Create(ctx_, "for.cond", func);
        auto* body_bb = llvm::BasicBlock::Create(ctx_, "for.body", func);
        auto* step_bb = llvm::BasicBlock::Create(ctx_, "for.step", func);
        auto* exit_bb = llvm::BasicBlock::Create(ctx_, "for.exit", func);

        loop_exit_blocks_.push(exit_bb);
        loop_cont_blocks_.push(step_bb);

        builder_.CreateBr(cond_bb);
        builder_.SetInsertPoint(cond_bb);
        if (n.cond) {
            auto* cv = load_if_ptr(emit_expr(*n.cond));
            if (cv->getType()->isIntegerTy() &&
                !cv->getType()->isIntegerTy(1))
                cv = builder_.CreateICmpNE(
                    cv, llvm::ConstantInt::get(cv->getType(), 0));
            builder_.CreateCondBr(cv, body_bb, exit_bb);
        } else {
            builder_.CreateBr(body_bb);
        }

        builder_.SetInsertPoint(body_bb);
        n.body->accept(*this);
        if (!builder_.GetInsertBlock()->getTerminator())
            builder_.CreateBr(step_bb);

        builder_.SetInsertPoint(step_bb);
        if (n.step) n.step->accept(*this);
        builder_.CreateBr(cond_bb);

        builder_.SetInsertPoint(exit_bb);
        loop_exit_blocks_.pop();
        loop_cont_blocks_.pop();
    }

    void LLVMCodegen::visit(BreakStmt&) {
        if (!loop_exit_blocks_.empty()) {
            builder_.CreateBr(loop_exit_blocks_.top());
            auto* func = builder_.GetInsertBlock()->getParent();
            auto* dead = llvm::BasicBlock::Create(ctx_, "after.break", func);
            builder_.SetInsertPoint(dead);
        }
    }

    void LLVMCodegen::visit(ContinueStmt&) {
        if (!loop_cont_blocks_.empty()) {
            builder_.CreateBr(loop_cont_blocks_.top());
            auto* func = builder_.GetInsertBlock()->getParent();
            auto* dead = llvm::BasicBlock::Create(ctx_, "after.continue", func);
            builder_.SetInsertPoint(dead);
        }
    }

    // ═══════════════════════════════════════════════════
    // Expressions
    // ═══════════════════════════════════════════════════
    void LLVMCodegen::visit(IntLiteral& n) {
        last_val_ = llvm::ConstantInt::get(
            llvm::Type::getInt64Ty(ctx_), n.value, /*isSigned=*/true);
    }
    void LLVMCodegen::visit(FloatLiteral& n) {
        last_val_ = llvm::ConstantFP::get(
            llvm::Type::getDoubleTy(ctx_), n.value);
    }
    void LLVMCodegen::visit(BoolLiteral& n) {
        last_val_ = llvm::ConstantInt::get(
            llvm::Type::getInt1Ty(ctx_), n.value ? 1 : 0);
    }
    void LLVMCodegen::visit(StrLiteral& n) {
        last_val_ = builder_.CreateGlobalStringPtr(n.value);
    }
    void LLVMCodegen::visit(ArrayLiteral& n) {
        // Возвращаем alloca с заполненным массивом
        if (n.elements.empty()) { last_val_ = nullptr; return; }
        auto* first = load_if_ptr(emit_expr(*n.elements[0]));
        auto* arr_ty = llvm::ArrayType::get(
            first->getType(), n.elements.size());
        auto* func = builder_.GetInsertBlock()->getParent();
        auto* alloca = create_entry_alloca(func, "arr", arr_ty);

        for (size_t i = 0; i < n.elements.size(); ++i) {
            auto* val = load_if_ptr(emit_expr(*n.elements[i]));
            auto* ptr = builder_.CreateGEP(
                arr_ty, alloca,
                {llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), 0),
                llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), i)});
            builder_.CreateStore(val, ptr);
        }
        last_val_ = alloca;
    }

    void LLVMCodegen::visit(IdentExpr& n) {
        if (locals_.count(n.name)) {
            last_val_ = locals_.at(n.name); // возвращаем alloca, load_if_ptr по запросу
            return;
        }
        // Глобальная функция?
        if (auto* f = module_->getFunction(n.name)) {
            last_val_ = f;
            return;
        }
        last_val_ = nullptr;
    }

    void LLVMCodegen::visit(BinaryExpr& n) {
        auto* lhs = load_if_ptr(emit_expr(*n.lhs));
        auto* rhs = load_if_ptr(emit_expr(*n.rhs));
        if (!lhs || !rhs) { last_val_ = nullptr; return; }

        bool is_fp = lhs->getType()->isDoubleTy() ||
                    lhs->getType()->isFloatTy();

        const auto& op = n.op;
        if (op == "+")  last_val_ = is_fp ? builder_.CreateFAdd(lhs, rhs)
                                        : builder_.CreateAdd(lhs, rhs);
        else if (op == "-")  last_val_ = is_fp ? builder_.CreateFSub(lhs, rhs)
                                                : builder_.CreateSub(lhs, rhs);
        else if (op == "*")  last_val_ = is_fp ? builder_.CreateFMul(lhs, rhs)
                                                : builder_.CreateMul(lhs, rhs);
        else if (op == "/")  last_val_ = is_fp ? builder_.CreateFDiv(lhs, rhs)
                                                : builder_.CreateSDiv(lhs, rhs);
        else if (op == "%")  last_val_ = builder_.CreateSRem(lhs, rhs);
        else if (op == "==") last_val_ = is_fp ? builder_.CreateFCmpOEQ(lhs, rhs)
                                                : builder_.CreateICmpEQ(lhs, rhs);
        else if (op == "!=") last_val_ = is_fp ? builder_.CreateFCmpONE(lhs, rhs)
                                                : builder_.CreateICmpNE(lhs, rhs);
        else if (op == "<")  last_val_ = is_fp ? builder_.CreateFCmpOLT(lhs, rhs)
                                                : builder_.CreateICmpSLT(lhs, rhs);
        else if (op == "<=") last_val_ = is_fp ? builder_.CreateFCmpOLE(lhs, rhs)
                                                : builder_.CreateICmpSLE(lhs, rhs);
        else if (op == ">")  last_val_ = is_fp ? builder_.CreateFCmpOGT(lhs, rhs)
                                                : builder_.CreateICmpSGT(lhs, rhs);
        else if (op == ">=") last_val_ = is_fp ? builder_.CreateFCmpOGE(lhs, rhs)
                                                : builder_.CreateICmpSGE(lhs, rhs);
        else if (op == "&&") last_val_ = builder_.CreateAnd(lhs, rhs);
        else if (op == "||") last_val_ = builder_.CreateOr(lhs, rhs);
        else if (op == "&")  last_val_ = builder_.CreateAnd(lhs, rhs);
        else if (op == "|")  last_val_ = builder_.CreateOr(lhs, rhs);
        else if (op == "^")  last_val_ = builder_.CreateXor(lhs, rhs);
        else if (op == "<<") last_val_ = builder_.CreateShl(lhs, rhs);
        else if (op == ">>") last_val_ = builder_.CreateAShr(lhs, rhs);
        else last_val_ = lhs;
    }

    void LLVMCodegen::visit(UnaryExpr& n) {
        using Op = UnaryExpr::Op;

        switch (n.op) {
            case Op::Neg: {
                auto* v = load_if_ptr(emit_expr(*n.operand));
                last_val_ = v->getType()->isFloatingPointTy()
                    ? builder_.CreateFNeg(v)
                    : builder_.CreateNeg(v);
                break;
            }
            case Op::Not:
                last_val_ = builder_.CreateNot(
                    load_if_ptr(emit_expr(*n.operand)));
                break;
            case Op::BitNot:
                last_val_ = builder_.CreateNot(
                    load_if_ptr(emit_expr(*n.operand)));
                break;
            case Op::Deref: {
                auto* ptr = load_if_ptr(emit_expr(*n.operand));
                llvm::Type* elem_ty = nullptr;
                if (auto* ai = llvm::dyn_cast<llvm::AllocaInst>(ptr))
                    elem_ty = ai->getAllocatedType();
                else
                    elem_ty = llvm::Type::getInt64Ty(ctx_); // fallback
                auto* loaded = builder_.CreateLoad(elem_ty, ptr);
                last_val_ = loaded;
                break;
            }
            case Op::AddrOf: {
                n.operand->accept(*this);
                break;
            }
            case Op::PreInc: {
                n.operand->accept(*this);
                auto* alloca = last_val_;
                auto* ty = llvm::cast<llvm::AllocaInst>(alloca)->getAllocatedType();
                auto* v = builder_.CreateLoad(ty, alloca);
                auto* inc = builder_.CreateAdd(v, llvm::ConstantInt::get(ty, 1));
                builder_.CreateStore(inc, alloca);
                last_val_ = inc;
                break;
            }
            case Op::PreDec: {
                n.operand->accept(*this);
                auto* alloca = last_val_;
                auto* ty = llvm::cast<llvm::AllocaInst>(alloca)->getAllocatedType();
                auto* v = builder_.CreateLoad(ty, alloca);
                auto* dec = builder_.CreateSub(v, llvm::ConstantInt::get(ty, 1));
                builder_.CreateStore(dec, alloca);
                last_val_ = dec;
                break;
            }
            case Op::PostInc: {
                n.operand->accept(*this);
                auto* alloca = last_val_;
                auto* ty = llvm::cast<llvm::AllocaInst>(alloca)->getAllocatedType();
                auto* v = builder_.CreateLoad(ty, alloca);
                builder_.CreateStore(
                    builder_.CreateAdd(v, llvm::ConstantInt::get(ty, 1)), alloca);
                last_val_ = v; // возвращаем старое значение
                break;
            }
            case Op::PostDec: {
                n.operand->accept(*this);
                auto* alloca = last_val_;
                auto* ty = llvm::cast<llvm::AllocaInst>(alloca)->getAllocatedType();
                auto* v = builder_.CreateLoad(ty, alloca);
                builder_.CreateStore(
                    builder_.CreateSub(v, llvm::ConstantInt::get(ty, 1)), alloca);
                last_val_ = v;
                break;
            }
        }
    }

    void LLVMCodegen::visit(AssignExpr& n) {
        // Получаем указатель (alloca или GEP)
        n.target->accept(*this);
        auto* ptr = last_val_;

        auto* rhs = load_if_ptr(emit_expr(*n.value));

        if (n.op == "=") {
            builder_.CreateStore(rhs, ptr);
            last_val_ = rhs;
            return;
        }
        // Составные присваивания: +=, -=, и т.д.
        auto* lhs = load_if_ptr(ptr);
        llvm::Value* result = nullptr;
        bool is_fp = lhs->getType()->isFloatingPointTy();
        if      (n.op == "+=") result = is_fp ? builder_.CreateFAdd(lhs, rhs) : builder_.CreateAdd(lhs, rhs);
        else if (n.op == "-=") result = is_fp ? builder_.CreateFSub(lhs, rhs) : builder_.CreateSub(lhs, rhs);
        else if (n.op == "*=") result = is_fp ? builder_.CreateFMul(lhs, rhs) : builder_.CreateMul(lhs, rhs);
        else if (n.op == "/=") result = is_fp ? builder_.CreateFDiv(lhs, rhs) : builder_.CreateSDiv(lhs, rhs);
        else result = rhs;
        builder_.CreateStore(result, ptr);
        last_val_ = result;
    }

    void LLVMCodegen::visit(CallExpr& n) {
        auto* callee = module_->getFunction(n.callee);
        if (!callee) {
            llvm::errs() << "[codegen] Unknown function: '" << n.callee << "'\n";
            last_val_ = nullptr;
            return;
        }

        std::vector<llvm::Value*> args;
        auto* ft = callee->getFunctionType();

        for (size_t i = 0; i < n.args.size(); ++i) {
            auto* v = load_if_ptr(emit_expr(*n.args[i]));
            if (i < ft->getNumParams()) {
                auto* expected = ft->getParamType(i);
                if (v && v->getType() != expected) {
                    if (v->getType()->isIntegerTy() && expected->isIntegerTy())
                        v = builder_.CreateIntCast(v, expected, true);
                    else if (v->getType()->isIntegerTy() && expected->isFloatingPointTy())
                        v = builder_.CreateSIToFP(v, expected);
                    else if (v->getType()->isFloatingPointTy() && expected->isIntegerTy())
                        v = builder_.CreateFPToSI(v, expected);
                }
            }
            args.push_back(v);
        }

        last_val_ = callee->getReturnType()->isVoidTy()
            ? builder_.CreateCall(callee, args)
            : builder_.CreateCall(callee, args, "calltmp");
    }


    void LLVMCodegen::visit(MethodCallExpr& n) {
        // Имя метода: "TypeName_methodName"
        n.receiver->accept(*this);
        auto* recv_ptr = last_val_;

        // Определяем имя struct по типу указателя
        std::string mangled;
        if (auto* ai = llvm::dyn_cast<llvm::AllocaInst>(recv_ptr)) {
            if (auto* st = llvm::dyn_cast<llvm::StructType>(
                    ai->getAllocatedType()))
                mangled = st->getName().str() + "_" + n.method;
        }
        auto* callee = module_->getFunction(mangled);
        if (!callee) { last_val_ = nullptr; return; }

        std::vector<llvm::Value*> args = {recv_ptr};
        for (auto& a : n.args)
            args.push_back(load_if_ptr(emit_expr(*a)));

        last_val_ = callee->getReturnType()->isVoidTy()
            ? builder_.CreateCall(callee, args)
            : builder_.CreateCall(callee, args, "mcall");
    }

    void LLVMCodegen::visit(IndexExpr& n) {
        n.array->accept(*this);
        auto* arr_ptr = last_val_;
        auto* idx = load_if_ptr(emit_expr(*n.index));

        llvm::Type* arr_ty = nullptr;
        if (auto* ai = llvm::dyn_cast<llvm::AllocaInst>(arr_ptr))
            arr_ty = ai->getAllocatedType();

        if (arr_ty && arr_ty->isArrayTy()) {
            auto* gep = builder_.CreateGEP(
                arr_ty, arr_ptr,
                {llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), 0), idx});
            last_val_ = gep;
        } else {
            // ptr[idx]
            llvm::Type* elem_ty = nullptr;
            if (auto* ai = llvm::dyn_cast<llvm::AllocaInst>(arr_ptr))
                elem_ty = ai->getAllocatedType();
            else
                elem_ty = llvm::Type::getInt64Ty(ctx_);
            last_val_ = builder_.CreateGEP(elem_ty, arr_ptr, idx);
        }
    }

    void LLVMCodegen::visit(ImplicitCastExpr& n) {
        auto* val = load_if_ptr(emit_expr(*n.inner));
        auto* to   = llvm_type(n.target_type.get());
        if (!val || !to) { last_val_ = val; return; }

        auto* from = val->getType();
        if (from == to) { last_val_ = val; return; }

        bool from_fp = from->isFloatingPointTy();
        bool to_fp   = to->isFloatingPointTy();

        if (!from_fp && to_fp)
            last_val_ = builder_.CreateSIToFP(val, to);
        else if (from_fp && !to_fp)
            last_val_ = builder_.CreateFPToSI(val, to);
        else if (!from_fp && !to_fp)
            last_val_ = builder_.CreateIntCast(val, to, true);
        else
            last_val_ = builder_.CreateFPCast(val, to);
    }

    void LLVMCodegen::visit(FieldAccessExpr& n) {
        n.object->accept(*this);
        auto* obj_ptr = last_val_;

        llvm::StructType* st = nullptr;
        if (auto* ai = llvm::dyn_cast<llvm::AllocaInst>(obj_ptr))
            st = llvm::dyn_cast<llvm::StructType>(ai->getAllocatedType());

        if (!st) { last_val_ = nullptr; return; }

        // Найдём индекс поля
        for (auto& [name, info] : structs_) {
            if (info.type == st) {
                for (size_t i = 0; i < info.field_names.size(); ++i) {
                    if (info.field_names[i] == n.field) {
                        last_val_ = builder_.CreateStructGEP(st, obj_ptr, i);
                        return;
                    }
                }
            }
        }
        last_val_ = nullptr;
    }

    void LLVMCodegen::visit(StructInitExpr& n) {
        if (!structs_.count(n.type_name)) { last_val_ = nullptr; return; }
        auto& info = structs_[n.type_name];
        auto* func = builder_.GetInsertBlock()->getParent();
        auto* alloca = create_entry_alloca(func, n.type_name, info.type);

        for (auto& fi : n.fields) {
            auto it = std::find(info.field_names.begin(),
                                info.field_names.end(), fi.name);
            if (it == info.field_names.end()) continue;
            size_t idx = it - info.field_names.begin();
            auto* gep = builder_.CreateStructGEP(info.type, alloca, idx);
            auto* val = load_if_ptr(emit_expr(*fi.value));
            auto* field_ty = info.type->getElementType(idx);
            if (val && val->getType() != field_ty) {
                if (val->getType()->isIntegerTy() && field_ty->isIntegerTy())
                    val = builder_.CreateIntCast(val, field_ty, true);
                else if (val->getType()->isIntegerTy() && field_ty->isFloatingPointTy())
                    val = builder_.CreateSIToFP(val, field_ty);
                else if (val->getType()->isFloatingPointTy() && field_ty->isIntegerTy())
                    val = builder_.CreateFPToSI(val, field_ty);
            }
            builder_.CreateStore(val, gep);
        }
        last_val_ = alloca;
    }

    void LLVMCodegen::visit(SelfExpr&) {
        last_val_ = self_ptr_;
    }

    void LLVMCodegen::visit(MatchExpr& n) {
        auto* func  = builder_.GetInsertBlock()->getParent();
        auto* subj  = load_if_ptr(emit_expr(*n.subject));
        auto* merge = llvm::BasicBlock::Create(ctx_, "match.end", func);

        llvm::BasicBlock* default_bb = merge;

        std::vector<llvm::BasicBlock*> arm_bbs;
        for (size_t i = 0; i < n.arms.size(); ++i) {
            auto* arm_bb = llvm::BasicBlock::Create(
                ctx_, "match.arm" + (i ? std::to_string(i) : ""), func);
            arm_bbs.push_back(arm_bb);
            if (dynamic_cast<WildcardPattern*>(n.arms[i]->pattern.get()))
                default_bb = arm_bb;
        }

        bool is_int = subj && subj->getType()->isIntegerTy();
        bool is_str = subj && subj->getType()->isPointerTy();

        if (is_int) {
            // ── Integer / bool → llvm SwitchInst ──────────────
            auto* sw = builder_.CreateSwitch(subj, default_bb, n.arms.size());

            for (size_t i = 0; i < n.arms.size(); ++i) {
                auto* arm = n.arms[i].get();
                if (dynamic_cast<WildcardPattern*>(arm->pattern.get()))
                    continue;
                if (auto* lp = dynamic_cast<LiteralPattern*>(arm->pattern.get())) {
                    auto* lit_val = load_if_ptr(emit_expr(*lp->value));
                    auto* case_val = llvm::dyn_cast<llvm::ConstantInt>(lit_val);
                    if (case_val && case_val->getType() != subj->getType()) {
                        case_val = llvm::ConstantInt::get(
                            llvm::cast<llvm::IntegerType>(subj->getType()),
                            case_val->getSExtValue(),
                            /*isSigned=*/true);
                    }
                    if (case_val)
                        sw->addCase(case_val, arm_bbs[i]);
                }
            }

        } else {
            // ── String / float → цепочка if-else ──────────────
            llvm::Function* strcmp_fn = module_->getFunction("strcmp");
            if (!strcmp_fn && is_str) {
                auto* i8ptr = llvm::PointerType::getUnqual(
                    llvm::Type::getInt8Ty(ctx_));
                auto* ft = llvm::FunctionType::get(
                    llvm::Type::getInt32Ty(ctx_), {i8ptr, i8ptr}, false);
                strcmp_fn = llvm::Function::Create(
                    ft, llvm::Function::ExternalLinkage, "strcmp", *module_);
            }

            llvm::BasicBlock* check_bb = builder_.GetInsertBlock();
            for (size_t i = 0; i < n.arms.size(); ++i) {
                auto* arm = n.arms[i].get();
                if (dynamic_cast<WildcardPattern*>(arm->pattern.get()))
                    continue;

                bool is_last_non_wild = true;
                for (size_t j = i + 1; j < n.arms.size(); ++j) {
                    if (!dynamic_cast<WildcardPattern*>(n.arms[j]->pattern.get())) {
                        is_last_non_wild = false;
                        break;
                    }
                }
                auto* next_bb = is_last_non_wild
                    ? default_bb
                    : llvm::BasicBlock::Create(ctx_, "match.check", func);

                builder_.SetInsertPoint(check_bb);
                llvm::Value* cond = nullptr;

                if (auto* lp = dynamic_cast<LiteralPattern*>(arm->pattern.get())) {
                    auto* pat_val = load_if_ptr(emit_expr(*lp->value));
                    if (is_str && strcmp_fn) {
                        auto* r = builder_.CreateCall(strcmp_fn, {subj, pat_val});
                        cond = builder_.CreateICmpEQ(
                            r, llvm::ConstantInt::get(
                                llvm::Type::getInt32Ty(ctx_), 0));
                    } else {
                        cond = builder_.CreateFCmpOEQ(subj, pat_val);
                    }
                }

                if (cond)
                    builder_.CreateCondBr(cond, arm_bbs[i], next_bb);
                else
                    builder_.CreateBr(default_bb);

                check_bb = next_bb;
            }

            builder_.SetInsertPoint(check_bb);
            if (!builder_.GetInsertBlock()->getTerminator())
                builder_.CreateBr(default_bb);
        }

        // ── Тела arm-блоков ───────────────────────────────────
        for (size_t i = 0; i < n.arms.size(); ++i) {
            builder_.SetInsertPoint(arm_bbs[i]);
            if (n.arms[i]->body)
                n.arms[i]->body->accept(*this);
            if (!builder_.GetInsertBlock()->getTerminator())
                builder_.CreateBr(merge);
        }

        builder_.SetInsertPoint(merge);
        last_val_ = nullptr;
    }



    void LLVMCodegen::visit(MatchArm& n) {
        if (n.body) n.body->accept(*this);
    }


    void LLVMCodegen::declare_builtins() {
        auto* i8ptr  = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(ctx_));
        auto* i32    = llvm::Type::getInt32Ty(ctx_);
        auto* i64    = llvm::Type::getInt64Ty(ctx_);
        auto* voidTy = llvm::Type::getVoidTy(ctx_);

        // printf(i8*, ...) -> i32  — базовый C printf
        auto* printf_ty = llvm::FunctionType::get(i32, {i8ptr}, /*vararg=*/true);
        auto* printf_fn = llvm::Function::Create(
            printf_ty, llvm::Function::ExternalLinkage, "printf", *module_);

        // puts(i8*) -> i32
        auto* puts_ty = llvm::FunctionType::get(i32, {i8ptr}, false);
        llvm::Function::Create(puts_ty, llvm::Function::ExternalLinkage,
                            "puts", *module_);

        // ── Flux builtins ────────────────────────────────────

        // println(str) — печатает строку + \n
        {
            auto* ft = llvm::FunctionType::get(voidTy, {i8ptr}, false);
            auto* fn = llvm::Function::Create(
                ft, llvm::Function::InternalLinkage, "println", *module_);
            auto* bb = llvm::BasicBlock::Create(ctx_, "entry", fn);
            llvm::IRBuilder<> b(bb);
            auto* fmt = b.CreateGlobalStringPtr("%s\n", "println_fmt");
            b.CreateCall(printf_fn, {fmt, fn->arg_begin()});
            b.CreateRetVoid();
        }

        // print(str) — без \n
        {
            auto* ft = llvm::FunctionType::get(voidTy, {i8ptr}, false);
            auto* fn = llvm::Function::Create(
                ft, llvm::Function::InternalLinkage, "print", *module_);
            auto* bb = llvm::BasicBlock::Create(ctx_, "entry", fn);
            llvm::IRBuilder<> b(bb);
            auto* fmt = b.CreateGlobalStringPtr("%s", "print_fmt");
            b.CreateCall(printf_fn, {fmt, fn->arg_begin()});
            b.CreateRetVoid();
        }

        // println_int(i64) — печатает целое + \n
        {
            auto* ft = llvm::FunctionType::get(voidTy, {i64}, false);
            auto* fn = llvm::Function::Create(
                ft, llvm::Function::InternalLinkage, "println_int", *module_);
            auto* bb = llvm::BasicBlock::Create(ctx_, "entry", fn);
            llvm::IRBuilder<> b(bb);
            auto* fmt = b.CreateGlobalStringPtr("%ld\n", "println_int_fmt");
            b.CreateCall(printf_fn, {fmt, fn->arg_begin()});
            b.CreateRetVoid();
        }

        // println_float(f64) — печатает float + \n
        {
            auto* ft = llvm::FunctionType::get(
                voidTy, {llvm::Type::getDoubleTy(ctx_)}, false);
            auto* fn = llvm::Function::Create(
                ft, llvm::Function::InternalLinkage, "println_float", *module_);
            auto* bb = llvm::BasicBlock::Create(ctx_, "entry", fn);
            llvm::IRBuilder<> b(bb);
            auto* fmt = b.CreateGlobalStringPtr("%f\n", "println_float_fmt");
            b.CreateCall(printf_fn, {fmt, fn->arg_begin()});
            b.CreateRetVoid();
        }

        // println_bool(i1) — печатает true/false + \n
        {
            auto* ft = llvm::FunctionType::get(
                voidTy, {llvm::Type::getInt1Ty(ctx_)}, false);
            auto* fn = llvm::Function::Create(
                ft, llvm::Function::InternalLinkage, "println_bool", *module_);
            auto* bb = llvm::BasicBlock::Create(ctx_, "entry", fn);
            llvm::IRBuilder<> b(bb);
            // Выбираем строку "true"/"false" через select
            auto* true_str  = b.CreateGlobalStringPtr("true\n");
            auto* false_str = b.CreateGlobalStringPtr("false\n");
            auto* str = b.CreateSelect(fn->arg_begin(), true_str, false_str);
            b.CreateCall(printf_fn, {b.CreateGlobalStringPtr("%s"), str});
            b.CreateRetVoid();
        }
    }
}

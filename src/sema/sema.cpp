#include "flux/sema/sema.h"

#include <string>
#include <unordered_set>

namespace flux {
    SemanticAnalyzer::SemanticAnalyzer(DiagEngine& diag) : diag_(diag) {}

    void SemanticAnalyzer::analyze(ProgramNode& program) {
        program.accept(*this);
    }

    void SemanticAnalyzer::push_scope() {
        scopes_.emplace_back();
    }

    void SemanticAnalyzer::pop_scope() {
        scopes_.pop_back();
    }

    void SemanticAnalyzer::declare_var(const std::string& name, const std::string& type, const SourceLocation& loc) {
        auto& scope = scopes_.back();
        if (scope.count(name)) {
            diag_.emit(DiagLevel::Warning, loc,
                "Variable '" + name + "' shadows previous declaration");
        }
        scope[name] = type;
    }

    std::string SemanticAnalyzer::lookup_var(const std::string& name) const { // Поиск от внутреннего scope к внешнему
        for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) return found->second;
        }
        return ""; // не найдено
    }
    
    std::string SemanticAnalyzer::type_node_to_string(TypeNode* t) {
        if (!t) return "unknown";

        if (auto* pt = dynamic_cast<PrimitiveType*>(t))
            return pt->name;

        if (auto* r = dynamic_cast<RefType*>(t))
            return "&" + type_node_to_string(r->inner.get());

        if (auto* a = dynamic_cast<ArrayType*>(t))
            return type_node_to_string(a->element.get()) + "[]";

        if (auto* g = dynamic_cast<GenericType*>(t)) {
            std::string s = g->name + "<";
            for (size_t i = 0; i < g->args.size(); ++i) {
                if (i) s += ", ";
                s += type_node_to_string(g->args[i].get());
            }
            return s + ">";
        }

        if (dynamic_cast<UnitType*>(t))
            return "()";

        return "unknown";
    }

    void SemanticAnalyzer::register_func(FuncDecl& decl, const std::string& owner) {
        FuncSignature sig;
        sig.name        = decl.name;
        sig.return_type = type_node_to_string(decl.return_type.get());
        sig.decl        = &decl;
        sig.is_pub      = decl.is_pub;
        sig.owner       = owner;

        // for (auto& param : decl.params)
        //     sig.param_types.push_back(type_node_to_string(param->type.get()));

        for (auto& param : decl.params) {
            std::string ptype;
            if (param->name == "self" && !param->type) {
                ptype = owner;  // self = Point!
            } else if (param->type) {
                ptype = type_node_to_string(param->type.get());
            } else {
                ptype = "unknown";
            }
            sig.param_types.push_back(ptype);
        }

        func_table_[decl.name].push_back(std::move(sig));
    }

    // Выбирает подходящую перегрузку по имени и типам аргументов.
    // Сейчас — точное совпадение количества и типов.
    const FuncSignature* SemanticAnalyzer::resolve_overload(const std::string& name, const std::vector<std::string>& arg_types,const SourceLocation& loc) {
        auto it = func_table_.find(name);
        if (it == func_table_.end()) {
            diag_.emit(DiagLevel::Error, loc,
                "Undefined function '" + name + "'");
            return nullptr;
        }

        const OverloadSet& overloads = it->second;

        // Если типы аргументов неизвестны — возвращаем первую перегрузку
        // (полный вывод типов будет в следующей фазе)
        if (arg_types.empty()) {
            return &overloads[0];
        }

        // Точное совпадение по количеству и типам параметров
        for (const auto& sig : overloads) {
            if (sig.param_types.size() != arg_types.size()) continue;
            bool match = true;
            for (size_t i = 0; i < arg_types.size(); ++i) {
                if (!is_assign_compatible(arg_types[i], sig.param_types[i])) {
                    match = false;
                    break;
                }
            }
            if (match) return &sig;
        }

        // Совпадения нет — выводим ошибку с подсказкой какие перегрузки есть
        std::string msg = "No matching overload for '" + name + "' with args (";
        for (size_t i = 0; i < arg_types.size(); ++i) {
            if (i) msg += ", ";
            msg += arg_types[i];
        }
        msg += "). Available overloads:\n";
        for (const auto& sig : overloads) {
            msg += "  " + sig.name + "(";
            for (size_t i = 0; i < sig.param_types.size(); ++i) {
                if (i) msg += ", ";
                msg += sig.param_types[i];
            }
            msg += ") -> " + sig.return_type + "\n";
        }
        diag_.emit(DiagLevel::Error, loc, msg);
        return nullptr;
    }

    void SemanticAnalyzer::check_main_entry_point() {
        auto it = func_table_.find("main");

        // main обязан существовать
        if (it == func_table_.end()) {
            diag_.emit(DiagLevel::Error, {}, "No entry point: 'main' function not found");
            return;
        }

        const OverloadSet& overloads = it->second;

        // main не может быть перегружен
        if (overloads.size() > 1) {
            diag_.emit(DiagLevel::Error, overloads[1].decl->loc,
                "'main' cannot be overloaded — only one entry point allowed");
        }

        const FuncSignature& sig = overloads[0];
        const FuncDecl& decl     = *sig.decl;

        // Допустимые сигнатуры main:
        //   fnc main() -> int32
        //   fnc main() -> Result<int32, string>
        //   fnc main(argc: int32, argv: &&str) -> int32         // аргументы командной строки
        //   fnc main(argc: int32, argv: &&str) -> Result<int32, string>

        bool valid_return =
            sig.return_type == "int32" ||
            sig.return_type == "Result<int32, string>";

        if (!valid_return) {
            diag_.emit(DiagLevel::Error, decl.loc,
                "'main' must return 'int32' or 'Result<int32, string>', got '"
                + sig.return_type + "'");
        }

        size_t nparams = sig.param_types.size();

        if (nparams == 0) {
            // fnc main() — без аргументов, OK
            return;
        }

        if (nparams == 2) {
            // fnc main(argc: int32, argv: &&str) — аргументы командной строки
            bool argc_ok = sig.param_types[0] == "int32";
            bool argv_ok = sig.param_types[1] == "&&str" ||
                        sig.param_types[1] == "&[]&str"; // разные формы записи

            if (!argc_ok || !argv_ok) {
                diag_.emit(DiagLevel::Error, decl.loc,
                    "'main' with arguments must have signature: "
                    "fnc main(argc: int32, argv: &&str) -> int32");
            }
            return;
        }

        // Любое другое количество параметров — ошибка
        diag_.emit(DiagLevel::Error, decl.loc,
            "'main' must have 0 or 2 parameters (argc: int32, argv: &&str)");
    }

    // ── Declarations ─────────────────────────────────────────
    void SemanticAnalyzer::visit(ProgramNode& node) {
        // Первый проход: регистрируем все функции
        // Нужно чтобы функции могли вызывать друг друга до объявления
        for (auto& decl : node.decls) {
            if (auto* fn = dynamic_cast<FuncDecl*>(decl.get()))
                register_func(*fn, "");
            if (auto* impl = dynamic_cast<ImplDecl*>(decl.get()))
                for (auto& method : impl->methods)
                    register_func(*method, impl->target);
            if (auto* cls = dynamic_cast<ClassDecl*>(decl.get()))
                for (auto& method : cls->methods)
                    register_func(*method, cls->name);
        }

        // Второй проход: полный анализ каждой ноды
        for (auto& decl : node.decls)
            if (decl) decl->accept(*this);

        // После регистрации всех функций — проверяем main
        check_main_entry_point();
    }

    void SemanticAnalyzer::visit(FuncDecl& node) {
        auto* prev_func = current_func_;
        current_func_   = &node;

        push_scope();

        for (auto& param : node.params) {
            std::string param_type;
            if (param->name == "self" && !param->type) {
                param_type = current_impl_type_;  // Point.max → self: Point
            } else {
                param_type = type_node_to_string(param->type.get());
            }
            declare_var(param->name, param_type, param->loc);
        }

        if (node.body) node.body->accept(*this);

        pop_scope();
        current_func_ = prev_func;
    }

    void SemanticAnalyzer::visit(VarDecl& node) {
        std::string decl_type  = type_node_to_string(node.type.get());
        std::string init_type  = "unknown";

        if (node.init) {
            init_type = eval_type(*node.init);
        }

        //* Вывод типа: let x = expr — тип берём из выражения
        if (decl_type == "unknown" && init_type != "unknown") {
            decl_type = init_type;
        }

        //* Проверка совместимости: let x: int32 = 5.0 — ошибка
        if (node.type && init_type != "unknown" && init_type != "inferred") {
            if (!is_assign_compatible(init_type, decl_type)) {
                diag_.emit(DiagLevel::Error, node.loc,
                    "Cannot assign '" + init_type + "' to variable of type '"
                    + decl_type + "'");
            }
        }

        //* let x; без типа и без инициализатора — предупреждение
        if (decl_type == "unknown" && !node.init) {
            diag_.emit(DiagLevel::Warning, node.loc,
                "Variable '" + node.name + "' declared without type or initializer");
            decl_type = "unknown";
        }

        declare_var(node.name, decl_type, node.loc);
        last_type_ = decl_type;
    }


    void SemanticAnalyzer::visit(StructDecl& node) {
        // Проверяем дублирование имени типа
        if (type_table_.count(node.name)) {
            diag_.emit(DiagLevel::Error, node.loc,
                "Type '" + node.name + "' is already defined");
            return;
        }

        TypeInfo info;
        info.name     = node.name;
        info.is_class = false;

        std::unordered_set<std::string> seen_fields;

        for (auto& field : node.fields) {
            // Дублирование полей
            if (seen_fields.count(field->name)) {
                diag_.emit(DiagLevel::Error, field->loc,
                    "Field '" + field->name + "' is already declared in struct '"
                    + node.name + "'");
                continue;
            }
            seen_fields.insert(field->name);

            // Проверяем что тип поля существует
            std::string ftype = type_node_to_string(field->type.get());
            if (!is_known_type(ftype)) {
                diag_.emit(DiagLevel::Error, field->loc,
                    "Unknown type '" + ftype + "' for field '"
                    + field->name + "'");
            }

            info.fields.push_back({ field->name, ftype, field->is_pub });
        }

        type_table_[node.name] = std::move(info);
    }

    void SemanticAnalyzer::visit(ClassDecl& node) {
        // Проверяем дублирование имени типа
        if (type_table_.count(node.name)) {
            diag_.emit(DiagLevel::Error, node.loc,
                "Type '" + node.name + "' is already defined");
            return;
        }

        TypeInfo info;
        info.name     = node.name;
        info.is_class = true;

        std::unordered_set<std::string> seen_fields;
        std::unordered_set<std::string> seen_methods;

        for (auto& field : node.fields) {
            if (seen_fields.count(field->name)) {
                diag_.emit(DiagLevel::Error, field->loc,
                    "Field '" + field->name + "' is already declared in class '"
                    + node.name + "'");
                continue;
            }
            seen_fields.insert(field->name);

            std::string ftype = type_node_to_string(field->type.get());
            if (!is_known_type(ftype)) {
                diag_.emit(DiagLevel::Error, field->loc,
                    "Unknown type '" + ftype + "' for field '" + field->name + "'");
            }

            info.fields.push_back({ field->name, ftype, field->is_pub });
        }

        type_table_[node.name] = std::move(info);

        auto prev = current_impl_type_;
        current_impl_type_ = node.name;

        // Анализируем методы — они уже зарегистрированы в func_table_ из первого прохода
        for (auto& method : node.methods) {
            // Проверяем дублирование методов с одинаковой сигнатурой
            if (seen_methods.count(method->name)) {
                // Перегрузка допустима — проверяем что сигнатуры различаются
                // Это уже обеспечивается register_func + resolve_overload
            }
            seen_methods.insert(method->name);
            method->accept(*this);
        }

        current_impl_type_ = prev;
    }

    void SemanticAnalyzer::visit(ImplDecl& node) {
        // Проверяем что тип для которого пишем impl существует
        if (!type_table_.count(node.target)) {
            diag_.emit(DiagLevel::Error, node.loc,
                "impl for unknown type '" + node.target + "'");
            return;
        }

        // Проверяем что impl не дублирует уже существующие методы в классе
        // (актуально если кто-то напишет impl для class — это запрещено)
        if (type_table_[node.target].is_class) {
            diag_.emit(DiagLevel::Error, node.loc,
                "Cannot use impl for class '" + node.target
                + "' — methods are declared inside class body");
            return;
        }

        auto prev = current_impl_type_;
        current_impl_type_ = node.target;

        // Анализируем каждый метод
        for (auto& method : node.methods)
            method->accept(*this);

        current_impl_type_ = prev;
    }

    // ── Statements ───────────────────────────────────────────

    void SemanticAnalyzer::visit(BlockStmt& node) {
        push_scope();
        for (auto& stmt : node.stmts)
            if (stmt) stmt->accept(*this);
        pop_scope();
    }

    void SemanticAnalyzer::visit(ReturnStmt& node) {
        if (!current_func_) return;

        std::string expected = type_node_to_string(current_func_->return_type.get());
        std::string actual = node.value ? eval_type(*node.value) : "()";

        if (!is_assign_compatible(actual, expected)) {
            diag_.emit(DiagLevel::Error, node.loc, 
                "Return type mismatch: expected '" + expected + "', got '" + actual + "'");
        }
        last_type_ = actual;
    }

    void SemanticAnalyzer::visit(ExprStmt& node) {
        if (node.expr) node.expr->accept(*this);
    }

    void SemanticAnalyzer::visit(IfStmt& node) {
        std::string cond_type = eval_type(*node.cond);
        if (cond_type != "bool" && cond_type != "unknown") {
            diag_.emit(DiagLevel::Error, node.loc,
                "If condition must be bool, got '" + cond_type + "'");
        }
        if (node.then_block)  node.then_block->accept(*this);
        if (node.else_branch) node.else_branch->accept(*this);
    }

    void SemanticAnalyzer::visit(ForStmt& node) {
        push_scope();
        if (node.init) node.init->accept(*this);

        if (node.cond) {
            std::string cond_type = eval_type(*node.cond);
            if (cond_type != "bool" && cond_type != "unknown")
                diag_.emit(DiagLevel::Error, node.loc,
                    "For condition must be bool, got '" + cond_type + "'");
        }
        if (node.step) eval_type(*node.step); // разбираем шаг — тип не важен
        if (node.body) node.body->accept(*this);
        pop_scope();
    }

    void SemanticAnalyzer::visit(WhileStmt& node) {
        std::string cond_type = eval_type(*node.cond);
        if (cond_type != "bool" && cond_type != "unknown")
            diag_.emit(DiagLevel::Error, node.loc,
                "While condition must be bool, got '" + cond_type + "'");
        if (node.body) node.body->accept(*this);
    }

    // ── Expressions ──────────────────────────────────────────
    void SemanticAnalyzer::visit(IdentExpr& node) {
        if (node.name == "self" && current_func_ && !current_impl_type_.empty()) {
            last_type_ = current_impl_type_;
            return;
        }

        std::string t = lookup_var(node.name);
        if (!t.empty()) {
            last_type_ = t;
            return;
        }

        if (is_builtin(node.name)) {
            last_type_ = "builtin";
            return;
        }

        diag_.emit(DiagLevel::Error, node.loc, "Undefined '" + node.name + "'");
        last_type_ = "unknown";
    }


    void SemanticAnalyzer::visit(BinaryExpr& node) {
        std::string lhs_type = eval_type(*node.lhs);
        std::string rhs_type = eval_type(*node.rhs);

        //* Операторы сравнения всегда возвращают bool
        static const std::unordered_set<std::string> cmp_ops = {
            "==", "!=", "<", ">", "<=", ">="
        };
        if (cmp_ops.count(node.op)) {
            // Операнды должны быть совместимы
            if (!is_implicitly_convertible(lhs_type, rhs_type) &&
                !is_implicitly_convertible(rhs_type, lhs_type))
            {
                diag_.emit(DiagLevel::Error, node.loc,
                    "Cannot compare '" + lhs_type + "' with '" + rhs_type + "'");
            }
            last_type_ = "bool";
            return;
        }

        //* Логические операторы: && ||
        if (node.op == "&&" || node.op == "||") {
            if (lhs_type != "bool")
                diag_.emit(DiagLevel::Error, node.loc,
                    "Left operand of '" + node.op + "' must be bool, got '" + lhs_type + "'");
            if (rhs_type != "bool")
                diag_.emit(DiagLevel::Error, node.loc,
                    "Right operand of '" + node.op + "' must be bool, got '" + rhs_type + "'");
            last_type_ = "bool";
            return;
        }

        //* Арифметические операторы: + - * / %
        if (node.op == "+" || node.op == "-" ||
            node.op == "*" || node.op == "/" || node.op == "%")
        {
            if (!is_numeric(lhs_type))
                diag_.emit(DiagLevel::Error, node.loc,
                    "Left operand of '" + node.op + "' is not numeric: '" + lhs_type + "'");
            if (!is_numeric(rhs_type))
                diag_.emit(DiagLevel::Error, node.loc,
                    "Right operand of '" + node.op + "' is not numeric: '" + rhs_type + "'");
            // Результат — более широкий тип из двух
            last_type_ = wider_type(lhs_type, rhs_type);
            return;
        }

        //* Побитовые операторы: & | ^ << >>
        if (node.op == "&"  || node.op == "|"  || node.op == "^" ||
            node.op == "<<" || node.op == ">>")
        {
            if (!is_numeric(lhs_type) || !is_numeric(rhs_type))
                diag_.emit(DiagLevel::Error, node.loc,
                    "Bitwise operator '" + node.op + "' requires numeric operands");
            last_type_ = wider_type(lhs_type, rhs_type);
            return;
        }

        last_type_ = "unknown";
    }

    void SemanticAnalyzer::visit(UnaryExpr& node) {
        std::string t = eval_type(*node.operand);
        switch (node.op) {
            case UnaryExpr::Op::Neg:
                if (!is_numeric(t))
                    diag_.emit(DiagLevel::Error, node.loc,
                        "Unary '-' requires numeric operand, got '" + t + "'");
                last_type_ = t;
                break;

            case UnaryExpr::Op::Not:
                if (t != "bool")
                    diag_.emit(DiagLevel::Error, node.loc,
                        "Unary '!' requires bool, got '" + t + "'");
                last_type_ = "bool";
                break;

            case UnaryExpr::Op::BitNot:
                if (!is_numeric(t))
                    diag_.emit(DiagLevel::Error, node.loc,
                        "Unary '~' requires numeric operand, got '" + t + "'");
                last_type_ = t;
                break;

            case UnaryExpr::Op::PreInc:
            case UnaryExpr::Op::PreDec:
            case UnaryExpr::Op::PostInc:
            case UnaryExpr::Op::PostDec:
                if (!is_numeric(t))
                    diag_.emit(DiagLevel::Error, node.loc,
                        "Increment/decrement requires numeric operand, got '" + t + "'");
                last_type_ = t;
                break;

            case UnaryExpr::Op::AddrOf:
                //* &x → тип &T
                last_type_ = "&" + t;
                break;

            case UnaryExpr::Op::Deref:
                // *x → снимаем & с типа
                if (t.empty() || t[0] != '&') {
                    diag_.emit(DiagLevel::Error, node.loc,
                        "Cannot dereference non-reference type '" + t + "'");
                    last_type_ = "unknown";
                } else {
                    last_type_ = t.substr(1); // убираем ведущий &
                }
                break;
        }
    }

    void SemanticAnalyzer::visit(AssignExpr& node) {
        std::string target_type = eval_type(*node.target);
        std::string value_type  = eval_type(*node.value);

        if (target_type != "unknown" && value_type != "unknown") {
            //* Составное присваивание: += -= *=  — операнды должны быть числами
            if (node.op != "=") {
                if (!is_numeric(target_type))
                    diag_.emit(DiagLevel::Error, node.loc,
                        "Compound assignment '" + node.op +
                        "' requires numeric target, got '" + target_type + "'");
            }

            //* Проверка совместимости типов
            if (!is_assign_compatible(value_type, target_type)) {
                diag_.emit(DiagLevel::Error, node.loc,
                    "Cannot assign '" + value_type + "' to '" + target_type + "'");
            }
        }
        last_type_ = target_type;
    }

    void SemanticAnalyzer::visit(CallExpr& node) {
        std::vector<std::string> arg_types;
        for (auto& arg : node.args) {
            arg_types.push_back(eval_type(*arg));
        }
        size_t dot_pos = node.callee.find('.');
        if (dot_pos != std::string::npos) {
            std::string type_name = node.callee.substr(0, dot_pos);
            std::string method_name = node.callee.substr(dot_pos + 1);

            auto it = func_table_.find(method_name);
            if (it != func_table_.end()) {
                for (const auto& sig : it->second) {
                    if (sig.owner == type_name) {
                        if (!sig.is_pub) {
                            diag_.emit(DiagLevel::Error, node.loc,
                                "Static method '" + method_name + "' of '" + type_name + "' is private");
                        }

                        // Проверяем аргументы
                        if (sig.param_types.size() == arg_types.size()) {
                            bool match = true;

                            for (size_t i = 0; i < arg_types.size(); ++i) {
                                if (!is_assign_compatible(arg_types[i], sig.param_types[i])) {
                                    match = false;
                                    break;
                                }
                            }
                            if (match) {
                                last_type_ = sig.return_type;
                                return; 

                            }
                        }
                    }
                }
            }

            diag_.emit(DiagLevel::Error, node.loc,
                "No matching static method '" + method_name + "' for type '" + type_name + "'");
            last_type_ = "unknown";
            return;
        }

        if (is_builtin(node.callee)) {
            last_type_ = "()";
            return;
        }

        if (dot_pos != std::string::npos) {
            std::string type_name = node.callee.substr(0, dot_pos);
            std::string method_name = node.callee.substr(dot_pos + 1);
            
            // Ищем метод с owner == type_name
            auto it = func_table_.find(method_name);
            if (it != func_table_.end()) {
                for (const auto& sig : it->second) {
                    if (sig.owner == type_name) {
                        // Проверяем видимость
                        if (!sig.is_pub && current_impl_type_ != type_name) {
                            diag_.emit(DiagLevel::Error, node.loc,
                                "Static method '" + method_name + "' of '" + type_name + 
                                "' is private");
                        }
                        
                        // Проверяем перегрузку по аргументам
                        if (sig.param_types.size() == arg_types.size()) {
                            bool match = true;
                            for (size_t i = 0; i < arg_types.size(); ++i) {
                                if (!is_assign_compatible(arg_types[i], sig.param_types[i])) {
                                    match = false;
                                    break;
                                }
                            }
                            if (match) {
                                last_type_ = sig.return_type;
                                return; // ← НАШЛИ!
                            }
                        }
                    }
                }
            }
            
            diag_.emit(DiagLevel::Error, node.loc,
                "No matching static method '" + method_name + "' for type '" + type_name + "'");
            last_type_ = "unknown";
            return;
        }

        // Конструкторы Result
        if (node.callee == "Ok") {
            std::string inner = arg_types.empty() ? "unknown" : arg_types[0];
            last_type_ = "Result<" + inner + ", _>";
            return;
        }
        if (node.callee == "Err") {
            std::string inner = arg_types.empty() ? "unknown" : arg_types[0];
            last_type_ = "Result<_, " + inner + ">";
            return;
        }

        // Обычная функция — разрешаем перегрузку по типам аргументов
        const FuncSignature* sig = resolve_overload(node.callee, arg_types, node.loc);
        if (sig) {
            if (!sig->owner.empty()) {
                // Метод impl — нельзя вызывать как обычную функцию
                if (current_impl_type_ != sig->owner) {
                    // Снаружи impl — только через оператор точки!
                    diag_.emit(DiagLevel::Error, node.loc,
                        "Method '" + node.callee + "' not found ");
                    last_type_ = "unknown";
                    return;
                }
            }
            last_type_ = sig->return_type;
        } else {
            last_type_ = "unknown";
        }
        last_type_ = sig ? sig->return_type : "unknown";
    }

    void SemanticAnalyzer::visit(MethodCallExpr& node) {
        std::string receiver_type;
        std::vector<std::string> arg_types;

        if (node.receiver) receiver_type = eval_type(*node.receiver);
        for (auto& arg : node.args) if (arg) arg_types.push_back(eval_type(*arg));

        auto it = func_table_.find(node.method);
        if (it != func_table_.end()) {
            for (const auto& sig : it->second) {
                if (sig.owner == receiver_type) {
                    // full_args = [receiver] + args
                    std::vector<std::string> full_args{receiver_type};
                    full_args.insert(full_args.end(), arg_types.begin(), arg_types.end());
                    // Совпадение сигнатуры
                    if (sig.param_types.size() == full_args.size()) {
                        bool match = true;
                        for (size_t i = 0; i < full_args.size(); ++i) {
                            bool compat = is_assign_compatible(full_args[i], sig.param_types[i]);
                            if (!is_assign_compatible(full_args[i], sig.param_types[i])) {
                                match = false;
                                break;
                            }
                        }
                        if (match) {
                            if (!sig.is_pub && current_impl_type_ != sig.owner) {
                                diag_.emit(DiagLevel::Error, node.loc,
                                    "Method '" + node.method + "' of '" + sig.owner + "' is private");
                                last_type_ = "unknown";
                                return;
                            }
                            last_type_ = sig.return_type;
                            return;
                        }
                        
                    }
                }
            }
        }

        diag_.emit(DiagLevel::Error, node.loc, "No method '" + node.method + "' for '" + receiver_type + "'");
        last_type_ = "unknown";

        if (node.method == "to_string") { last_type_ = "string";  return; }
        if (node.method == "len")       { last_type_ = "usize_t"; return; }
        if (node.method == "push")      { last_type_ = "()";      return; }
        if (node.method == "pop")       { last_type_ = "()";      return; }
        if (node.method == "is_empty")  { last_type_ = "bool";    return; }
        if (node.method == "contains")  { last_type_ = "bool";    return; }

        last_type_ = "unknown";
    }

    void SemanticAnalyzer::visit(IndexExpr& node) {
        std::string arr_type   = eval_type(*node.array);
        std::string index_type = eval_type(*node.index);

        // Индекс должен быть целым числом
        if (!is_numeric(index_type) || index_type == "float" || index_type == "double") {
            diag_.emit(DiagLevel::Error, node.loc,
                "Array index must be integer, got '" + index_type + "'");
        }

        // Тип элемента: int32[] → int32
        if (arr_type.size() > 2 && arr_type.substr(arr_type.size() - 2) == "[]") {
            last_type_ = arr_type.substr(0, arr_type.size() - 2);
        } else {
            diag_.emit(DiagLevel::Error, node.loc,
                "Cannot index non-array type '" + arr_type + "'");
            last_type_ = "unknown";
        }
    }


    void SemanticAnalyzer::visit(ArrayLiteral& node) {
        if (node.elements.empty()) {
            last_type_ = "unknown[]";
            return;
        }

        std::string elem_type = eval_type(*node.elements[0]);

        for (size_t i = 1; i < node.elements.size(); ++i) {
            std::string t = eval_type(*node.elements[i]);
            if (!is_implicitly_convertible(t, elem_type)) {
                // Пробуем в другую сторону — может elem_type надо расширить
                if (is_implicitly_convertible(elem_type, t)) {
                    elem_type = t; // расширяем тип массива
                } else {
                    diag_.emit(DiagLevel::Error, node.loc,
                        "Inconsistent array element types: '" +
                        elem_type + "' and '" + t + "'");
                }
            }
        }
        last_type_ = elem_type + "[]";
    }


    void SemanticAnalyzer::visit(ImplicitCastExpr& node) {
        if (node.inner) eval_type(*node.inner); // проверяем inner
        //* тип всего выражения — целевой тип каста
        last_type_ = type_node_to_string(node.target_type.get());
    }

    void SemanticAnalyzer::visit(MatchExpr& node) {
        std::string subject_type = eval_type(*node.subject);

        bool has_wildcard = false;
        for (auto& arm : node.arms) {
            if (dynamic_cast<WildcardPattern*>(arm->pattern.get()))
                has_wildcard = true;
            arm->accept(*this);
        }

        //* Если нет wildcard — предупреждение что match может быть неполным
        //* Полная проверка exhaustiveness требует знания всех вариантов типа
        if (!has_wildcard) {
            diag_.emit(DiagLevel::Warning, node.loc,
                "Match may not be exhaustive: no wildcard '_' arm");
        }

        last_type_ = "()";
    }

    void SemanticAnalyzer::visit(MatchArm& node) {
        push_scope();
        // Переменные из паттерна видны в теле arm
        // IdentPattern → объявляем переменную
        if (auto* ip = dynamic_cast<IdentPattern*>(node.pattern.get()))
            declare_var(ip->name, "inferred", node.loc);
        if (auto* cp = dynamic_cast<ConstructorPattern*>(node.pattern.get()))
            for (auto& arg : cp->args)
                if (auto* ap = dynamic_cast<IdentPattern*>(arg.get()))
                    declare_var(ap->name, "inferred", node.loc);
        if (node.body) node.body->accept(*this);
        pop_scope();
    }

    void SemanticAnalyzer::visit(StructInitExpr& node) {
        auto it = type_table_.find(node.type_name);
        if (it == type_table_.end()) {
            diag_.emit(DiagLevel::Error, node.loc,
                "Unknown type '" + node.type_name + "'");
            last_type_ = "unknown";
            return;
        }

        const TypeInfo& info = it->second;

        for (auto& finit : node.fields) {
            std::string val_type = eval_type(*finit.value);

            // Ищем поле в TypeInfo
            const FieldInfo* fi = nullptr;
            for (const auto& f : info.fields)
                if (f.name == finit.name) { fi = &f; break; }

            if (!fi) {
                diag_.emit(DiagLevel::Error, node.loc,
                    "No field '" + finit.name + "' in type '" + node.type_name + "'");
                continue;
            }

            if (!is_assign_compatible(val_type, fi->type)) {
                diag_.emit(DiagLevel::Error, node.loc,
                    "Field '" + finit.name + "' expects '" + fi->type +
                    "', got '" + val_type + "'");
            }
        }

        last_type_ = node.type_name;
    }

    void SemanticAnalyzer::visit(FieldAccessExpr& node) {
        std::string obj_type;
        if (node.object) obj_type = eval_type(*node.object);
        auto it = type_table_.find(obj_type);
        if (it == type_table_.end()) {
            // Тип неизвестен — пропускаем проверку поля
            last_type_ = "unknown";
            return;
        }

        for (const auto& field : it->second.fields) {
            if (field.name == node.field) {
                last_type_ = field.type;
                return;
            }
        }

        diag_.emit(DiagLevel::Error, node.loc,
            "No field '" + node.field + "' in type '" + obj_type + "'");
        last_type_ = "unknown";
    }

    void SemanticAnalyzer::visit(SelfExpr& node) {
        if (current_impl_type_.empty()) {
            diag_.emit(DiagLevel::Error, node.loc, "'self' outside impl");
            last_type_ = "unknown";
        } else {
            last_type_ = current_impl_type_;
        }
    }

    bool SemanticAnalyzer::is_known_type(const std::string& name) const {
        // Примитивные типы
        static const std::unordered_set<std::string> primitives = {
            "bool",
            "int8", "int16", "int32", "int64", "int128",
            "uint8", "uint16", "uint32", "uint64", "uint128",
            "float", "double",
            "isize_t", "usize_t",
            "str", "string",
            "()"
        };

        // Примитив
        if (primitives.count(name)) return true;

        // Ссылка: &int32, &str
        if (name.size() > 1 && name[0] == '&')
            return is_known_type(name.substr(1));

        // Generic: Result<...>, Option<...>
        auto lt = name.find('<');
        if (lt != std::string::npos)
            return true; // Генерики проверяются отдельно в type inference

        // Пользовательский тип: struct или class
        if (type_table_.count(name)) return true;

        return false;
    }

    bool SemanticAnalyzer::is_builtin(const std::string& name) const {
        static const std::unordered_set<std::string> builtins = {
            "println", "print", "eprintln", "eprint"
        };
        return builtins.count(name) > 0;
    }

    std::string SemanticAnalyzer::eval_type(ASTNode& node) {
        last_type_ = "unknown";
        node.accept(*this);
        std::string result = last_type_;
        return last_type_;
    }

    // Ранг числового типа — чем выше, тем шире тип
    int SemanticAnalyzer::numeric_rank(const std::string& t) {
        if (t == "int8")    return 1;
        if (t == "uint8")   return 2;
        if (t == "int16")   return 3;
        if (t == "uint16")  return 4;
        if (t == "int32")   return 5;
        if (t == "uint32")  return 6;
        if (t == "int64")   return 7;
        if (t == "uint64")  return 8;
        if (t == "int128")  return 9;
        if (t == "uint128") return 10;
        if (t == "isize_t") return 7;
        if (t == "usize_t") return 8;
        if (t == "float")   return 11;
        if (t == "double")  return 12;
        return -1;
    }

    bool SemanticAnalyzer::is_numeric(const std::string& t) {
        return numeric_rank(t) != -1;
    }

    // Возвращает более широкий из двух числовых типов
    std::string SemanticAnalyzer::wider_type(const std::string& a, const std::string& b) {
        return numeric_rank(a) >= numeric_rank(b) ? a : b;
    }

    // Можно ли неявно привести from -> to
    bool SemanticAnalyzer::is_implicitly_convertible(const std::string& from, const std::string& to) {
        if (from == to) return true;

        // числовое расширение: меньший тип -> больший
        if (is_numeric(from) && is_numeric(to))
            return numeric_rank(from) <= numeric_rank(to);

        if (from.find("Result<") == 0 && to.find("Result<") == 0) {
                if (from.find('_') != std::string::npos || to.find('_')   != std::string::npos)
                    return true;
        }
        
        return false;
    }

    // bool SemanticAnalyzer::is_result_compatible(const std::string& from, const std::string& to) const {
    //     // Result<int32, _> совместим с Result<int32, string>
    //     // _ — wildcard тип из Ok/Err конструкторов
    //     if (from.find("Result<") == 0 && to.find("Result<") == 0) {
    //         // Если один из типов содержит _ — считаем совместимым
    //         if (from.find('_') != std::string::npos ||
    //             to.find('_')   != std::string::npos)
    //             return true;
    //     }
    //     return false;
    // }

    bool SemanticAnalyzer::is_assign_compatible(const std::string& from, const std::string& to) {
        if (from == to) return true;

        // Любое числовое преобразование разрешено
        if (is_numeric(from) && is_numeric(to))
            return true;

        // Result с wildcard совместимостью
        if (from.find("Result<") == 0 && to.find("Result<") == 0) {
            if (from.find('_') != std::string::npos ||
                to.find('_')   != std::string::npos)
                return true;
        }

        return false;
    }
}
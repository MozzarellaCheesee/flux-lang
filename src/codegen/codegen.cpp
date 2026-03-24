#include "flux/codegen/codegen.h"
#include "flux/ast/type.h"
#include <unordered_map>
#include <unordered_set>

namespace flux {

    std::string CodeGen::sanitize_name(const std::string& name) {
        // Зарезервированные слова C++
        static const std::unordered_set<std::string> reserved = {
            "new", "delete", "class", "template", "operator",
            "namespace", "try", "catch", "throw", "this"
        };
        if (reserved.count(name)) return "flux_" + name;
        return name;
    }


    // ─── Entrypoint ──────────────────────────────────────────────────────────────

    std::string CodeGen::generate(ProgramNode& program) {
        // ── flux_runtime: встраивается в каждую скомпилированную программу ──
        out_ << R"(
// ============================================================
//  flux_runtime — автоматически вставляется компилятором Flux
// ============================================================
#include <cstdint>
#include <cstddef>
#include <cmath>
#include <cstring>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <variant>
#include <optional>
#include <stdexcept>

// ── Псевдонимы типов ─────────────────────────────────────────
using int8_t   = int8_t;
using int16_t  = int16_t;
using int32_t  = int32_t;
using int64_t  = int64_t;
using uint8_t  = uint8_t;
using uint16_t = uint16_t;
using uint32_t = uint32_t;
using uint64_t = uint64_t;
using usize_t  = size_t;
using isize_t  = ptrdiff_t;

// ── Result<T, E> ─────────────────────────────────────────────
template<typename T, typename E>
struct Result {
    std::variant<T, E> data;
    static Result Ok(T v)  { return Result{std::variant<T,E>(std::in_place_index<0>, std::move(v))}; }
    static Result Err(E e) { return Result{std::variant<T,E>(std::in_place_index<1>, std::move(e))}; }
    bool is_ok()  const { return data.index() == 0; }
    bool is_err() const { return data.index() == 1; }
    T&   unwrap()       { return std::get<0>(data); }
    E&   unwrap_err()   { return std::get<1>(data); }
    const T& unwrap() const { return std::get<0>(data); }
};

// ── println / print / eprintln / eprint ──────────────────────
template<typename... Args>
void println(Args&&... args) { (std::cout << ... << args); std::cout << '\n'; }
template<typename... Args>
void print(Args&&... args)   { (std::cout << ... << args); }
template<typename... Args>
void eprintln(Args&&... args){ (std::cerr << ... << args); std::cerr << '\n'; }
template<typename... Args>
void eprint(Args&&... args)  { (std::cerr << ... << args); }

// ── io runtime ───────────────────────────────────────────────
inline std::string read_line() {
    std::string s;
    std::getline(std::cin, s);
    return s;
}
inline int32_t read_int() {
    int32_t n = 0; std::cin >> n; std::cin.ignore();
    return n;
}
inline uint32_t read_uint() {
    uint32_t n = 0; std::cin >> n; std::cin.ignore();
    return n;
}
inline float read_float() {
    float n = 0.0f; std::cin >> n; std::cin.ignore();
    return n;
}
inline double read_double() {
    double n = 0.0; std::cin >> n; std::cin.ignore();
    return n;
}

// ── math runtime ─────────────────────────────────────────────
inline double flux_sqrt  (double x)              { return std::sqrt(x); }
inline double flux_cbrt  (double x)              { return std::cbrt(x); }
inline double flux_pow   (double b, double e)    { return std::pow(b, e); }
inline int32_t abs_i32   (int32_t x)             { return std::abs(x); }
inline int64_t abs_i64   (int64_t x)             { return std::abs(x); }
inline double  abs_f64   (double x)              { return std::fabs(x); }
inline double  flux_floor(double x)              { return std::floor(x); }
inline double  flux_ceil (double x)              { return std::ceil(x); }
inline double  flux_round(double x)              { return std::round(x); }
inline double  flux_trunc(double x)              { return std::trunc(x); }
inline double  flux_log  (double x)              { return std::log(x); }
inline double  flux_log2 (double x)              { return std::log2(x); }
inline double  flux_log10(double x)              { return std::log10(x); }
inline double  flux_exp  (double x)              { return std::exp(x); }
inline double  flux_sin  (double x)              { return std::sin(x); }
inline double  flux_cos  (double x)              { return std::cos(x); }
inline double  flux_tan  (double x)              { return std::tan(x); }
inline double  flux_asin (double x)              { return std::asin(x); }
inline double  flux_acos (double x)              { return std::acos(x); }
inline double  flux_atan (double x)              { return std::atan(x); }
inline double  flux_atan2(double y, double x)    { return std::atan2(y, x); }
inline int32_t min_i32(int32_t a, int32_t b)     { return a < b ? a : b; }
inline int32_t max_i32(int32_t a, int32_t b)     { return a > b ? a : b; }
inline int64_t min_i64(int64_t a, int64_t b)     { return a < b ? a : b; }
inline int64_t max_i64(int64_t a, int64_t b)     { return a > b ? a : b; }
inline double  min_f64(double a, double b)        { return a < b ? a : b; }
inline double  max_f64(double a, double b)        { return a > b ? a : b; }
inline int32_t clamp_i32(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
inline double  clamp_f64(double v, double lo, double hi)     { return v < lo ? lo : v > hi ? hi : v; }

// ── string runtime ───────────────────────────────────────────
inline int32_t str_len      (const std::string& s)  { return (int32_t)s.size(); }
inline bool    str_is_empty (const std::string& s)  { return s.empty(); }
inline bool    str_contains (const std::string& s, const std::string& sub) {
    return s.find(sub) != std::string::npos;
}
inline bool str_starts_with(const std::string& s, const std::string& p) {
    return s.size() >= p.size() && s.compare(0, p.size(), p) == 0;
}
inline bool str_ends_with(const std::string& s, const std::string& p) {
    return s.size() >= p.size() && s.compare(s.size() - p.size(), p.size(), p) == 0;
}
inline std::string str_to_upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::toupper(c); });
    return s;
}
inline std::string str_to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
    return s;
}
inline std::string str_repeat(const std::string& s, int32_t n) {
    std::string r; r.reserve(s.size() * (n > 0 ? n : 0));
    for (int32_t i = 0; i < n; ++i) r += s;
    return r;
}
inline std::string str_trim_start(std::string s) {
    auto it = std::find_if(s.begin(), s.end(), [](unsigned char c){ return !std::isspace(c); });
    s.erase(s.begin(), it);
    return s;
}
inline std::string str_trim_end(std::string s) {
    auto it = std::find_if(s.rbegin(), s.rend(), [](unsigned char c){ return !std::isspace(c); });
    s.erase(it.base(), s.end());
    return s;
}
inline std::string str_trim(std::string s) { return str_trim_end(str_trim_start(std::move(s))); }
inline std::string str_replace(std::string s, const std::string& from, const std::string& to) {
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
    return s;
}
inline std::string str_concat(const std::string& a, const std::string& b) { return a + b; }
inline int32_t     str_char_at(const std::string& s, int32_t i) {
    if (i < 0 || (size_t)i >= s.size()) return -1;
    return (unsigned char)s[i];
}
inline std::string to_string_i32 (int32_t n)  { return std::to_string(n); }
inline std::string to_string_i64 (int64_t n)  { return std::to_string(n); }
inline std::string to_string_f64 (double n)   { return std::to_string(n); }
inline std::string to_string_bool(bool b)      { return b ? "true" : "false"; }
inline int32_t     parse_int  (const std::string& s) { return std::stoi(s); }
inline double      parse_float(const std::string& s) { return std::stod(s); }

// ── using — math из <cmath> без std:: ────────────────────────
using std::sqrt; using std::cbrt;  using std::pow;
using std::floor; using std::ceil; using std::round; using std::trunc;
using std::log;   using std::log2; using std::log10; using std::exp;
using std::sin;   using std::cos;  using std::tan;
using std::asin;  using std::acos; using std::atan;  using std::atan2;
// ============================================================
//  Конец flux_runtime
// ============================================================

)";
        gen(program);
        return out_.str();
    }

    // ─── Types ───────────────────────────────────────────────────────────────────

    std::string CodeGen::map_type_str(const std::string& t) {
        if (t == "bool")    return "bool";
        if (t == "int8")    return "int8_t";
        if (t == "int16")   return "int16_t";
        if (t == "int32")   return "int32_t";
        if (t == "int64")   return "int64_t";
        if (t == "int128")  return "__int128";
        if (t == "uint8")   return "uint8_t";
        if (t == "uint16")  return "uint16_t";
        if (t == "uint32")  return "uint32_t";
        if (t == "uint64")  return "uint64_t";
        if (t == "uint128") return "unsigned __int128";
        if (t == "float")   return "float";
        if (t == "double")  return "double";
        if (t == "isize_t") return "ptrdiff_t";
        if (t == "usize_t") return "size_t";
        if (t == "str")     return "std::string_view";
        if (t == "&str")    return "std::string_view";
        if (t == "string")  return "std::string";
        if (t == "()")      return "void";
        return t; // пользовательский тип — оставляем как есть
    }

    std::string CodeGen::map_type(TypeNode* t) {
        if (!t) return "void";
        if (auto* pt = dynamic_cast<PrimitiveType*>(t))
            return map_type_str(pt->name);
        if (auto* rt = dynamic_cast<RefType*>(t)){
            auto* inner = dynamic_cast<PrimitiveType*>(rt->inner.get());
            if (inner && (inner->name == "str"))
                return "std::string_view";
            return map_type(rt->inner.get()) + "&";
        }
        if (auto* at = dynamic_cast<ArrayType*>(t))
            return "std::vector<" + map_type(at->element.get()) + ">";
        if (auto* st = dynamic_cast<SliceType*>(t))
            return "std::vector<" + map_type(st->element.get()) + ">";
        if (auto* gt = dynamic_cast<GenericType*>(t)) {
            // Result<T, E> → Result<T, E>
            // Option<T>    → std::optional<T>
            if (gt->name == "Option" && gt->args.size() == 1)
                return "std::optional<" + map_type(gt->args[0].get()) + ">";
            std::string s = gt->name + "<";
            for (size_t i = 0; i < gt->args.size(); i++) {
                if (i) s += ", ";
                s += map_type(gt->args[i].get());
            }
            return s + ">";
        }
        if (dynamic_cast<UnitType*>(t)) return "void";
        return "auto";
    }

    // ─── Declarations ─────────────────────────────────────────────────────────────

    void CodeGen::gen(ProgramNode& node) {
        // Форвард-декларации
        for (auto& decl : node.decls) {
            if (auto* s = dynamic_cast<StructDecl*>(decl.get()))
                emitln("struct " + s->name + ";");
            if (auto* c = dynamic_cast<ClassDecl*>(decl.get()))
                emitln("struct " + c->name + ";");
        }
        emit("\n");

        // Собираем impl-методы по owner
        std::unordered_map<std::string, std::vector<FuncDecl*>> impl_methods;
        for (auto& decl : node.decls)
            if (auto* i = dynamic_cast<ImplDecl*>(decl.get()))
                for (auto& m : i->methods)
                    impl_methods[i->target].push_back(m.get());

        // Генерируем struct + impl вместе
        for (auto& decl : node.decls) {
            if (auto* s = dynamic_cast<StructDecl*>(decl.get())) {
                emitln("struct " + s->name + " {");
                indent();
                for (auto& field : s->fields)
                    emitln(map_type(field->type.get()) + " " + field->name + ";");
                // ✅ Методы impl внутри struct
                for (auto* m : impl_methods[s->name])
                    gen(*m, s->name);
                dedent();
                emitln("};");
                emit("\n");
            }
            else if (auto* c = dynamic_cast<ClassDecl*>(decl.get())) gen(*c);
            else if (auto* f = dynamic_cast<FuncDecl*>(decl.get()))   gen(*f);
            // ImplDecl пропускаем — уже обработали выше
        }
    }


    void CodeGen::gen(StructDecl& node) {
        emitln("struct " + node.name + " {");
        indent();
        for (auto& field : node.fields)
            emitln(map_type(field->type.get()) + " " + field->name + ";");
        dedent();
        emitln("};");
        emit("\n");
    }

    void CodeGen::gen(ClassDecl& node) {
        emitln("struct " + node.name + " {");
        indent();

        // Поля
        for (auto& field : node.fields)
            emitln(map_type(field->type.get()) + " " + field->name + ";");

        emit("\n");

        // Методы
        for (auto& method : node.methods)
            gen(*method, node.name);

        dedent();
        emitln("};");
        emit("\n");
    }

    void CodeGen::gen(ImplDecl& node) {
        // Методы impl генерируются как методы структуры
        // Для этого нужны определения вне тела — используем Type::method синтаксис
        for (auto& method : node.methods)
            gen(*method, node.target);
        emit("\n");
    }

    void CodeGen::gen(FuncDecl& node, const std::string& owner) {
        // extern fnc — реализация в runtime prelude, ничего не генерируем
        if (node.is_extern) return;

        std::string ret = map_type(node.return_type.get());

        bool is_method = !owner.empty();
        bool inside_class = is_method && indent_ > 0;

        // Определяем has_self
        bool has_self = false;
        for (auto& p : node.params)
            if (p->name == "self") { has_self = true; break; }

        // Имя функции с sanitize (new → flux_new)
        std::string fname;
        if (inside_class) {
            fname = sanitize_name(node.name);
        } else if (is_method) {
            fname = owner + "::" + sanitize_name(node.name);
        } else {
            fname = sanitize_name(node.name);
        }

        // Параметры (self пропускаем)
        std::string params;
        bool first = true;
        for (auto& param : node.params) {
            if (param->name == "self") continue;
            if (!first) params += ", ";
            first = false;
            std::string ptype = param->type ? map_type(param->type.get()) : "auto";
            params += ptype + " " + param->name;
        }

        // static для методов без self
        std::string prefix;
        if (is_method && !has_self)
            prefix = "static ";

        std::string signature = prefix + ret + " " + fname + "(" + params + ")";
        emitln(signature + " {");
        indent();
        if (node.body) gen(*node.body);
        dedent();
        emitln("}");
        emit("\n");
    }


    // ─── Statements ──────────────────────────────────────────────────────────────

    void CodeGen::gen(ASTNode& node) {
        if (auto* n = dynamic_cast<BlockStmt*>(&node))    return gen(*n);
        if (auto* n = dynamic_cast<VarDecl*>(&node))      return gen(*n);
        if (auto* n = dynamic_cast<ReturnStmt*>(&node))   return gen(*n);
        if (auto* n = dynamic_cast<IfStmt*>(&node))       return gen(*n);
        if (auto* n = dynamic_cast<ForStmt*>(&node))      return gen(*n);
        if (auto* n = dynamic_cast<WhileStmt*>(&node))    return gen(*n);
        if (auto* n = dynamic_cast<ExprStmt*>(&node))     return gen(*n);
        if (auto* n = dynamic_cast<BreakStmt*>(&node))    return emitln("break;");
        if (auto* n = dynamic_cast<ContinueStmt*>(&node)) return emitln("continue;");
    }

    void CodeGen::gen(BlockStmt& node) {
        for (auto& stmt : node.stmts)
            if (stmt) gen(*stmt);
    }

    void CodeGen::gen(VarDecl& node) {
        std::string type_str;
        if (node.type)
            type_str = map_type(node.type.get());
        else
            type_str = "auto";

        std::string line = type_str + " " + node.name;
        if (node.init)
            line += " = " + expr(*node.init);
        line += ";";
        emitln(line);
    }

    void CodeGen::gen(ReturnStmt& node) {
        if (node.value)
            emitln("return " + expr(*node.value) + ";");
        else
            emitln("return;");
    }

    void CodeGen::gen(IfStmt& node) {
        emitln("if (" + expr(*node.cond) + ") {");
        indent();
        if (node.then_block) gen(*node.then_block);
        dedent();
        if (node.else_branch) {
            emitln("} else {");
            indent();
            gen(*node.else_branch);
            dedent();
        }
        emitln("}");
    }

    void CodeGen::gen(ForStmt& node) {
        // for (let x: T = init; cond; step)
        std::string init_str, cond_str, step_str;
        if (node.init) {
            auto* vd = dynamic_cast<VarDecl*>(node.init.get());
            if (vd) {
                std::string t = vd->type ? map_type(vd->type.get()) : "auto";
                init_str = t + " " + vd->name + " = " + (vd->init ? expr(*vd->init) : "0");
            }
        }
        if (node.cond) cond_str = expr(*node.cond);
        if (node.step) step_str = expr(*node.step);

        emitln("for (" + init_str + "; " + cond_str + "; " + step_str + ") {");
        indent();
        if (node.body) gen(*node.body);
        dedent();
        emitln("}");
    }

    void CodeGen::gen(WhileStmt& node) {
        emitln("while (" + expr(*node.cond) + ") {");
        indent();
        if (node.body) gen(*node.body);
        dedent();
        emitln("}");
    }

    void CodeGen::gen(ExprStmt& node) {
        if (node.expr) emitln(expr(*node.expr) + ";");
    }

    // ─── Expressions ─────────────────────────────────────────────────────────────

    std::string CodeGen::expr(ASTNode& node) {
        if (auto* n = dynamic_cast<IntLiteral*>(&node))        return expr(*n);
        if (auto* n = dynamic_cast<FloatLiteral*>(&node))      return expr(*n);
        if (auto* n = dynamic_cast<StrLiteral*>(&node))        return expr(*n);
        if (auto* n = dynamic_cast<BoolLiteral*>(&node))       return expr(*n);
        if (auto* n = dynamic_cast<IdentExpr*>(&node))         return expr(*n);
        if (auto* n = dynamic_cast<SelfExpr*>(&node))          return expr(*n);
        if (auto* n = dynamic_cast<BinaryExpr*>(&node))        return expr(*n);
        if (auto* n = dynamic_cast<UnaryExpr*>(&node))         return expr(*n);
        if (auto* n = dynamic_cast<AssignExpr*>(&node))        return expr(*n);
        if (auto* n = dynamic_cast<MethodCallExpr*>(&node))    return expr(*n);
        if (auto* n = dynamic_cast<CallExpr*>(&node))          return expr(*n);
        if (auto* n = dynamic_cast<FieldAccessExpr*>(&node))   return expr(*n);
        if (auto* n = dynamic_cast<IndexExpr*>(&node))         return expr(*n);
        if (auto* n = dynamic_cast<ArrayLiteral*>(&node))      return expr(*n);
        if (auto* n = dynamic_cast<StructInitExpr*>(&node))    return expr(*n);
        if (auto* n = dynamic_cast<MatchExpr*>(&node))         return expr(*n);
        if (auto* n = dynamic_cast<ImplicitCastExpr*>(&node)) {
            return "static_cast<" + map_type(n->target_type.get()) + ">(" + expr(*n->inner) + ")";
        }
        return "/* unknown_expr */";
    }

    std::string CodeGen::expr(IntLiteral& n)   { return std::to_string(n.value); }
    std::string CodeGen::expr(FloatLiteral& n) { return std::to_string(n.value); }
    std::string CodeGen::expr(StrLiteral& n)   { return "std::string(" + n.value + ")"; }
    std::string CodeGen::expr(BoolLiteral& n)  { return n.value ? "true" : "false"; }
    std::string CodeGen::expr(IdentExpr& n)    { return n.name; }
    std::string CodeGen::expr(SelfExpr&)       { return "(*this)"; }

    std::string CodeGen::expr(BinaryExpr& n) {
        return "(" + expr(*n.lhs) + " " + n.op + " " + expr(*n.rhs) + ")";
    }

    std::string CodeGen::expr(UnaryExpr& n) {
        switch (n.op) {
            case UnaryExpr::Op::Neg:     return "(-" + expr(*n.operand) + ")";
            case UnaryExpr::Op::Not:     return "(!" + expr(*n.operand) + ")";
            case UnaryExpr::Op::BitNot:  return "(~" + expr(*n.operand) + ")";
            case UnaryExpr::Op::PreInc:  return "(++" + expr(*n.operand) + ")";
            case UnaryExpr::Op::PreDec:  return "(--" + expr(*n.operand) + ")";
            case UnaryExpr::Op::PostInc: return "(" + expr(*n.operand) + "++)";
            case UnaryExpr::Op::PostDec: return "(" + expr(*n.operand) + "--)";
            case UnaryExpr::Op::AddrOf:  return "(&" + expr(*n.operand) + ")";
            case UnaryExpr::Op::Deref:   return "(*" + expr(*n.operand) + ")";
        }
        return "";
    }

    std::string CodeGen::expr(AssignExpr& n) {
        return expr(*n.target) + " " + n.op + " " + expr(*n.value);
    }

    std::string CodeGen::expr(CallExpr& n) {
        // Ok(...) / Err(...)
        if (n.callee == "Ok") {
            std::string inner = n.args.empty() ? "" : expr(*n.args[0]);
            return "Result_Ok(" + inner + ")";
        }
        if (n.callee == "Err") {
            std::string inner = n.args.empty() ? "" : expr(*n.args[0]);
            return "Result_Err(" + inner + ")";
        }

        std::string args;
        for (size_t i = 0; i < n.args.size(); i++) {
            if (i) args += ", ";
            args += expr(*n.args[i]);
        }
        return n.callee + "(" + args + ")";
    }

    std::string CodeGen::expr(MethodCallExpr& n) {
        // Builtin методы
        if (n.method == "to_string") return expr(*n.receiver) + ".to_string()";  // std::string::to_string уже есть
        if (n.method == "len")       return expr(*n.receiver) + ".size()";
        if (n.method == "push")      return expr(*n.receiver) + ".push_back(" + (n.args.empty() ? "" : expr(*n.args[0])) + ")";
        if (n.method == "pop")       return expr(*n.receiver) + ".pop_back()";
        if (n.method == "is_empty")  return expr(*n.receiver) + ".empty()";
        if (n.method == "contains")  return expr(*n.receiver) + ".contains(" + (n.args.empty() ? "" : expr(*n.args[0])) + ")";

        std::string args;
        for (size_t i = 0; i < n.args.size(); i++) {
            if (i) args += ", ";
            args += expr(*n.args[i]);
        }

        // Статический вызов: TypeName.method(args)
        if (auto* id = dynamic_cast<IdentExpr*>(n.receiver.get())) {
            if (!id->name.empty() && std::isupper(id->name[0]))
                return id->name + "::" + sanitize_name(n.method) + "(" + args + ")";
        }

        // self.method() → this->method()
        if (dynamic_cast<SelfExpr*>(n.receiver.get()))
            return "this->" + sanitize_name(n.method) + "(" + args + ")";

        return expr(*n.receiver) + "." + sanitize_name(n.method) + "(" + args + ")";
    }

    std::string CodeGen::expr(FieldAccessExpr& n) {
        if (auto* se = dynamic_cast<SelfExpr*>(n.object.get()))
            return "this->" + n.field;
        return expr(*n.object) + "." + n.field;
    }

    std::string CodeGen::expr(IndexExpr& n) {
        return expr(*n.array) + "[" + expr(*n.index) + "]";
    }

    std::string CodeGen::expr(ArrayLiteral& n) {
        std::string elems;
        for (size_t i = 0; i < n.elements.size(); i++) {
            if (i) elems += ", ";
            elems += expr(*n.elements[i]);
        }
        return "{" + elems + "}";
    }

    std::string CodeGen::expr(StructInitExpr& n) {
        // Point { x: 1, y: 2 } → Point{.x=1, .y=2}  (C++20 designated init)
        std::string fields;
        for (size_t i = 0; i < n.fields.size(); i++) {
            if (i) fields += ", ";
            fields += "." + n.fields[i].name + "=" + expr(*n.fields[i].value);
        }
        return n.type_name + "{" + fields + "}";
    }

    std::string CodeGen::expr(MatchExpr& n) {
        // match транслируется в лямбду с if/else if
        std::string subject = expr(*n.subject);
        std::string result = "[&]() {\n";
        std::string pad(indent_ * 4 + 4, ' ');
        std::string pad2(indent_ * 4 + 8, ' ');

        std::string subject_var = "__match_val";
        result += pad + "auto " + subject_var + " = " + subject + ";\n";

        bool first = true;
        for (auto& arm : n.arms) {
            std::string cond;
            std::string bindings;

            if (dynamic_cast<WildcardPattern*>(arm->pattern.get())) {
                // wildcard → else
            } else if (auto* lp = dynamic_cast<LiteralPattern*>(arm->pattern.get())) {
                cond = subject_var + " == " + expr(*lp->value);
            } else if (auto* ip = dynamic_cast<IdentPattern*>(arm->pattern.get())) {
                bindings = pad2 + "auto " + ip->name + " = " + subject_var + ";\n";
            } else if (auto* cp = dynamic_cast<ConstructorPattern*>(arm->pattern.get())) {
                // Ok(x) / Err(e)
                if (cp->name == "Ok")
                    cond = subject_var + ".is_ok()";
                else if (cp->name == "Err")
                    cond = subject_var + ".is_err()";
                else
                    cond = "/* " + cp->name + " */true";
                // биндинги аргументов
                for (size_t i = 0; i < cp->args.size(); i++) {
                    if (auto* ap = dynamic_cast<IdentPattern*>(cp->args[i].get())) {
                        if (cp->name == "Ok")
                            bindings += pad2 + "auto " + ap->name + " = " + subject_var + ".unwrap();\n";
                        else if (cp->name == "Err")
                            bindings += pad2 + "auto " + ap->name + " = " + subject_var + ".unwrap_err();\n";
                    }
                }
            }

            // Генерация ветки
            std::string branch_kw = first ? "if" : "} else if";
            if (cond.empty()) {
                result += pad + "} else {\n";
            } else {
                result += pad + branch_kw + " (" + cond + ") {\n";
            }
            result += bindings;

            // Тело arm
            if (arm->body) {
                for (auto& stmt : arm->body->stmts) {
                    if (stmt) {
                        // Встраиваем — если это ExprStmt, делаем return
                        if (auto* es = dynamic_cast<ExprStmt*>(stmt.get()))
                            result += pad2 + "return " + expr(*es->expr) + ";\n";
                        else {
                            // Для блоков — рекурсивно
                            result += pad2 + "/* stmt */;\n";
                        }
                    }
                }
            }
            first = false;
        }
        result += pad + "}\n";
        result += pad + "return {};\n";
        result += std::string(indent_ * 4, ' ') + "}()";
        return result;
    }

} // namespace flux

#include "flux/complete/complete.h"

#include "flux/lexer/lexer.h"
#include "flux/preprocessor/preprocessor.h"
#include "flux/parser/parser.h"
#include "flux/sema/sema.h"
#include "flux/common/diagnostic.h"
#include "flux/ast/decl.h"
#include "flux/ast/stmt.h"
#include "flux/ast/expr.h"
#include "flux/ast/type.h"

#include <filesystem>
#include <sstream>
#include <cctype>
#include <unordered_map>

namespace flux {

namespace {

// ── Ключевые слова и встроенные типы ─────────────────────────────────────────

const std::vector<std::string> FLUX_KEYWORDS = {
    "fnc", "let", "return", "if", "else", "for", "while",
    "continue", "break", "match", "pub", "struct", "class",
    "impl", "self", "true", "false"
};

const std::vector<std::string> FLUX_BUILTIN_TYPES = {
    "int8",  "int16",  "int32",  "int64",  "int128",
    "uint8", "uint16", "uint32", "uint64", "uint128",
    "float", "double", "bool",   "str",    "string",
    "isize_t", "usize_t"
};

// ── Анализ контекста курсора ──────────────────────────────────────────────────

// Разбивает source на строки
std::vector<std::string> split_lines(const std::string& src) {
    std::vector<std::string> lines;
    std::istringstream ss(src);
    std::string ln;
    while (std::getline(ss, ln)) lines.push_back(ln);
    return lines;
}

// Возвращает текст строки до позиции col (1-based)
std::string text_before_cursor(const std::string& source, uint32_t line, uint32_t col) {
    auto lines = split_lines(source);
    if (line == 0 || line > (uint32_t)lines.size()) return "";
    const auto& l = lines[line - 1];
    uint32_t take = (col > 0) ? col - 1 : 0;
    if (take > (uint32_t)l.size()) take = (uint32_t)l.size();
    return l.substr(0, take);
}

// Если текст перед курсором заканчивается на `identifier.`,
// возвращает имя идентификатора перед точкой. Иначе — пустую строку.
std::string dot_receiver(const std::string& before) {
    size_t end = before.size();
    while (end > 0 && before[end - 1] == ' ') --end;
    if (end == 0 || before[end - 1] != '.') return "";
    size_t dot = end - 1;
    size_t id_end = dot;
    if (id_end == 0) return "";
    size_t id_start = id_end;
    while (id_start > 0 &&
           (std::isalnum((unsigned char)before[id_start - 1]) || before[id_start - 1] == '_'))
        --id_start;
    if (id_start == id_end) return "";
    return before.substr(id_start, id_end - id_start);
}

// ── Обход AST для сбора локальных переменных ─────────────────────────────────

// Рекурсивно обходит блок, собирая VarDecl чья строка < cursor_line
void collect_vars_in_block(
    BlockStmt* block,
    uint32_t   cursor_line,
    std::unordered_map<std::string, std::string>& out
) {
    if (!block) return;
    for (auto& stmt : block->stmts) {
        if (!stmt || stmt->loc.line >= cursor_line) continue;

        if (auto* vd = dynamic_cast<VarDecl*>(stmt.get())) {
            std::string tstr = vd->type
                ? SemanticAnalyzer::type_to_str(vd->type.get())
                : "auto";
            out[vd->name] = tstr;
        }
        // Рекурсия в вложенные блоки
        if (auto* ifs = dynamic_cast<IfStmt*>(stmt.get())) {
            collect_vars_in_block(ifs->then_block.get(), cursor_line, out);
            if (auto* eb = dynamic_cast<BlockStmt*>(ifs->else_branch.get()))
                collect_vars_in_block(eb, cursor_line, out);
        }
        if (auto* fs = dynamic_cast<ForStmt*>(stmt.get())) {
            if (auto* vd = dynamic_cast<VarDecl*>(fs->init.get())) {
                std::string tstr = vd->type
                    ? SemanticAnalyzer::type_to_str(vd->type.get())
                    : "auto";
                out[vd->name] = tstr;
            }
            collect_vars_in_block(fs->body.get(), cursor_line, out);
        }
        if (auto* ws = dynamic_cast<WhileStmt*>(stmt.get()))
            collect_vars_in_block(ws->body.get(), cursor_line, out);
    }
}

// Ищет FuncDecl, строка объявления которого ближайшая к cursor_line снизу
struct FuncContext { FuncDecl* func; std::string owner; };

FuncContext find_enclosing_func(ProgramNode* program, uint32_t cursor_line) {
    FuncDecl* best      = nullptr;
    std::string best_owner;
    uint32_t best_line  = 0;

    auto try_func = [&](FuncDecl* fd, const std::string& owner) {
        if (fd && fd->loc.line <= cursor_line && fd->loc.line > best_line) {
            best       = fd;
            best_line  = fd->loc.line;
            best_owner = owner;
        }
    };

    for (auto& d : program->decls) {
        if (auto* fd   = dynamic_cast<FuncDecl*>(d.get()))
            try_func(fd, "");
        else if (auto* impl = dynamic_cast<ImplDecl*>(d.get()))
            for (auto& m : impl->methods) try_func(m.get(), impl->target);
        else if (auto* cls  = dynamic_cast<ClassDecl*>(d.get()))
            for (auto& m : cls->methods)  try_func(m.get(), cls->name);
    }
    return {best, best_owner};
}

// Собирает параметры + локальные переменные, видимые в точке cursor_line
std::unordered_map<std::string, std::string> collect_local_vars(
    ProgramNode* program,
    uint32_t     cursor_line
) {
    std::unordered_map<std::string, std::string> vars;
    auto [func, owner] = find_enclosing_func(program, cursor_line);
    if (!func) return vars;

    if (func->has_self && !owner.empty())
        vars["self"] = owner;

    for (auto& p : func->params) {
        std::string tstr = p->type
            ? SemanticAnalyzer::type_to_str(p->type.get())
            : owner;
        vars[p->name] = tstr;
    }

    collect_vars_in_block(func->body.get(), cursor_line, vars);
    return vars;
}

// ── Вспомогательные ──────────────────────────────────────────────────────────

std::string func_signature(const FuncSignature& sig) {
    std::string s = "fnc ";
    if (!sig.owner.empty()) s += sig.owner + "::";
    s += sig.name + "(";
    for (size_t i = 0; i < sig.param_types.size(); ++i) {
        if (i) s += ", ";
        s += sig.param_types[i];
    }
    return s + ") -> " + sig.return_type;
}

// ── Вспомогательная: слово под курсором ──────────────────────────────────────

// Extract identifier word at 1-based (line, col). col points into or adjacent to the word.
std::string word_at_cursor(const std::string& source, uint32_t line, uint32_t col) {
    auto lines = split_lines(source);
    if (line == 0 || line > static_cast<uint32_t>(lines.size())) return "";
    const auto& l = lines[line - 1];
    size_t ci = col > 0 ? col - 1 : 0;
    if (ci >= l.size()) ci = l.size() > 0 ? l.size() - 1 : 0;

    size_t start = ci;
    while (start > 0 && (std::isalnum(static_cast<unsigned char>(l[start-1])) || l[start-1] == '_'))
        --start;
    size_t end = ci;
    while (end < l.size() && (std::isalnum(static_cast<unsigned char>(l[end])) || l[end] == '_'))
        ++end;

    if (start == end) return "";
    return l.substr(start, end - start);
}

// Рекурсивный поиск VarDecl с именем `word` в блоке (до cursor_line)
DefinitionResult find_var_def_in_block(BlockStmt* block,
                                        const std::string& word,
                                        uint32_t cursor_line,
                                        const std::string& fallback_path) {
    if (!block) return {};
    for (auto& stmt : block->stmts) {
        if (!stmt || stmt->loc.line >= cursor_line) continue;
        if (auto* vd = dynamic_cast<VarDecl*>(stmt.get())) {
            if (vd->name == word) {
                std::string fp = vd->loc.filepath.empty() ? fallback_path : vd->loc.filepath;
                return {fp, vd->loc.line, vd->loc.col};
            }
        }
        if (auto* ifs = dynamic_cast<IfStmt*>(stmt.get())) {
            auto r = find_var_def_in_block(ifs->then_block.get(), word, cursor_line, fallback_path);
            if (!r.filepath.empty()) return r;
            if (auto* eb = dynamic_cast<BlockStmt*>(ifs->else_branch.get())) {
                r = find_var_def_in_block(eb, word, cursor_line, fallback_path);
                if (!r.filepath.empty()) return r;
            }
        }
        if (auto* fs = dynamic_cast<ForStmt*>(stmt.get())) {
            if (auto* vd = dynamic_cast<VarDecl*>(fs->init.get())) {
                if (vd->name == word) {
                    std::string fp = vd->loc.filepath.empty() ? fallback_path : vd->loc.filepath;
                    return {fp, vd->loc.line, vd->loc.col};
                }
            }
            auto r = find_var_def_in_block(fs->body.get(), word, cursor_line, fallback_path);
            if (!r.filepath.empty()) return r;
        }
        if (auto* ws = dynamic_cast<WhileStmt*>(stmt.get())) {
            auto r = find_var_def_in_block(ws->body.get(), word, cursor_line, fallback_path);
            if (!r.filepath.empty()) return r;
        }
    }
    return {};
}

} // anonymous namespace

// ── Публичный API ─────────────────────────────────────────────────────────────

std::vector<CompletionItem> compute_completions(
    const std::string& source,
    const std::string& filepath,
    uint32_t line,
    uint32_t col
) {
    std::vector<CompletionItem> items;

    // ── 1. Определяем контекст ────────────────────────────────────────────────
    std::string before   = text_before_cursor(source, line, col);
    std::string dot_recv = dot_receiver(before);
    bool is_dot          = !dot_recv.empty();

    // ── 2. Запускаем pipeline (ошибки игнорируем) ─────────────────────────────
    DiagEngine diag;

    Lexer lexer(source, filepath, diag);
    auto tokens = lexer.tokenize();

    Preprocessor pp(diag, std::filesystem::path(filepath).parent_path());
    auto processed = pp.process(tokens, std::filesystem::path(filepath));

    Parser parser(std::move(processed), diag);
    auto program = parser.parse_program();

    SemanticAnalyzer sema(diag);
    if (program) sema.analyze(*program);

    const auto& ftable = sema.func_table();
    const auto& ttable = sema.type_table();

    // ── 3. Dot-completion: `recv.` ────────────────────────────────────────────
    if (is_dot) {
        std::string recv_type;

        if (program) {
            auto locals = collect_local_vars(program.get(), line);
            auto it = locals.find(dot_recv);
            if (it != locals.end()) recv_type = it->second;
        }
        // Если перед точкой стоит имя типа (статический вызов, напр. Point.new)
        if (recv_type.empty() && ttable.count(dot_recv))
            recv_type = dot_recv;

        if (!recv_type.empty()) {
            // Поля
            auto tit = ttable.find(recv_type);
            if (tit != ttable.end()) {
                for (auto& f : tit->second.fields)
                    items.emplace_back(f.name, ck::Field, f.type);
            }
            // Методы
            for (auto& [fname, overloads] : ftable)
                for (auto& sig : overloads)
                    if (sig.owner == recv_type)
                        items.emplace_back(sig.name, ck::Method, func_signature(sig));
        }
        return items;
    }

    // ── 4. Общая подстановка ──────────────────────────────────────────────────

    // Ключевые слова
    for (auto& kw : FLUX_KEYWORDS)
        items.emplace_back(kw, ck::Keyword, "");

    // Встроенные типы
    for (auto& t : FLUX_BUILTIN_TYPES)
        items.emplace_back(t, ck::Keyword, "built-in type");

    // Пользовательские типы (struct / class)
    for (auto& [tname, tinfo] : ttable)
        items.emplace_back(tname,
                         tinfo.is_class ? ck::Class : ck::Struct,
                         tinfo.is_class ? "class" : "struct");

    // Свободные функции
    for (auto& [fname, overloads] : ftable)
        for (auto& sig : overloads)
            if (sig.owner.empty())
                items.emplace_back(sig.name, ck::Function, func_signature(sig));

    // Локальные переменные и параметры
    if (program) {
        auto locals = collect_local_vars(program.get(), line);
        for (auto& [vname, vtype] : locals)
            items.emplace_back(vname, ck::Variable, vtype);
    }

    return items;
}

// ── compute_hover ─────────────────────────────────────────────────────────────

HoverResult compute_hover(const std::string& source, const std::string& filepath,
                           uint32_t line, uint32_t col) {
    std::string word = word_at_cursor(source, line, col);
    if (word.empty()) return {};

    // Run pipeline (errors are silently ignored)
    DiagEngine diag;
    Lexer lexer(source, filepath, diag);
    auto tokens = lexer.tokenize();
    Preprocessor pp(diag, std::filesystem::path(filepath).parent_path());
    auto processed = pp.process(tokens, std::filesystem::path(filepath));
    Parser parser(std::move(processed), diag);
    auto program = parser.parse_program();
    SemanticAnalyzer sema(diag);
    if (program) sema.analyze(*program);

    const auto& ftable = sema.func_table();
    const auto& ttable = sema.type_table();

    // Check functions
    auto fit = ftable.find(word);
    if (fit != ftable.end() && !fit->second.empty()) {
        auto& sig = fit->second[0];
        std::string md = "```flux\n" + func_signature(sig) + "\n```";
        if (fit->second.size() > 1)
            md += "\n\n*+" + std::to_string(fit->second.size() - 1) + " overload(s)*";
        return {md};
    }

    // Check types
    auto tit = ttable.find(word);
    if (tit != ttable.end()) {
        const auto& ti = tit->second;
        std::string kind_str = ti.is_class ? "class" : "struct";
        std::string md = "```flux\n" + kind_str + " " + word + " {\n";
        for (auto& f : ti.fields)
            md += "    " + f.name + ": " + f.type + ",\n";
        md += "}\n```";
        return {md};
    }

    // Check local vars at this position
    if (program) {
        auto locals = collect_local_vars(program.get(), line);
        auto vit = locals.find(word);
        if (vit != locals.end())
            return {"```flux\n" + word + ": " + vit->second + "\n```"};
    }

    // Built-in types
    static const std::unordered_map<std::string, std::string> builtins = {
        {"int32",  "32-bit signed integer"},
        {"int64",  "64-bit signed integer"},
        {"int8",   "8-bit signed integer"},
        {"int16",  "16-bit signed integer"},
        {"int128", "128-bit signed integer"},
        {"uint32", "32-bit unsigned integer"},
        {"uint64", "64-bit unsigned integer"},
        {"uint8",  "8-bit unsigned integer"},
        {"uint16", "16-bit unsigned integer"},
        {"uint128","128-bit unsigned integer"},
        {"float",  "32-bit float"},
        {"double", "64-bit float"},
        {"bool",   "boolean"},
        {"str",    "string slice"},
        {"string", "owned string"},
        {"isize_t","pointer-sized signed integer"},
        {"usize_t","pointer-sized unsigned integer"},
    };
    auto bit = builtins.find(word);
    if (bit != builtins.end())
        return {"**" + word + "** — " + bit->second};

    return {};
}

// ── compute_definition ────────────────────────────────────────────────────────

DefinitionResult compute_definition(const std::string& source, const std::string& filepath,
                                     uint32_t line, uint32_t col) {
    std::string word = word_at_cursor(source, line, col);
    if (word.empty()) return {};

    DiagEngine diag;
    Lexer lexer(source, filepath, diag);
    auto tokens = lexer.tokenize();
    Preprocessor pp(diag, std::filesystem::path(filepath).parent_path());
    auto processed = pp.process(tokens, std::filesystem::path(filepath));
    Parser parser(std::move(processed), diag);
    auto program = parser.parse_program();
    if (!program) return {};

    auto loc_or = [&](const SourceLocation& loc) -> DefinitionResult {
        std::string fp = loc.filepath.empty() ? filepath : loc.filepath;
        return {fp, loc.line, loc.col};
    };

    // Обход всех деклараций верхнего уровня
    for (auto& d : program->decls) {
        if (auto* fd = dynamic_cast<FuncDecl*>(d.get())) {
            if (fd->name == word) return loc_or(fd->loc);
        }
        if (auto* sd = dynamic_cast<StructDecl*>(d.get())) {
            if (sd->name == word) return loc_or(sd->loc);
        }
        if (auto* cd = dynamic_cast<ClassDecl*>(d.get())) {
            if (cd->name == word) return loc_or(cd->loc);
            for (auto& m : cd->methods)
                if (m->name == word) return loc_or(m->loc);
        }
        if (auto* impl = dynamic_cast<ImplDecl*>(d.get())) {
            for (auto& m : impl->methods)
                if (m->name == word) return loc_or(m->loc);
        }
    }

    // Локальные переменные и параметры в объемлющей функции
    auto [func, owner] = find_enclosing_func(program.get(), line);
    if (func) {
        for (auto& p : func->params)
            if (p->name == word) return loc_or(p->loc);
        if (func->has_self && word == "self") return loc_or(func->loc);
        auto r = find_var_def_in_block(func->body.get(), word, line, filepath);
        if (!r.filepath.empty()) return r;
    }

    return {};
}

} // namespace flux

#include "flux/lsp/server.h"
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

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <cctype>
#include <algorithm>
#include <initializer_list>
#include <cstring>

#ifdef _WIN32
#  include <fcntl.h>
#  include <io.h>
#endif

namespace flux {

// ═════════════════════════════════════════════════════════════════════════════
// Minimal JSON implementation
// ═════════════════════════════════════════════════════════════════════════════

struct Json {
    enum class Kind { Null, Bool, Num, Str, Arr, Obj };

    Kind kind = Kind::Null;
    bool b = false;
    double n = 0.0;
    std::string s;
    std::vector<Json> arr;
    std::vector<std::pair<std::string, Json>> obj;

    // Constructors
    Json() : kind(Kind::Null) {}
    explicit Json(bool v) : kind(Kind::Bool), b(v) {}
    explicit Json(double v) : kind(Kind::Num), n(v) {}
    explicit Json(int v) : kind(Kind::Num), n(static_cast<double>(v)) {}
    explicit Json(int64_t v) : kind(Kind::Num), n(static_cast<double>(v)) {}
    explicit Json(const char* v) : kind(Kind::Str), s(v ? v : "") {}
    explicit Json(std::string v) : kind(Kind::Str), s(std::move(v)) {}

    static Json object(std::initializer_list<std::pair<const char*, Json>> pairs) {
        Json j;
        j.kind = Kind::Obj;
        for (auto& [k, v] : pairs)
            j.obj.emplace_back(k, v);
        return j;
    }

    static Json array(std::vector<Json> items) {
        Json j;
        j.kind = Kind::Arr;
        j.arr = std::move(items);
        return j;
    }

    // Object key lookup
    const Json& operator[](const std::string& key) const {
        if (kind == Kind::Obj) {
            for (auto& [k, v] : obj)
                if (k == key) return v;
        }
        static const Json null_json;
        return null_json;
    }

    // Array index lookup
    const Json& operator[](size_t idx) const {
        if (kind == Kind::Arr && idx < arr.size())
            return arr[idx];
        static const Json null_json;
        return null_json;
    }

    bool contains(const std::string& key) const {
        if (kind != Kind::Obj) return false;
        for (auto& [k, v] : obj)
            if (k == key) return true;
        return false;
    }

    std::string as_str(std::string def = "") const {
        if (kind == Kind::Str) return s;
        if (kind == Kind::Num) {
            if (n == std::floor(n))
                return std::to_string(static_cast<int64_t>(n));
            return std::to_string(n);
        }
        if (kind == Kind::Bool) return b ? "true" : "false";
        return def;
    }

    int64_t as_int(int64_t def = 0) const {
        if (kind == Kind::Num) return static_cast<int64_t>(n);
        if (kind == Kind::Bool) return b ? 1 : 0;
        if (kind == Kind::Str) {
            try { return std::stoll(s); } catch (...) {}
        }
        return def;
    }

    bool as_bool(bool def = false) const {
        if (kind == Kind::Bool) return b;
        if (kind == Kind::Num) return n != 0.0;
        if (kind == Kind::Str) return !s.empty();
        if (kind == Kind::Null) return false;
        return def;
    }

    bool is_null() const { return kind == Kind::Null; }

    // ── JSON serializer ───────────────────────────────────────────────────────
    std::string dump() const {
        switch (kind) {
            case Kind::Null: return "null";
            case Kind::Bool: return b ? "true" : "false";
            case Kind::Num: {
                if (n == std::floor(n) && n >= static_cast<double>(INT64_MIN) && n <= static_cast<double>(INT64_MAX))
                    return std::to_string(static_cast<int64_t>(n));
                // floating point
                std::ostringstream oss;
                oss << n;
                return oss.str();
            }
            case Kind::Str: {
                std::string r;
                r.reserve(s.size() + 2);
                r += '"';
                for (unsigned char c : s) {
                    if      (c == '"')  r += "\\\"";
                    else if (c == '\\') r += "\\\\";
                    else if (c == '\n') r += "\\n";
                    else if (c == '\r') r += "\\r";
                    else if (c == '\t') r += "\\t";
                    else if (c < 0x20) {
                        char buf[7];
                        std::snprintf(buf, sizeof(buf), "\\u%04x", (unsigned)c);
                        r += buf;
                    } else {
                        r += static_cast<char>(c);
                    }
                }
                r += '"';
                return r;
            }
            case Kind::Arr: {
                std::string r = "[";
                for (size_t i = 0; i < arr.size(); ++i) {
                    if (i) r += ',';
                    r += arr[i].dump();
                }
                r += ']';
                return r;
            }
            case Kind::Obj: {
                std::string r = "{";
                for (size_t i = 0; i < obj.size(); ++i) {
                    if (i) r += ',';
                    // serialize key as JSON string
                    Json key_json(obj[i].first);
                    r += key_json.dump();
                    r += ':';
                    r += obj[i].second.dump();
                }
                r += '}';
                return r;
            }
        }
        return "null";
    }

    // ── Recursive descent JSON parser ─────────────────────────────────────────
    static Json parse(std::string_view src) {
        size_t pos = 0;
        Json result = parse_value(src, pos);
        return result;
    }

private:
    static void skip_ws(std::string_view src, size_t& pos) {
        while (pos < src.size() && (src[pos] == ' ' || src[pos] == '\t' ||
               src[pos] == '\n' || src[pos] == '\r'))
            ++pos;
    }

    static Json parse_value(std::string_view src, size_t& pos) {
        skip_ws(src, pos);
        if (pos >= src.size()) return Json{};

        char c = src[pos];
        if (c == '"')  return parse_string(src, pos);
        if (c == '{')  return parse_object(src, pos);
        if (c == '[')  return parse_array(src, pos);
        if (c == 't')  return parse_literal(src, pos, "true",  Json(true));
        if (c == 'f')  return parse_literal(src, pos, "false", Json(false));
        if (c == 'n')  return parse_literal(src, pos, "null",  Json{});
        if (c == '-' || (c >= '0' && c <= '9')) return parse_number(src, pos);
        return Json{};
    }

    static Json parse_literal(std::string_view src, size_t& pos,
                              const char* lit, Json val) {
        size_t len = std::strlen(lit);
        if (src.substr(pos, len) == lit) {
            pos += len;
            return val;
        }
        return Json{};
    }

    static Json parse_number(std::string_view src, size_t& pos) {
        size_t start = pos;
        if (pos < src.size() && src[pos] == '-') ++pos;
        while (pos < src.size() && src[pos] >= '0' && src[pos] <= '9') ++pos;
        if (pos < src.size() && src[pos] == '.') {
            ++pos;
            while (pos < src.size() && src[pos] >= '0' && src[pos] <= '9') ++pos;
        }
        if (pos < src.size() && (src[pos] == 'e' || src[pos] == 'E')) {
            ++pos;
            if (pos < src.size() && (src[pos] == '+' || src[pos] == '-')) ++pos;
            while (pos < src.size() && src[pos] >= '0' && src[pos] <= '9') ++pos;
        }
        std::string num_str(src.substr(start, pos - start));
        try {
            double v = std::stod(num_str);
            return Json(v);
        } catch (...) {
            return Json{};
        }
    }

    static uint32_t parse_hex4(std::string_view src, size_t& pos) {
        uint32_t val = 0;
        for (int i = 0; i < 4 && pos < src.size(); ++i, ++pos) {
            char c = src[pos];
            uint32_t digit = 0;
            if (c >= '0' && c <= '9')      digit = c - '0';
            else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
            val = (val << 4) | digit;
        }
        return val;
    }

    static void utf8_encode(uint32_t codepoint, std::string& out) {
        if (codepoint <= 0x7F) {
            out += static_cast<char>(codepoint);
        } else if (codepoint <= 0x7FF) {
            out += static_cast<char>(0xC0 | (codepoint >> 6));
            out += static_cast<char>(0x80 | (codepoint & 0x3F));
        } else if (codepoint <= 0xFFFF) {
            out += static_cast<char>(0xE0 | (codepoint >> 12));
            out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (codepoint & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (codepoint >> 18));
            out += static_cast<char>(0x80 | ((codepoint >> 18) & 0x3F));  // intentional, covers BMP fallback
            out += static_cast<char>(0x80 | ((codepoint >> 6)  & 0x3F));
            out += static_cast<char>(0x80 | (codepoint & 0x3F));
        }
    }

    static Json parse_string(std::string_view src, size_t& pos) {
        // pos points to opening '"'
        ++pos;
        std::string result;
        while (pos < src.size() && src[pos] != '"') {
            if (src[pos] == '\\') {
                ++pos;
                if (pos >= src.size()) break;
                char esc = src[pos++];
                switch (esc) {
                    case '"':  result += '"';  break;
                    case '\\': result += '\\'; break;
                    case '/':  result += '/';  break;
                    case 'b':  result += '\b'; break;
                    case 'f':  result += '\f'; break;
                    case 'n':  result += '\n'; break;
                    case 'r':  result += '\r'; break;
                    case 't':  result += '\t'; break;
                    case 'u': {
                        uint32_t cp = parse_hex4(src, pos);
                        // Handle surrogate pairs
                        if (cp >= 0xD800 && cp <= 0xDBFF) {
                            // High surrogate — expect \uXXXX low surrogate
                            if (pos + 1 < src.size() && src[pos] == '\\' && src[pos+1] == 'u') {
                                pos += 2;
                                uint32_t low = parse_hex4(src, pos);
                                if (low >= 0xDC00 && low <= 0xDFFF) {
                                    cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                                } else {
                                    utf8_encode(cp, result);
                                    utf8_encode(low, result);
                                    break;
                                }
                            }
                        }
                        utf8_encode(cp, result);
                        break;
                    }
                    default: result += esc; break;
                }
            } else {
                result += src[pos++];
            }
        }
        if (pos < src.size()) ++pos; // consume closing '"'
        return Json(std::move(result));
    }

    static Json parse_array(std::string_view src, size_t& pos) {
        ++pos; // consume '['
        Json j;
        j.kind = Kind::Arr;
        skip_ws(src, pos);
        if (pos < src.size() && src[pos] == ']') { ++pos; return j; }
        while (pos < src.size()) {
            j.arr.push_back(parse_value(src, pos));
            skip_ws(src, pos);
            if (pos < src.size() && src[pos] == ',') { ++pos; continue; }
            if (pos < src.size() && src[pos] == ']') { ++pos; break; }
            break;
        }
        return j;
    }

    static Json parse_object(std::string_view src, size_t& pos) {
        ++pos; // consume '{'
        Json j;
        j.kind = Kind::Obj;
        skip_ws(src, pos);
        if (pos < src.size() && src[pos] == '}') { ++pos; return j; }
        while (pos < src.size()) {
            skip_ws(src, pos);
            if (pos >= src.size() || src[pos] != '"') break;
            Json key = parse_string(src, pos);
            skip_ws(src, pos);
            if (pos < src.size() && src[pos] == ':') ++pos;
            Json val = parse_value(src, pos);
            j.obj.emplace_back(key.s, std::move(val));
            skip_ws(src, pos);
            if (pos < src.size() && src[pos] == ',') { ++pos; continue; }
            if (pos < src.size() && src[pos] == '}') { ++pos; break; }
            break;
        }
        return j;
    }
};

// ═════════════════════════════════════════════════════════════════════════════
// URI utilities
// ═════════════════════════════════════════════════════════════════════════════

// Convert file:///C:/path/to/file.flx → C:/path/to/file.flx (Windows)
// Convert file:///home/user/file.flx  → /home/user/file.flx  (Unix)
static std::string uri_to_path(const std::string& uri) {
    std::string path = uri;
    // Strip "file://"
    if (path.rfind("file://", 0) == 0) {
        path = path.substr(7); // removes "file://"
    }
    // Percent-decode
    std::string decoded;
    decoded.reserve(path.size());
    for (size_t i = 0; i < path.size(); ++i) {
        if (path[i] == '%' && i + 2 < path.size()) {
            char hi = path[i+1], lo = path[i+2];
            auto hex_digit = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return -1;
            };
            int h = hex_digit(hi), l = hex_digit(lo);
            if (h >= 0 && l >= 0) {
                decoded += static_cast<char>((h << 4) | l);
                i += 2;
                continue;
            }
        }
        decoded += path[i];
    }
    // On Windows the path starts with /C:/... — strip the leading slash
#ifdef _WIN32
    if (decoded.size() >= 3 && decoded[0] == '/' &&
        std::isalpha(static_cast<unsigned char>(decoded[1])) && decoded[2] == ':') {
        decoded = decoded.substr(1);
    }
    // Normalise forward slashes to backslashes on Windows is NOT needed for
    // filesystem::path, but let's keep forward slashes for our string usage.
#endif
    return decoded;
}

// ═════════════════════════════════════════════════════════════════════════════
// LspServer
// ═════════════════════════════════════════════════════════════════════════════

class LspServer {
public:
    int run() {
#ifdef _WIN32
        _setmode(_fileno(stdin),  _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);
#endif
        while (running_) {
            Json msg = read_msg();
            if (msg.is_null() && std::cin.eof()) break;
            if (!msg.is_null()) handle(msg);
        }
        return 0;
    }

private:
    std::unordered_map<std::string, std::string> docs_;
    bool running_ = true;

    // ── I/O ──────────────────────────────────────────────────────────────────

    Json read_msg() {
        std::string line;
        int len = -1;
        while (std::getline(std::cin, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) break; // end of headers
            if (line.rfind("Content-Length:", 0) == 0)
                len = std::stoi(line.substr(15));
        }
        if (len <= 0 || std::cin.fail()) return Json{};
        std::string body(static_cast<size_t>(len), '\0');
        std::cin.read(body.data(), len);
        if (std::cin.fail()) return Json{};
        return Json::parse(body);
    }

    void send_msg(const Json& msg) {
        std::string body = msg.dump();
        std::string hdr  = "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n";
        std::cout.write(hdr.data(),  static_cast<std::streamsize>(hdr.size()));
        std::cout.write(body.data(), static_cast<std::streamsize>(body.size()));
        std::cout.flush();
    }

    void send_response(const Json& id, Json result) {
        Json resp = Json::object({
            {"jsonrpc", Json("2.0")},
            {"id",      id},
            {"result",  std::move(result)}
        });
        send_msg(resp);
    }

    void send_error(const Json& id, int code, const std::string& msg_text) {
        Json resp = Json::object({
            {"jsonrpc", Json("2.0")},
            {"id",      id},
            {"error",   Json::object({
                {"code",    Json(code)},
                {"message", Json(msg_text)}
            })}
        });
        send_msg(resp);
    }

    // ── Capabilities ──────────────────────────────────────────────────────────

    Json make_capabilities() {
        return Json::object({
            {"textDocumentSync", Json::object({
                {"openClose", Json(true)},
                {"change",    Json(1)}          // 1 = Full
            })},
            {"completionProvider", Json::object({
                {"triggerCharacters", Json::array({Json(".")})}
            })},
            {"hoverProvider",      Json(true)},
            {"definitionProvider", Json(true)}
        });
    }

    // ── Dispatch ──────────────────────────────────────────────────────────────

    void handle(const Json& msg) {
        const std::string method = msg["method"].as_str();
        const Json& params       = msg["params"];
        bool has_id              = msg.contains("id");
        const Json& id           = msg["id"];

        if (method == "initialize") {
            send_response(id, Json::object({
                {"capabilities", make_capabilities()},
                {"serverInfo",   Json::object({
                    {"name",    Json("fluxc-lsp")},
                    {"version", Json("0.1")}
                })}
            }));
        }
        else if (method == "initialized") {
            // notification — no response
        }
        else if (method == "shutdown") {
            if (has_id) send_response(id, Json{});
            running_ = false;
        }
        else if (method == "exit") {
            running_ = false;
        }
        else if (method == "textDocument/didOpen") {
            std::string uri  = params["textDocument"]["uri"].as_str();
            std::string text = params["textDocument"]["text"].as_str();
            docs_[uri] = std::move(text);
        }
        else if (method == "textDocument/didChange") {
            std::string uri = params["textDocument"]["uri"].as_str();
            // contentChanges is an array; take first element's full text
            const Json& changes = params["contentChanges"];
            if (changes.kind == Json::Kind::Arr && !changes.arr.empty()) {
                docs_[uri] = changes.arr[0]["text"].as_str();
            }
        }
        else if (method == "textDocument/didClose") {
            std::string uri = params["textDocument"]["uri"].as_str();
            docs_.erase(uri);
        }
        else if (method == "textDocument/completion") {
            if (has_id)
                send_response(id, handle_completion(params));
        }
        else if (method == "textDocument/hover") {
            if (has_id) {
                Json result = handle_hover(params);
                send_response(id, std::move(result));
            }
        }
        else if (method == "textDocument/definition") {
            if (has_id)
                send_response(id, handle_definition(params));
        }
        else {
            // Unknown request — send method-not-found error
            if (has_id)
                send_error(id, -32601, "Method not found: " + method);
        }
    }

    // ── Get document source ───────────────────────────────────────────────────

    std::string get_source(const std::string& uri) {
        auto it = docs_.find(uri);
        if (it != docs_.end()) return it->second;
        // Fallback: read from disk
        std::string path = uri_to_path(uri);
        std::ifstream f(path);
        if (!f.is_open()) return {};
        return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
    }

    // ── Completion handler ────────────────────────────────────────────────────

    Json handle_completion(const Json& params) {
        std::string uri  = params["textDocument"]["uri"].as_str();
        std::string path = uri_to_path(uri);
        std::string text = get_source(uri);
        if (text.empty()) {
            return Json::object({
                {"isIncomplete", Json(false)},
                {"items",        Json::array({})}
            });
        }

        // LSP positions are 0-based; our API is 1-based
        int64_t line = params["position"]["line"].as_int() + 1;
        int64_t col  = params["position"]["character"].as_int() + 1;

        std::vector<CompletionItem> items;
        try {
            items = compute_completions(text, path,
                                        static_cast<uint32_t>(line),
                                        static_cast<uint32_t>(col));
        } catch (...) {}

        std::vector<Json> json_items;
        json_items.reserve(items.size());
        for (auto& item : items) {
            json_items.push_back(Json::object({
                {"label",  Json(item.label)},
                {"kind",   Json(item.kind)},
                {"detail", Json(item.detail)}
            }));
        }

        return Json::object({
            {"isIncomplete", Json(false)},
            {"items",        Json::array(std::move(json_items))}
        });
    }

    // ── Hover handler ─────────────────────────────────────────────────────────

    Json handle_hover(const Json& params) {
        std::string uri  = params["textDocument"]["uri"].as_str();
        std::string path = uri_to_path(uri);
        std::string text = get_source(uri);
        if (text.empty()) return Json{};

        // LSP is 0-based; our API is 1-based
        int64_t lsp_line = params["position"]["line"].as_int();
        int64_t lsp_char = params["position"]["character"].as_int();
        uint32_t line = static_cast<uint32_t>(lsp_line + 1);
        uint32_t col  = static_cast<uint32_t>(lsp_char + 1);

        HoverResult hover_result;
        try {
            hover_result = compute_hover(text, path, line, col);
        } catch (...) {}

        if (hover_result.markdown.empty()) return Json{};

        return Json::object({
            {"contents", Json::object({
                {"kind",  Json("markdown")},
                {"value", Json(hover_result.markdown)}
            })}
        });
    }

    // ── Definition handler ────────────────────────────────────────────────────

    // Преобразует путь к файлу в URI (file:///C:/path/file.flx)
    static std::string path_to_uri(const std::string& path) {
        std::string uri = "file:///";
        for (char c : path)
            uri += (c == '\\') ? '/' : c;
        return uri;
    }

    Json handle_definition(const Json& params) {
        std::string uri  = params["textDocument"]["uri"].as_str();
        std::string path = uri_to_path(uri);
        std::string text = get_source(uri);
        if (text.empty()) return Json{};

        // LSP 0-based → наш API 1-based
        uint32_t line = static_cast<uint32_t>(params["position"]["line"].as_int() + 1);
        uint32_t col  = static_cast<uint32_t>(params["position"]["character"].as_int() + 1);

        DefinitionResult def;
        try { def = compute_definition(text, path, line, col); } catch (...) {}

        if (def.filepath.empty() || def.line == 0) return Json{};

        // LSP Range использует 0-based позиции
        uint32_t lsp_line = def.line > 0 ? def.line - 1 : 0;
        uint32_t lsp_col  = def.col  > 0 ? def.col  - 1 : 0;

        return Json::object({
            {"uri",   Json(path_to_uri(def.filepath))},
            {"range", Json::object({
                {"start", Json::object({
                    {"line",      Json(static_cast<int64_t>(lsp_line))},
                    {"character", Json(static_cast<int64_t>(lsp_col))}
                })},
                {"end", Json::object({
                    {"line",      Json(static_cast<int64_t>(lsp_line))},
                    {"character", Json(static_cast<int64_t>(lsp_col))}
                })}
            })}
        });
    }
};

// ═════════════════════════════════════════════════════════════════════════════
// Public entry point
// ═════════════════════════════════════════════════════════════════════════════

int run_lsp_server() {
    LspServer server;
    return server.run();
}

} // namespace flux

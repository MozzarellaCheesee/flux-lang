#pragma once
#include <string>
#include <unordered_map>

namespace flux {

// Встроенные stdlib-модули Flux, хранящиеся как исходный код.
// Все функции объявлены как extern — их реализации в C++ runtime prelude.
static const std::unordered_map<std::string, std::string> STDLIB_MODULES = {

// ─── io ──────────────────────────────────────────────────────────────────────
{"io", R"flux(
extern fnc read_line() -> string;
extern fnc read_int() -> int32;
extern fnc read_uint() -> uint32;
extern fnc read_float() -> float;
extern fnc read_double() -> double;
)flux"},

// ─── math ─────────────────────────────────────────────────────────────────────
{"math", R"flux(
extern fnc sqrt(x: double) -> double;
extern fnc cbrt(x: double) -> double;
extern fnc pow(base: double, exp: double) -> double;
extern fnc abs_i32(x: int32) -> int32;
extern fnc abs_i64(x: int64) -> int64;
extern fnc abs_f64(x: double) -> double;
extern fnc floor(x: double) -> double;
extern fnc ceil(x: double) -> double;
extern fnc round(x: double) -> double;
extern fnc trunc(x: double) -> double;
extern fnc log(x: double) -> double;
extern fnc log2(x: double) -> double;
extern fnc log10(x: double) -> double;
extern fnc exp(x: double) -> double;
extern fnc sin(x: double) -> double;
extern fnc cos(x: double) -> double;
extern fnc tan(x: double) -> double;
extern fnc asin(x: double) -> double;
extern fnc acos(x: double) -> double;
extern fnc atan(x: double) -> double;
extern fnc atan2(y: double, x: double) -> double;
extern fnc min_i32(a: int32, b: int32) -> int32;
extern fnc max_i32(a: int32, b: int32) -> int32;
extern fnc min_i64(a: int64, b: int64) -> int64;
extern fnc max_i64(a: int64, b: int64) -> int64;
extern fnc min_f64(a: double, b: double) -> double;
extern fnc max_f64(a: double, b: double) -> double;
extern fnc clamp_i32(v: int32, lo: int32, hi: int32) -> int32;
extern fnc clamp_f64(v: double, lo: double, hi: double) -> double;
)flux"},

// ─── string ───────────────────────────────────────────────────────────────────
{"string", R"flux(
extern fnc str_len(s: string) -> int32;
extern fnc str_is_empty(s: string) -> bool;
extern fnc str_to_upper(s: string) -> string;
extern fnc str_to_lower(s: string) -> string;
extern fnc str_contains(s: string, sub: string) -> bool;
extern fnc str_starts_with(s: string, prefix: string) -> bool;
extern fnc str_ends_with(s: string, suffix: string) -> bool;
extern fnc str_repeat(s: string, n: int32) -> string;
extern fnc str_trim(s: string) -> string;
extern fnc str_trim_start(s: string) -> string;
extern fnc str_trim_end(s: string) -> string;
extern fnc str_replace(s: string, from: string, to: string) -> string;
extern fnc str_concat(a: string, b: string) -> string;
extern fnc str_char_at(s: string, i: int32) -> int32;
extern fnc to_string_i32(n: int32) -> string;
extern fnc to_string_i64(n: int64) -> string;
extern fnc to_string_f64(n: double) -> string;
extern fnc to_string_bool(b: bool) -> string;
extern fnc parse_int(s: string) -> int32;
extern fnc parse_float(s: string) -> double;
)flux"},

}; // STDLIB_MODULES

} // namespace flux

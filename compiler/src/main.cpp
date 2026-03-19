#include "flux/lexer/lexer.h"
#include "flux/common/diagnostic.h"
#include <iostream>
#include <iomanip>

using namespace flux;

int main() {
    std::string src = R"(
        #import "./add.flx"
        fnc main() -> Result<int32, string> {
            let a: int32 = 5;
            let b: int32 = 4;
            let c: int32 = add(a, b);
            let d: int32; 
            add(a, b, &d);
            let e: &str = "abcd";
            let f: string = "abcd".to_string(); // Комментарий: &str ссылка (так же как и в языке Rust), а string уже в куче и является другим типом, так же &str по умолчанию ссылка
            let capacity = 4;
            let ab: int32[catacity] = [1, 45, 234, 64];
            for (let x: int8; x < 15; x++) {
                println(x);
            }
            i++;
            ++i;
            println(i < j);  // неявное приведение к большему типу если типы разные
            return Ok(0);
            match func {
                Ok(result) => println(result),
                Err(err) => println(err),
                _ => {println("unreachble")}
            };
        }
    )";

    DiagEngine diag;
    Lexer lexer(src, "<test>", diag);
    auto tokens = lexer.tokenize();

    for (auto& tok : tokens) {
        std::string_view name = token_kind_name(tok.kind);
        std::cout << std::left << std::setw(16) << name << tok.lexeme << "\n";
    }
}
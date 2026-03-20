#include "flux/compiler.h"
#include <iostream>

int main(int argc, char** argv) {
    auto opts = flux::parse_args(argc, argv);
    if (!opts) return 1;

    auto result = flux::compile(*opts);
    if (!result) {
        std::cerr << "Error: " << result.error << "\n";
        return 1;
    }
    return 0;
}

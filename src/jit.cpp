#include <cmath>
#include <cstdlib>
#include <iostream>

extern "C" {
#include "libtcc.h"
}

void handle_tcc_error(void* opaque, const char* msg) {
    fprintf(opaque, "%s\n", msg);
}

int main() {
    TCCState* tcc = tcc_new();
    if (tcc == nullptr) {
        std::cerr << "Failed to create TCC instance" << std::endl;
        return 1;
    }

    tcc_set_error_func(tcc, stderr, handle_tcc_error);
    assert(tcc_get_error_func(tcc) == handle_tcc_error);
    assert(tcc_get_error_opaque(tcc) == stderr);

    tcc_set_output_type(tcc, TCC_OUTPUT_MEMORY);

    const char* source =
        "void processArray(float* input, float* output, int size) {"
        "    for (int i = 0; i < size; ++i) {"
        "        output[i] = std::sqrt(input[i]);"
        "    }"
        "}";

    if (tcc_compile_string(tcc, source) == -1) {
        std::cerr << "Failed to compile code: " << tcc_get_error(tcc)
                  << std::endl;
        tcc_delete(tcc);
        return 1;
    }

    if (tcc_relocate(tcc, TCC_RELOCATE_AUTO) == -1) {
        std::cerr << "Failed to link code: " << tcc_get_error(tcc) << std::endl;
        tcc_delete(tcc);
        return 1;
    }

    auto processArray = reinterpret_cast<void (*)(float*, float*, int)>(
        tcc_get_symbol(tcc, "processArray"));
    if (!processArray) {
        std::cerr << "Failed to retrieve function pointer" << std::endl;
        tcc_delete(tcc);
        return 1;
    }

    // Example data
    constexpr int size = 5;
    float input[size] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float output[size] = {0.0f};

    processArray(input, output, size);

    std::cout << "Input: ";
    for (int i = 0; i < size; ++i) {
        std::cout << input[i] << " ";
    }
    std::cout << std::endl;

    std::cout << "Output: ";
    for (int i = 0; i < size; ++i) {
        std::cout << output[i] << " ";
    }
    std::cout << std::endl;

    tcc_delete(tcc);

    return 0;
}

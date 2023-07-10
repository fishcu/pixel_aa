#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

extern "C" {
#include "libtcc.h"
}

void handle_tcc_error(void* opaque, const char* msg) {
    fprintf(reinterpret_cast<FILE*>(opaque), "%s\n", msg);
}

std::string extract_parent_path(const std::string& path) {
    size_t found =
        path.find_last_of("/\\");  // Find the last slash or backslash
    if (found != std::string::npos) {
        return path.substr(0, found + 1);  // Extract the parent path
    } else {
        return "/";  // No slash or backslash found, return '/'
    }
}

int main(int argc, char* argv[]) {
    TCCState* tcc = tcc_new();
    if (tcc == nullptr) {
        std::cerr << "Failed to create TCC instance" << std::endl;
        return 1;
    }

    tcc_set_error_func(tcc, stderr, handle_tcc_error);
    assert(tcc_get_error_func(tcc) == handle_tcc_error);
    assert(tcc_get_error_opaque(tcc) == stderr);

    // TCC is not installed in the system. Make TCC find itself.
    for (int i = 1; i < argc; ++i) {
        char* a = argv[i];
        if (a[0] == '-') {
            if (a[1] == 'B')
                tcc_set_lib_path(tcc, a + 2);
            else if (a[1] == 'I')
                tcc_add_include_path(tcc, a + 2);
            else if (a[1] == 'L')
                tcc_add_library_path(tcc, a + 2);
        }
    }

    tcc_set_output_type(tcc, TCC_OUTPUT_MEMORY);

    const char* source =
        "#include <math.h>"
        "void processArray(float* input, float* output, int size) {"
        "    for (int i = 0; i < size; ++i) {"
        "        output[i] = sqrt(input[i]);"
        "    }"
        "}";

    if (tcc_compile_string(tcc, source) == -1) {
        tcc_delete(tcc);
        return 1;
    }

    if (tcc_relocate(tcc, TCC_RELOCATE_AUTO) == -1) {
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
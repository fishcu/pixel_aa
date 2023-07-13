#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// clang-format off
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
// clang-format on

#include <libtcc.h>

#include "string_manip.h"

typedef struct {
    char* str;
    int allocated_size;
    int used_size;
} DynamicString;

// Function to initialize the string
void init(DynamicString* str) {
    str->allocated_size = 2;
    str->used_size = 0;
    str->str = malloc(str->allocated_size * sizeof(char));
    str->str[0] = '\0';
}

// Function to add a const char* to the string
void add_string(DynamicString* str, const char* new_str) {
    int new_str_len = strlen(new_str);

    // Check if more memory is needed
    if (str->used_size + new_str_len >= str->allocated_size) {
        while (str->used_size + new_str_len >= str->allocated_size)
            str->allocated_size *= 2;

        // Reallocate memory with the new size
        str->str = realloc(str->str, str->allocated_size * sizeof(char));
    }

    // Add the new string to the dynamic string
    strcat(str->str, new_str);
    str->used_size += new_str_len;
}

void add_fmt_string(DynamicString* str, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    // Determine the size required for formatting
    int needed_size = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    // Check if more memory is needed
    if (str->used_size + needed_size >= str->allocated_size) {
        while (str->used_size + needed_size >= str->allocated_size)
            str->allocated_size *= 2;

        // Reallocate memory with the new size
        str->str = realloc(str->str, str->allocated_size * sizeof(char));
    }

    va_start(args, fmt);
    vsprintf(str->str + str->used_size, fmt, args);
    va_end(args);

    str->used_size += needed_size;
}

// Function to destroy the string and free the memory
void destroy(DynamicString* str) { free(str->str); }

void handle_tcc_error(void* opaque, const char* msg) {
    fprintf((FILE*)opaque, "%s\n", msg);
}

int main(int argc, char* argv[]) {
    // For perf. measurements
    struct timespec start, end;

    if (argc < 4) {
        printf("Usage: %s <input_path> <target_width> <target_height>\n",
               argv[0]);
        return 1;
    }

    const char* input_path = argv[1];
    int in_width, in_height, channels;
    unsigned char* in_img_data =
        stbi_load(input_path, &in_width, &in_height, &channels, STBI_rgb_alpha);
    if (!in_img_data) {
        printf("Failed to load image.\n");
        return 1;
    }
    if (channels != 3) {
        printf("Only 3 channel images are supported. Image has %d channels.\n",
               channels);
        return 1;
    }
    channels = 4;
    uint32_t* in = (uint32_t*)(in_img_data);

    printf("Image loaded successfully.\n");
    printf("Image width: %d\n", in_width);
    printf("Image height: %d\n", in_height);
    printf("Number of channels: %d\n", channels);

    // Allocate memory for the output image
    const int out_width = atoi(argv[2]);
    const int out_height = atoi(argv[3]);
    if (out_width < in_width || out_height < in_height) {
        printf("Error: Target size is smaller than the input image size.\n");
        stbi_image_free(in_img_data);
        return 1;
    }
    const int output_size = out_width * out_height * channels;
    unsigned char* out_img_data =
        (unsigned char*)malloc(output_size * sizeof(unsigned char));
    uint32_t* out = (uint32_t*)out_img_data;

    // JIT the image kernel
    TCCState* tcc = tcc_new();
    if (!tcc) {
        printf("Failed to create TCC instance!\n");
        return 1;
    }
    tcc_set_error_func(tcc, stderr, handle_tcc_error);
    assert(tcc_get_error_func(tcc) == handle_tcc_error);
    assert(tcc_get_error_opaque(tcc) == stderr);
    // Help TCC figure out where stuff is.
    // On a desktop system, in this configuration, we need at least:
    // -B./lib_tcc/x86_64 -L./lib_tcc/x86_64
    // On the MM, we need additionally to add -I and -L for libc and related
    // files.
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

    // Build the kernel source as string by emitting C source code.
    DynamicString source_code;
    init(&source_code);
    add_fmt_string(
        &source_code,
        "#include <stdint.h>\n"
        "void kernel(const uint32_t* restrict in, uint32_t* restrict out) {\n"
        "\tfor(int y = 0; y < %d; ++y) {\n"
        "\t\tconst int out_y_offset = y * %d;\n"
        "\t\tconst int in_y_offset = (int)(y * %ff) * %d;\n",
        out_height, out_width, (float)in_height / out_height, in_width);
    for (int x = 0; x < out_width; ++x) {
        int in_x = x * (float)in_width / out_width;
        add_fmt_string(&source_code, "\t\tout[out_y_offset + %d] = in[in_y_offset + %d];\n", x,
                       in_x);
    }
    add_string(&source_code,
               "\t}\n"
               "}\n");
    printf("%s", source_code.str);

    /*
    for (int y = 0; y < number; ++y) {
        const int out_y_offset = y * number;
        const int in_y_offset = y * float * number;
        out[out_y_offset + number] = in[in_y_offset + number];
        out[out_y_offset + number] = in[in_y_offset + number];
        out[out_y_offset + number] = in[in_y_offset + number];
        // ...
    }
    */

    printf("JIT compilation started...\n");
    clock_gettime(CLOCK_MONOTONIC, &start);

    if (tcc_compile_string(tcc, source_code.str) != 0) {
        printf("Failed to compile!\n");
        tcc_delete(tcc);
        return 1;
    }
    destroy(&source_code);
    if (tcc_add_library(tcc, "m") != 0) {
        printf("Failed to add library!\n");
        tcc_delete(tcc);
        return 1;
    }
    if (tcc_relocate(tcc, TCC_RELOCATE_AUTO) != 0) {
        printf("Failed to relocate!\n");
        tcc_delete(tcc);
        return 1;
    }
    void (*kernel)(const uint32_t* restrict, uint32_t* restrict);
    kernel = tcc_get_symbol(tcc, "kernel");
    if (!kernel) {
        printf("Failed to retrieve kernel function pointer!\n");
        tcc_delete(tcc);
        return 1;
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    int duration_ms = (end.tv_sec - start.tv_sec) * 1000 +
                      (end.tv_nsec - start.tv_nsec) / 1000000;
    printf("JIT compilation finished. Time elapsed: %d ms\n", duration_ms);

    // Measure performance

    const int num_perf_passes = 1000;
    clock_gettime(CLOCK_MONOTONIC, &start);

    // Iterate over all pixels in the output image
    for (int perf_pass = 0; perf_pass < num_perf_passes; ++perf_pass) {
        kernel(in, out);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    duration_ms = (end.tv_sec - start.tv_sec) * 1000 +
                  (end.tv_nsec - start.tv_nsec) / 1000000;
    printf("Time for %d passes: %d ms, that is %f ms per pass.\n",
           num_perf_passes, duration_ms, (float)duration_ms / num_perf_passes);

    // Save the resulting image
    char* directory = get_parent_path(input_path);
    char* file_name = get_filename(input_path);
    char* output_file_name = remove_extension(file_name);
    char* output_path = get_output_path(directory, output_file_name);
    printf("Saving output image to path: %s\n", output_path);

    if (stbi_write_png(output_path, out_width, out_height, channels,
                       out_img_data, out_width * channels) == 0) {
        printf("Failed to save the output image.\n");
        free(directory);
        free(file_name);
        free(output_file_name);
        free(output_path);
        tcc_delete(tcc);
        free(out_img_data);
        stbi_image_free(in_img_data);
        return 1;
    }

    printf("Output image saved successfully!\n");

    free(directory);
    free(file_name);
    free(output_file_name);
    free(output_path);
    tcc_delete(tcc);
    free(out_img_data);
    stbi_image_free(in_img_data);

    return 0;
}

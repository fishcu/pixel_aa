#include <stdint.h>
static inline void fill_row(const uint32_t* restrict in,
                            uint32_t* restrict out) {
    for (int x = 128; x > 0; --x) {
        *out++ = *in;
        *out++ = *in;
        *out++ = *in++;
        *out++ = *in;
        *out++ = *in++;
    }
}
void kernel(const uint32_t* restrict in_ptr, uint32_t* restrict out_ptr) {
    const uint32_t* restrict in = in_ptr;
    uint32_t* restrict out = out_ptr;
    for (int y = 32; y > 0; --y) {
        fill_row(in, out);
        out += 640;
        fill_row(in, out);
        out += 640;
        fill_row(in, out);
        out += 640;
        in += 256;
        fill_row(in, out);
        out += 640;
        fill_row(in, out);
        out += 640;
        in += 256;
        fill_row(in, out);
        out += 640;
        fill_row(in, out);
        out += 640;
        in += 256;
        fill_row(in, out);
        out += 640;
        fill_row(in, out);
        out += 640;
        in += 256;
        fill_row(in, out);
        out += 640;
        fill_row(in, out);
        out += 640;
        in += 256;
        fill_row(in, out);
        out += 640;
        fill_row(in, out);
        out += 640;
        in += 256;
        fill_row(in, out);
        out += 640;
        fill_row(in, out);
        out += 640;
        in += 256;
    }
}

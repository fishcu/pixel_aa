#include <stdint.h>
static inline void fill_row(const uint32_t* restrict in,
                            uint32_t* restrict out) {
    int x = 127;
    do {
        out[0] = in[0];
        out[1] = in[0];
        out[2] = in[0];
        out[3] = in[1];
        out[4] = in[1];
        out += 5;
        in += 2;
    } while (x--);
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

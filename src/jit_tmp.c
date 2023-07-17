#include <stdint.h>
static inline void fill_row(const uint32_t* restrict in, uint32_t* restrict out,
                            int out_y_offset, int in_y_offset) {
    for (int x = 0, in_x = 0; x < 640; x += 5, in_x += 2) {
        out[out_y_offset + x] = out[out_y_offset + x + 1] =
            out[out_y_offset + x + 2] = in[in_y_offset + in_x + 0];
        out[out_y_offset + x + 3] = out[out_y_offset + x + 4] =
            in[in_y_offset + in_x + 1];
    }
}
void kernel(const uint32_t* restrict in, uint32_t* restrict out) {
    for (int y = 0, out_y_offset = 0, in_y_offset = 0; y < 32; ++y) {
        fill_row(in, out, out_y_offset, in_y_offset);
        out_y_offset += 640;
        fill_row(in, out, out_y_offset, in_y_offset);
        out_y_offset += 640;
        fill_row(in, out, out_y_offset, in_y_offset);
        out_y_offset += 640;
        in_y_offset += 256;
        fill_row(in, out, out_y_offset, in_y_offset);
        out_y_offset += 640;
        fill_row(in, out, out_y_offset, in_y_offset);
        out_y_offset += 640;
        in_y_offset += 256;
        fill_row(in, out, out_y_offset, in_y_offset);
        out_y_offset += 640;
        fill_row(in, out, out_y_offset, in_y_offset);
        out_y_offset += 640;
        in_y_offset += 256;
        fill_row(in, out, out_y_offset, in_y_offset);
        out_y_offset += 640;
        fill_row(in, out, out_y_offset, in_y_offset);
        out_y_offset += 640;
        in_y_offset += 256;
        fill_row(in, out, out_y_offset, in_y_offset);
        out_y_offset += 640;
        fill_row(in, out, out_y_offset, in_y_offset);
        out_y_offset += 640;
        in_y_offset += 256;
        fill_row(in, out, out_y_offset, in_y_offset);
        out_y_offset += 640;
        fill_row(in, out, out_y_offset, in_y_offset);
        out_y_offset += 640;
        in_y_offset += 256;
        fill_row(in, out, out_y_offset, in_y_offset);
        out_y_offset += 640;
        fill_row(in, out, out_y_offset, in_y_offset);
        out_y_offset += 640;
        in_y_offset += 256;
    }
}

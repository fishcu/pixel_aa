#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// #define USE_OPENMP
#ifdef USE_OPENMP
#include "omp.h"
#endif

// clang-format off
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
// clang-format on

#include "string_manip.h"

#ifdef FIXED_POINT
// We need a data type that's at least 8 bits bigger than
// FIXED_POINT_BITS to handle multiplication with uchar and not overflow.
// We need a signed fixed point type to deal with mix operation containing a
// difference operation.
// #define FIXED_POINT
#define FIXED_POINT_BITS 8
typedef int16_t fixed_point_t;
typedef fixed_point_t weight_t;

inline fixed_point_t float_to_fixed(float value) {
    return (fixed_point_t)(value * (1 << FIXED_POINT_BITS));
}

inline fixed_point_t mix(fixed_point_t x, fixed_point_t y, fixed_point_t a) {
    return x + ((a * (y - x)) >> FIXED_POINT_BITS);
}

#define WEIGHT_TOL 1
#define WEIGHT_TOL_UPPER ((1 << FIXED_POINT_BITS) - WEIGHT_TOL)
#else  // !FIXED_POINT
typedef float weight_t;

inline float mix(float x, float y, float a) { return x + a * (y - x); }

#define WEIGHT_TOL 1.0e-2f
#define WEIGHT_TOL_UPPER (1.0f - WEIGHT_TOL)
#endif  // FIXED_POINT

inline float sign(float value) {
    if (value < 0.0f) {
        return -1.0f;
    } else if (value > 0.0f) {
        return 1.0f;
    } else {
        return 0.0f;
    }
}

// vec3 to_lin(vec3 x) { return pow(x, vec3(2.2)); }

// vec3 to_srgb(vec3 x) { return pow(x, vec3(1.0 / 2.2)); }

#define GET_CH(color, c) (((color) >> (8 * (2 - (c)))) & 0xFF)
#define GET_COL(r, g, b)                                              \
    (((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | ((uint32_t)(b)) | \
     0xFF << 24)

float smoothstep(float edge0, float edge1, float x) {
    float t = fmaxf(0.0, fminf(1.0, (x - edge0) / (edge1 - edge0)));
    return t * t * (3.0 - 2.0 * t);
}

float slopestep(float edge0, float edge1, float x, float slope) {
    x = fmaxf(0.0, fminf(1.0, (x - edge0) / (edge1 - edge0)));
    const float s = sign(x - 0.5f);
    const float o = (1.0f + s) * 0.5f;
    return o - 0.5f * s * pow(2.0f * (o - s * x), slope);
}

int main(int argc, char* argv[]) {
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

    // Stuff that's constant over the whole image
    // Iteration limits: For the first and last N pixels in each row and column,
    // we don't need to interpolate as we simply sample the border pixel from
    // the input image. This not just saves computations, but also allows us to
    // drop boundary checks throughout the sampling.
    // Derivation:
    /*
    in_x >= pixel transition start
    in_x >= 0.5f - in_x_step * 0.5f
    (0.5 + x) * s - 0.5 >= 0.5 - s * 0.5
    =>
    x >= 1 / s - 1
    */
    const int border_x = out_width >= in_width ? out_width / in_width - 1 : 0;
    const int border_y =
        out_height >= in_height ? out_height / in_height - 1 : 0;
    // Precompute interpolation weights
    // constexpr float sharpness = 1.5f;
    weight_t* weights_x = (weight_t*)malloc(out_width * sizeof(weight_t));
    weight_t* weights_y = (weight_t*)malloc(out_height * sizeof(weight_t));

    for (int x = 0, in_x_error = in_width / 2 - out_width / 2 - out_width;
         x < out_width; ++x, in_x_error += in_width) {
        if (in_x_error >= 0) {
            in_x_error -= out_width;
        }

        // To get phase back without calc. in_x_f, shift back by +out_width
        // again, and divide by out_width.
        const float phase = (float)(in_x_error + out_width) / out_width;

        const float in_x_step = (float)(in_width) / out_width;
#ifdef FIXED_POINT
        weights_x[x] = float_to_fixed(smoothstep(
            0.5f - in_x_step * 0.5f, 0.5f + in_x_step * 0.5f, phase));
#else   // !FIXED_POINT
        weights_x[x] =
            smoothstep(0.5f - in_x_step * 0.5f, 0.5f + in_x_step * 0.5f, phase);
#endif  // FIXED_POINT
    }
    for (int y = 0, in_y_error = in_height / 2 - out_height / 2 - out_height;
         y < out_height; ++y, in_y_error += in_height) {
        if (in_y_error >= 0) {
            in_y_error -= out_height;
        }
        const float phase = (float)(in_y_error + out_height) / out_height;

        const float in_y_step = (float)(in_height) / out_height;
#ifdef FIXED_POINT
        weights_y[y] = float_to_fixed(smoothstep(
            0.5f - in_y_step * 0.5f, 0.5f + in_y_step * 0.5f, phase));
#else   // !FIXED_POINT
        weights_y[y] =
            smoothstep(0.5f - in_y_step * 0.5f, 0.5f + in_y_step * 0.5f, phase);
#endif  // FIXED_POINT
    }

    // Measure performance
    struct timespec start, end;
    const int num_perf_passes = 1000;
    clock_gettime(CLOCK_MONOTONIC, &start);

    // Iterate over all pixels in the output image
    for (int perf_pass = 0; perf_pass < num_perf_passes; ++perf_pass) {
        // Top border, offset_y is effectively = 0
        for (int y = 0; y < border_y; ++y) {
            const int out_row_offset = y * out_width;

            // Top left corner, offset_x = 0
            for (int x = 0; x < border_x; ++x) {
                out[out_row_offset + x] = in[0];
            }

            // Middle part of top bar
            int in_x_error =
                in_width / 2 - out_width / 2 - out_width + in_width * border_x;
            uint32_t* in_ptr[2] = {in, in + 1};
            for (int x = border_x; x < out_width - border_x;
                 ++x, in_x_error += in_width) {
                // Update samples when we've moved enough.
                if (in_x_error >= 0) {
                    in_x_error -= out_width;
                    // Shift samples one to right.
                    ++in_ptr[0];
                    ++in_ptr[1];
                }

                const weight_t offset_x = weights_x[x];
                if (offset_x < WEIGHT_TOL) {
                    // Need 1 sample, no mixing
                    out[out_row_offset + x] = *in_ptr[0];
                } else if (offset_x > WEIGHT_TOL_UPPER) {
                    // Need 1 sample, no mixing
                    out[out_row_offset + x] = *in_ptr[1];
                } else {
                    // Need 2 samples, mix with offset_x
                    out[out_row_offset + x] =
                        GET_COL(mix(GET_CH(*in_ptr[0], 0),
                                    GET_CH(*in_ptr[1], 0), offset_x),
                                mix(GET_CH(*in_ptr[0], 1),
                                    GET_CH(*in_ptr[1], 1), offset_x),
                                mix(GET_CH(*in_ptr[0], 2),
                                    GET_CH(*in_ptr[1], 2), offset_x));
                }
            }

            // Top right corner, offset_x = 1
            for (int x = out_width - border_x; x < out_width; ++x) {
                out[out_row_offset + x] = in[in_width - 1];
            }
        }

#ifdef USE_OPENMP
#pragma omp parallel
#endif  // USE_OPENMP
        {
#ifdef USE_OPENMP
            int num_threads = omp_get_num_threads();
            int thread_num = omp_get_thread_num();
#else   // !USE_OPENMP
            int num_threads = 1;
            int thread_num = 0;
#endif  // USE_OPENMP

            int start_y = border_y + (out_height - border_y - border_y) *
                                         thread_num / num_threads;
            int end_y = border_y + (out_height - border_y - border_y) *
                                       (thread_num + 1) / num_threads;

            int in_y_error = ((in_height / 2 - out_height / 2 - out_height +
                               start_y * in_height + out_height) %
                              out_height) -
                             out_height;

            int in_start_y = (start_y * in_height + in_height / 2);
            in_start_y = in_start_y / out_height -
                         (in_start_y % out_height < out_height / 2 ? 1 : 0);
            int in_row_offset = in_start_y * in_width;
            for (int y = start_y; y < end_y; ++y, in_y_error += in_height) {
                // Shift input row when we've moved enough.
                if (in_y_error >= 0) {
                    in_y_error -= out_height;
                    in_row_offset += in_width;
                }

                const int out_row_offset = y * out_width;

                const weight_t offset_y = weights_y[y];

                // Left border, offset_x = 0
                uint32_t col;
                if (offset_y < WEIGHT_TOL) {
                    col = in[in_row_offset];
                } else if (offset_y > WEIGHT_TOL_UPPER) {
                    col = in[in_row_offset + in_width];
                } else {
                    col = GET_COL(
                        mix(GET_CH(in[in_row_offset], 0),
                            GET_CH(in[in_row_offset + in_width], 0), offset_y),
                        mix(GET_CH(in[in_row_offset], 1),
                            GET_CH(in[in_row_offset + in_width], 1), offset_y),
                        mix(GET_CH(in[in_row_offset], 2),
                            GET_CH(in[in_row_offset + in_width], 2), offset_y));
                }
                for (int x = 0; x < border_x; ++x) {
                    out[out_row_offset + x] = col;
                }

                // Keep all values relevant for interpolation in memory
                // and update them lazily.
                int in_x_error = in_width / 2 - out_width / 2 - out_width +
                                 in_width * border_x;
                uint32_t* in_ptr[4] = {in + in_row_offset,
                                       in + in_row_offset + 1,
                                       in + in_row_offset + in_width,
                                       in + in_row_offset + in_width + 1};
                // Center part of image
                for (int x = border_x; x < out_width - border_x;
                     ++x, in_x_error += in_width) {
                    // Update samples when we've moved enough.
                    if (in_x_error >= 0) {
                        in_x_error -= out_width;
                        // Shift samples one to right.
                        ++in_ptr[0];
                        ++in_ptr[1];
                        ++in_ptr[2];
                        ++in_ptr[3];
                    }

                    // Calc. and write output pixel
                    // Do a bilinear sampling with branching for often-occuring
                    // 0 and 1 weight samples.
                    const weight_t offset_x = weights_x[x];
                    if (offset_y < WEIGHT_TOL) {
                        if (offset_x < WEIGHT_TOL) {
                            // Need 1 sample, no mixing
                            out[out_row_offset + x] = *in_ptr[0];
                        } else if (offset_x > WEIGHT_TOL_UPPER) {
                            // Need 1 sample, no mixing
                            out[out_row_offset + x] = *in_ptr[1];
                        } else {
                            // Need 2 samples, mix with offset_x
                            out[out_row_offset + x] =
                                GET_COL(mix(GET_CH(*in_ptr[0], 0),
                                            GET_CH(*in_ptr[1], 0), offset_x),
                                        mix(GET_CH(*in_ptr[0], 1),
                                            GET_CH(*in_ptr[1], 1), offset_x),
                                        mix(GET_CH(*in_ptr[0], 2),
                                            GET_CH(*in_ptr[1], 2), offset_x));
                        }
                    } else if (offset_y > WEIGHT_TOL_UPPER) {
                        if (offset_x < WEIGHT_TOL) {
                            // Need 1 sample, no mixing
                            out[out_row_offset + x] = *in_ptr[2];
                        } else if (offset_x > WEIGHT_TOL_UPPER) {
                            // Need 1 sample, no mixing
                            out[out_row_offset + x] = *in_ptr[3];
                        } else {
                            // Need 2 samples, mix with offset_x
                            out[out_row_offset + x] =
                                GET_COL(mix(GET_CH(*in_ptr[2], 0),
                                            GET_CH(*in_ptr[3], 0), offset_x),
                                        mix(GET_CH(*in_ptr[2], 1),
                                            GET_CH(*in_ptr[3], 1), offset_x),
                                        mix(GET_CH(*in_ptr[2], 2),
                                            GET_CH(*in_ptr[3], 2), offset_x));
                        }
                    } else {
                        if (offset_x < WEIGHT_TOL) {
                            // Need 2 samples, mix with offset_y
                            out[out_row_offset + x] =
                                GET_COL(mix(GET_CH(*in_ptr[0], 0),
                                            GET_CH(*in_ptr[2], 0), offset_y),
                                        mix(GET_CH(*in_ptr[0], 1),
                                            GET_CH(*in_ptr[2], 1), offset_y),
                                        mix(GET_CH(*in_ptr[0], 2),
                                            GET_CH(*in_ptr[2], 2), offset_y));
                        } else if (offset_x > WEIGHT_TOL_UPPER) {
                            // Need 2 samples, mix with offset_y
                            out[out_row_offset + x] =
                                GET_COL(mix(GET_CH(*in_ptr[1], 0),
                                            GET_CH(*in_ptr[3], 0), offset_y),
                                        mix(GET_CH(*in_ptr[1], 1),
                                            GET_CH(*in_ptr[3], 1), offset_y),
                                        mix(GET_CH(*in_ptr[1], 2),
                                            GET_CH(*in_ptr[3], 2), offset_y));
                        } else {
                            // Need 4 samples, mix with offset_x and offset_y
                            out[out_row_offset + x] = GET_COL(
                                mix(mix(GET_CH(*in_ptr[0], 0),
                                        GET_CH(*in_ptr[1], 0), offset_x),
                                    mix(GET_CH(*in_ptr[2], 0),
                                        GET_CH(*in_ptr[3], 0), offset_x),
                                    offset_y),
                                mix(mix(GET_CH(*in_ptr[0], 1),
                                        GET_CH(*in_ptr[1], 1), offset_x),
                                    mix(GET_CH(*in_ptr[2], 1),
                                        GET_CH(*in_ptr[3], 1), offset_x),
                                    offset_y),
                                mix(mix(GET_CH(*in_ptr[0], 2),
                                        GET_CH(*in_ptr[1], 2), offset_x),
                                    mix(GET_CH(*in_ptr[2], 2),
                                        GET_CH(*in_ptr[3], 2), offset_x),
                                    offset_y));
                        }
                    }
                }

                // Right border, offset_x = 1
                if (offset_y < WEIGHT_TOL) {
                    col = in[in_row_offset + in_width - 1];
                } else if (offset_y > WEIGHT_TOL_UPPER) {
                    col = in[in_row_offset + 2 * in_width - 1];
                } else {
                    col = GET_COL(
                        mix(GET_CH(in[in_row_offset + in_width - 1], 0),
                            GET_CH(in[in_row_offset + 2 * in_width - 1], 0),
                            offset_y),
                        mix(GET_CH(in[in_row_offset + in_width - 1], 1),
                            GET_CH(in[in_row_offset + 2 * in_width - 1], 1),
                            offset_y),
                        mix(GET_CH(in[in_row_offset + in_width - 1], 2),
                            GET_CH(in[in_row_offset + 2 * in_width - 1], 2),
                            offset_y));
                }
                for (int x = out_width - border_x; x < out_width; ++x) {
                    out[out_row_offset + x] = col;
                }
            }
        }

        // Bottom border, offset_y is effectively = 1
        const int in_row_offset = (in_height - 1) * in_width;
        for (int y = out_height - border_y; y < out_height; ++y) {
            const int out_row_offset = y * out_width;

            // Bottom left corner, offset_x = 0
            for (int x = 0; x < border_x; ++x) {
                out[out_row_offset + x] = in[in_row_offset];
            }

            // Middle part of bottom bar
            int in_x_error =
                in_width / 2 - out_width / 2 - out_width + in_width * border_x;
            uint32_t* in_ptr[2] = {in + in_row_offset, in + in_row_offset + 1};
            for (int x = border_x; x < out_width - border_x;
                 ++x, in_x_error += in_width) {
                // Update samples when we've moved enough.
                if (in_x_error >= 0) {
                    in_x_error -= out_width;
                    // Shift samples one to right.
                    ++in_ptr[0];
                    ++in_ptr[1];
                }

                const weight_t offset_x = weights_x[x];
                if (offset_x < WEIGHT_TOL) {
                    // Need 1 sample, no mixing
                    out[out_row_offset + x] = *in_ptr[0];
                } else if (offset_x > WEIGHT_TOL_UPPER) {
                    // Need 1 sample, no mixing
                    out[out_row_offset + x] = *in_ptr[1];
                } else {
                    // Need 2 samples, mix with offset_x
                    out[out_row_offset + x] =
                        GET_COL(mix(GET_CH(*in_ptr[0], 0),
                                    GET_CH(*in_ptr[1], 0), offset_x),
                                mix(GET_CH(*in_ptr[0], 1),
                                    GET_CH(*in_ptr[1], 1), offset_x),
                                mix(GET_CH(*in_ptr[0], 2),
                                    GET_CH(*in_ptr[1], 2), offset_x));
                }
            }

            // Bottom right corner, offset_x = 1
            for (int x = out_width - border_x; x < out_width; ++x) {
                out[out_row_offset + x] = in[in_row_offset + in_width - 1];
            }
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    long duration_ms = (end.tv_sec - start.tv_sec) * 1000 +
                       (end.tv_nsec - start.tv_nsec) / 1000000;
    printf("Time for %d passes: %ld ms, that is %f ms per pass.\n",
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
        free(weights_x);
        free(weights_y);
        free(out_img_data);
        stbi_image_free(in_img_data);
        return 1;
    }

    printf("Output image saved successfully!\n");

    free(directory);
    free(file_name);
    free(output_file_name);
    free(output_path);
    free(weights_x);
    free(weights_y);
    free(out_img_data);
    stbi_image_free(in_img_data);

    return 0;
}

#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
// #include <condition_variable>

// #include "omp.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

inline float sign(float value) {
    if (value < 0.0f) {
        return -1.0f;
    } else if (value > 0.0f) {
        return 1.0f;
    } else {
        return 0.0f;
    }
}

inline float mix(float x, float y, float a) { return x + a * (y - x); }

// vec3 to_lin(vec3 x) { return pow(x, vec3(2.2)); }

// vec3 to_srgb(vec3 x) { return pow(x, vec3(1.0 / 2.2)); }

#define OFFSET_TOL 1.0e-2f

#define GET_CH(color, c) ((unsigned char)(((color) >> (8 * (2 - (c)))) & 0xFF))
#define GET_COL(r, g, b)                                              \
    (((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | ((uint32_t)(b)) | \
     0xff << 24)

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

void rasterize_center(uint32_t *in, uint32_t *out, int in_pitch, int out_pitch,
                      int in_width, int out_width, int border_x,  //
                      int begin_y, int end_y,                     //
                      float *weights_x, float *weights_y,         //
                      float in_x_step, float in_y_step) {
    for (int y = begin_y; y < end_y; ++y) {
        const int out_row_offset = y * out_pitch;
        const float in_y = (y + 0.5f) * in_y_step - 0.5f;
        const int in_row_offset = int(in_y) * in_pitch;

        const float offset_y = weights_y[y];

        // Left border, offset_x = 0
        uint32_t col;
        if (offset_y < OFFSET_TOL) {
            col = in[in_row_offset];
        } else if (offset_y > 1.0f - OFFSET_TOL) {
            col = in[in_row_offset + in_pitch];
        } else {
            col =
                GET_COL(mix(GET_CH(in[in_row_offset], 0),
                            GET_CH(in[in_row_offset + in_pitch], 0), offset_y),
                        mix(GET_CH(in[in_row_offset], 1),
                            GET_CH(in[in_row_offset + in_pitch], 1), offset_y),
                        mix(GET_CH(in[in_row_offset], 2),
                            GET_CH(in[in_row_offset + in_pitch], 2), offset_y));
        }
        for (int x = 0; x < border_x; ++x) {
            out[out_row_offset + x] = col;
        }

        // Keep all values relevant for interpolation in memory
        // and update them lazily.
        float in_x = (border_x + 0.5f) * in_x_step - 0.5f;
        int in_sample_x = int(in_x);
        uint32_t *in_ptr[4] = {in + in_row_offset + in_sample_x,
                               in + in_row_offset + in_sample_x + 1,
                               in + in_row_offset + in_pitch + in_sample_x,
                               in + in_row_offset + in_pitch + in_sample_x + 1};
        // Center part of image
        for (int x = border_x; x < out_width - border_x;
             ++x, in_x += in_x_step) {
            // Update samples when we've moved enough.
            if (int(in_x) > in_sample_x) {
                in_sample_x = int(in_x);
                // Shift samples one to right.
                ++in_ptr[0];
                ++in_ptr[1];
                ++in_ptr[2];
                ++in_ptr[3];
            }

            // Calc. and write output pixel
            // Do a bilinear sampling with branching for often-occuring 0
            // and 1 weight samples.
            const float offset_x = weights_x[x];
            if (offset_y < OFFSET_TOL) {
                if (offset_x < OFFSET_TOL) {
                    // Need 1 sample, no mixing
                    out[out_row_offset + x] = *in_ptr[0];
                } else if (offset_x > 1.0f - OFFSET_TOL) {
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
            } else if (offset_y > 1.0f - OFFSET_TOL) {
                if (offset_x < OFFSET_TOL) {
                    // Need 1 sample, no mixing
                    out[out_row_offset + x] = *in_ptr[2];
                } else if (offset_x > 1.0f - OFFSET_TOL) {
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
                if (offset_x < OFFSET_TOL) {
                    // Need 2 samples, mix with offset_y
                    out[out_row_offset + x] =
                        GET_COL(mix(GET_CH(*in_ptr[0], 0),
                                    GET_CH(*in_ptr[2], 0), offset_y),
                                mix(GET_CH(*in_ptr[0], 1),
                                    GET_CH(*in_ptr[2], 1), offset_y),
                                mix(GET_CH(*in_ptr[0], 2),
                                    GET_CH(*in_ptr[2], 2), offset_y));
                } else if (offset_x > 1.0f - OFFSET_TOL) {
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
                    out[out_row_offset + x] =
                        GET_COL(mix(mix(GET_CH(*in_ptr[0], 0),
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
        if (offset_y < OFFSET_TOL) {
            col = in[in_row_offset + in_width - 1];
        } else if (offset_y > 1.0f - OFFSET_TOL) {
            col = in[in_row_offset + in_pitch + in_width - 1];
        } else {
            col = GET_COL(
                mix(GET_CH(in[in_row_offset + in_width - 1], 0),
                    GET_CH(in[in_row_offset + in_pitch + in_width - 1], 0),
                    offset_y),
                mix(GET_CH(in[in_row_offset + in_width - 1], 1),
                    GET_CH(in[in_row_offset + in_pitch + in_width - 1], 1),
                    offset_y),
                mix(GET_CH(in[in_row_offset + in_width - 1], 2),
                    GET_CH(in[in_row_offset + in_pitch + in_width - 1], 2),
                    offset_y));
        }
        for (int x = out_width - border_x; x < out_width; ++x) {
            out[out_row_offset + x] = col;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage: %s <input_path> <target_width> <target_height>\n",
               argv[0]);
        return 1;
    }

    const std::string input_path = argv[1];
    int in_width, in_height, channels;
    unsigned char *in_img_data_uchar = stbi_load(
        input_path.c_str(), &in_width, &in_height, &channels, STBI_rgb_alpha);
    if (in_img_data_uchar == nullptr) {
        printf("Failed to load image.\n");
        return 1;
    }
    if (channels != 3) {
        printf("Only 3 channel images are supported. Image has %d channels.\n",
               channels);
        return 1;
    }
    channels = 4;
    uint32_t *in_img_data = reinterpret_cast<uint32_t *>(in_img_data_uchar);

    printf("Image loaded successfully.\n");
    printf("Image width: %d\n", in_width);
    printf("Image height: %d\n", in_height);
    printf("Number of channels: %d\n", channels);

    // Allocate memory for the output image
    const int out_width = std::stoi(argv[2]);
    const int out_height = std::stoi(argv[3]);
    if (out_width < in_width || out_height < in_height) {
        printf("Error: Target size is smaller than the input image size.\n");
        stbi_image_free(in_img_data_uchar);
        return 1;
    }
    const int output_size = out_width * out_height * channels;
    std::unique_ptr<unsigned char[]> out_img_data(
        new unsigned char[output_size]);
    uint32_t *out = reinterpret_cast<uint32_t *>(out_img_data.get());

    // Thread pool
    std::vector<std::thread> threads;
    // const int num_threads = 4;
    const int num_threads = std::thread::hardware_concurrency();
    threads.reserve(num_threads);
    // std::condition_variable cv;
    // int num_running_threads;
    // std::mutex num_running_threads_mtx;

    // Stuff that's constant over the whole image
    const float in_x_step = static_cast<float>(in_width) / out_width;
    const float in_y_step = static_cast<float>(in_height) / out_height;
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
    std::unique_ptr<float[]> weights_x(new float[out_width]);
    std::unique_ptr<float[]> weights_y(new float[out_height]);
    float in_x = 0.5f * in_x_step - 0.5f;
    for (int x = 0; x < out_width; ++x, in_x += in_x_step) {
        const float phase = in_x - int(in_x);
        weights_x[x] =
            smoothstep(0.5f - in_x_step * 0.5f, 0.5f + in_x_step * 0.5f, phase);
    }
    for (int y = 0; y < out_height; ++y) {
        const float in_y = (y + 0.5f) * in_y_step - 0.5f;
        const float phase = in_y - int(in_y);
        weights_y[y] =
            smoothstep(0.5f - in_y_step * 0.5f, 0.5f + in_y_step * 0.5f, phase);
    }

    // Measure performance
    const auto start = std::chrono::high_resolution_clock::now();
    constexpr int num_perf_passes = 1000;

    // Iterate over all pixels in the output image
    for (int perf_pass = 0; perf_pass < num_perf_passes; ++perf_pass) {
        // Reset and launch worker threads
        // num_running_threads = threads.size();
        /*
          uint32_t *in, uint32_t *out, int in_pitch, int out_pitch,
                          int in_width, int out_width, int border_x, //
                          int begin_y, int end_y,                    //
                          float *weights_x, float *weights_y,        //
                          float in_x_step, float in_y_step
        */
        threads.clear();
        for (int i = 0; i < num_threads; ++i) {
            int begin_y =
                border_y + (out_height - 2 * border_y) * i / num_threads;
            int end_y =
                border_y + (out_height - 2 * border_y) * (i + 1) / num_threads;
            // printf("pass %d\nlaunching thread %d, from %d to %d\n",
            // perf_pass, i,
            //        begin_y, end_y);
            threads.emplace_back(rasterize_center, in_img_data, out, in_width,
                                 out_width, in_width, out_width, border_x,
                                 begin_y, end_y, weights_x.get(),
                                 weights_y.get(), in_x_step, in_y_step);
        }

        // Top border, offset_y is effectively = 0
        for (int y = 0; y < border_y; ++y) {
            const int out_row_offset = y * out_width;

            // Top left corner, offset_x = 0
            for (int x = 0; x < border_x; ++x) {
                out[out_row_offset + x] = in_img_data[0];
            }

            // Middle part of top bar
            float in_x = (border_x + 0.5f) * in_x_step - 0.5f;
            int in_sample_x = int(in_x);
            uint32_t *in_ptr[2] = {in_img_data + in_sample_x,
                                   in_img_data + in_sample_x + 1};
            for (int x = border_x; x < out_width - border_x;
                 ++x, in_x += in_x_step) {
                // Update samples when we've moved enough.
                if (int(in_x) > in_sample_x) {
                    in_sample_x = int(in_x);
                    // Shift samples one to right.
                    ++in_ptr[0];
                    ++in_ptr[1];
                }

                const float offset_x = weights_x[x];
                if (offset_x < OFFSET_TOL) {
                    // Need 1 sample, no mixing
                    out[out_row_offset + x] = *in_ptr[0];
                } else if (offset_x > 1.0f - OFFSET_TOL) {
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
                out[out_row_offset + x] = in_img_data[in_width - 1];
            }
        }

        // Center of image is handled by worker threads.

        // Bottom border, offset_y is effectively = 1
        for (int y = out_height - border_y; y < out_height; ++y) {
            const int out_row_offset = y * out_width;
            const int in_row_offset = (in_height - 1) * in_width;

            // Bottom left corner, offset_x = 0
            for (int x = 0; x < border_x; ++x) {
                out[out_row_offset + x] = in_img_data[in_row_offset];
            }

            // Middle part of top bar
            float in_x = (border_x + 0.5f) * in_x_step - 0.5f;
            int in_sample_x = int(in_x);
            uint32_t *in_ptr[2] = {
                in_img_data + in_row_offset + in_sample_x,
                in_img_data + in_row_offset + in_sample_x + 1};
            for (int x = border_x; x < out_width - border_x;
                 ++x, in_x += in_x_step) {
                // Update samples when we've moved enough.
                if (int(in_x) > in_sample_x) {
                    in_sample_x = int(in_x);
                    // Shift samples one to right.
                    ++in_ptr[0];
                    ++in_ptr[1];
                }

                const float offset_x = weights_x[x];
                if (offset_x < OFFSET_TOL) {
                    // Need 1 sample, no mixing
                    out[out_row_offset + x] = *in_ptr[0];
                } else if (offset_x > 1.0f - OFFSET_TOL) {
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
                out[out_row_offset + x] =
                    in_img_data[in_row_offset + in_width - 1];
            }
        }

        // Wait until work threads are done
        for (auto &thread : threads) {
            thread.join();
        }
    }

    const auto end = std::chrono::high_resolution_clock::now();
    const auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    printf("Time for %d passes: %ld ms, that is %f ms per pass.\n",
           num_perf_passes, duration.count(),
           static_cast<float>(duration.count()) / num_perf_passes);

    // Save the resulting image
    // Get the directory path and file name from the input file path
    std::string directory =
        input_path.substr(0, input_path.find_last_of("/\\"));
    std::string file_name =
        input_path.substr(input_path.find_last_of("/\\") + 1);

    // Remove the original file extension from the file name
    std::string output_file_name =
        file_name.substr(0, file_name.find_last_of('.'));
    std::string output_path =
        directory + "/" + output_file_name + "_output.png";

    if (stbi_write_png(output_path.c_str(), out_width, out_height, channels,
                       out_img_data.get(), out_width * channels) == 0) {
        printf("Failed to save the output image.\n");
        stbi_image_free(in_img_data);
        return 1;
    }

    printf("Output image saved successfully: %s\n", output_path.c_str());

    stbi_image_free(in_img_data);

    return 0;
}

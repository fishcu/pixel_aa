#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>

// #include "glm/glm.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// using namespace glm;

template <typename T>
inline T clamp(T value, T min, T max) {
    if (value < min) {
        return min;
    } else if (value > max) {
        return max;
    } else {
        return value;
    }
}

inline float sign(float value) {
    if (value < 0.0f) {
        return -1.0f;
    } else if (value > 0.0f) {
        return 1.0f;
    } else {
        return 0.0f;
    }
}

inline float mix(float x, float y, float a) { return x * (1.0 - a) + y * a; }

// vec3 to_lin(vec3 x) { return pow(x, vec3(2.2)); }

// vec3 to_srgb(vec3 x) { return pow(x, vec3(1.0 / 2.2)); }

#define OFFSET_TOL 1.0e-2f

#define GET_CHANNEL(color, c) \
    ((unsigned char)(((color) >> (8 * (2 - (c)))) & 0xFF))
#define MAKE_COLOR(r, g, b)                                           \
    (((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | ((uint32_t)(b)) | \
     0xff << 24)

float smoothstep(float edge0, float edge1, float x) {
    float t = fmaxf(0.0, fminf(1.0, (x - edge0) / (edge1 - edge0)));
    return t * t * (3.0 - 2.0 * t);
}

float slopestep(float edge0, float edge1, float x, float slope) {
    x = clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
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

    const std::string input_path = argv[1];
    int in_width, in_height, channels;
    unsigned char* in_img_data_uchar = stbi_load(
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
    uint32_t* in_img_data = reinterpret_cast<uint32_t*>(in_img_data_uchar);
    const int in_img_size = in_width * in_height;

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
    uint32_t* out = reinterpret_cast<uint32_t*>(out_img_data.get());

    // Stuff that's constant over the whole image
    const float in_x_step = static_cast<float>(in_width) / out_width;
    const float in_y_step = static_cast<float>(in_height) / out_height;

    // Precompute interpolation weights
    constexpr float sharpness = 1.5f;
    std::unique_ptr<float[]> weights_x(new float[out_width]);
    std::unique_ptr<float[]> weights_y(new float[out_height]);
    float in_x = 0.5f * in_x_step - 0.5f;
    for (int x = 0; x < out_width; ++x, in_x += in_x_step) {
        // const float in_x = (x + 0.5f) * in_x_step - 0.5f;
        const float phase = in_x - int(in_x);
        weights_x[x] =
            smoothstep(0.5f - in_x_step * 0.5f, 0.5f + in_x_step * 0.5f, phase);
    }
    float in_y = 0.5f * in_y_step - 0.5f;
    for (int y = 0; y < out_height; ++y, in_y += in_y_step) {
        // float in_y = (0.5f + y) * in_y_step - 0.5f;
        const float phase = in_y - int(in_y);
        weights_y[y] =
            smoothstep(0.5f - in_y_step * 0.5f, 0.5f + in_y_step * 0.5f, phase);
    }

    // Measure performance
    const auto start = std::chrono::high_resolution_clock::now();
    constexpr int num_perf_passes = 500;

    // Iterate over all pixels in the output image
    for (int perf_pass = 0; perf_pass < num_perf_passes; ++perf_pass) {
        float in_y = 0.5f * in_y_step - 0.5f;
        for (int y = 0; y < out_height; ++y, in_y += in_y_step) {
            const int out_row_offset = y * out_width;

            const int in_row_offset = int(in_y) * in_width;
            const float offset_y = weights_y[y];

            float in_x = 0.5f * in_x_step - 0.5f;

            // Keep 4 values relevant for interpolation in memory
            // uint32_t val[4];
            // int in_sample_x = 0;
            for (int x = 0; x < out_width; ++x, in_x += in_x_step) {
                const float offset_x = weights_x[x];

                // Calc. and write output pixel
                // Do a bilinear sampling with branching for often-occuring 0
                // and 1 weight samples.
                const int base_in_idx = in_row_offset + int(in_x);
                const int out_idx = out_row_offset + x;
                if (offset_y < OFFSET_TOL) {
                    if (offset_x < OFFSET_TOL) {
                        // Need 1 sample, no mixing
                        out[out_idx] =
                            in_img_data[clamp(base_in_idx, 0, in_img_size - 1)];
                    } else if (offset_x > 1.0f - OFFSET_TOL) {
                        // Need 1 sample, no mixing
                        out[out_idx] = in_img_data[clamp(base_in_idx + 1, 0,
                                                         in_img_size - 1)];
                    } else {
                        // Need 2 samples, mix with offset_x
                        const uint32_t val[] = {
                            in_img_data[clamp(base_in_idx, 0, in_img_size - 1)],
                            in_img_data[clamp(base_in_idx + 1, 0,
                                              in_img_size - 1)]};
                        out[out_idx] =
                            MAKE_COLOR(mix(GET_CHANNEL(val[0], 0),
                                           GET_CHANNEL(val[1], 0), offset_x),
                                       mix(GET_CHANNEL(val[0], 1),
                                           GET_CHANNEL(val[1], 1), offset_x),
                                       mix(GET_CHANNEL(val[0], 2),
                                           GET_CHANNEL(val[1], 2), offset_x));
                    }
                } else if (offset_y > 1.0f - OFFSET_TOL) {
                    if (offset_x < OFFSET_TOL) {
                        // Need 1 sample, no mixing
                        out[out_idx] = in_img_data[clamp(base_in_idx + in_width,
                                                         0, in_img_size - 1)];
                    } else if (offset_x > 1.0f - OFFSET_TOL) {
                        // Need 1 sample, no mixing
                        out[out_idx] = in_img_data[clamp(
                            base_in_idx + in_width + 1, 0, in_img_size - 1)];
                    } else {
                        // Need 2 samples, mix with offset_x
                        const uint32_t val[] = {
                            in_img_data[clamp(base_in_idx + in_width, 0,
                                              in_img_size - 1)],
                            in_img_data[clamp(base_in_idx + in_width + 1, 0,
                                              in_img_size - 1)]};
                        out[out_idx] =
                            MAKE_COLOR(mix(GET_CHANNEL(val[0], 0),
                                           GET_CHANNEL(val[1], 0), offset_x),
                                       mix(GET_CHANNEL(val[0], 1),
                                           GET_CHANNEL(val[1], 1), offset_x),
                                       mix(GET_CHANNEL(val[0], 2),
                                           GET_CHANNEL(val[1], 2), offset_x));
                    }
                } else {
                    if (offset_x < OFFSET_TOL) {
                        // Need 2 samples, mix with offset_y
                        const uint32_t val[] = {
                            in_img_data[clamp(base_in_idx, 0, in_img_size - 1)],
                            in_img_data[clamp(base_in_idx + in_width, 0,
                                              in_img_size - 1)]};
                        out[out_idx] =
                            MAKE_COLOR(mix(GET_CHANNEL(val[0], 0),
                                           GET_CHANNEL(val[1], 0), offset_y),
                                       mix(GET_CHANNEL(val[0], 1),
                                           GET_CHANNEL(val[1], 1), offset_y),
                                       mix(GET_CHANNEL(val[0], 2),
                                           GET_CHANNEL(val[1], 2), offset_y));
                    } else if (offset_x > 1.0f - OFFSET_TOL) {
                        // Need 2 samples, mix with offset_y
                        const uint32_t val[] = {
                            in_img_data[clamp(base_in_idx + 1, 0,
                                              in_img_size - 1)],
                            in_img_data[clamp(base_in_idx + 1 + in_width, 0,
                                              in_img_size - 1)]};
                        out[out_idx] =
                            MAKE_COLOR(mix(GET_CHANNEL(val[0], 0),
                                           GET_CHANNEL(val[1], 0), offset_y),
                                       mix(GET_CHANNEL(val[0], 1),
                                           GET_CHANNEL(val[1], 1), offset_y),
                                       mix(GET_CHANNEL(val[0], 2),
                                           GET_CHANNEL(val[1], 2), offset_y));
                    } else {
                        // Need 4 samples, mix with offset_x and offset_y
                        const uint32_t val[] = {
                            in_img_data[clamp(base_in_idx, 0, in_img_size - 1)],
                            in_img_data[clamp(base_in_idx + 1, 0,
                                              in_img_size - 1)],
                            in_img_data[clamp(base_in_idx + in_width, 0,
                                              in_img_size - 1)],
                            in_img_data[clamp(base_in_idx + in_width + 1, 0,
                                              in_img_size - 1)],
                        };
                        out[out_idx] = MAKE_COLOR(
                            mix(mix(GET_CHANNEL(val[0], 0),
                                    GET_CHANNEL(val[1], 0), offset_x),
                                mix(GET_CHANNEL(val[2], 0),
                                    GET_CHANNEL(val[3], 0), offset_x),
                                offset_y),
                            mix(mix(GET_CHANNEL(val[0], 1),
                                    GET_CHANNEL(val[1], 1), offset_x),
                                mix(GET_CHANNEL(val[2], 1),
                                    GET_CHANNEL(val[3], 1), offset_x),
                                offset_y),
                            mix(mix(GET_CHANNEL(val[0], 2),
                                    GET_CHANNEL(val[1], 2), offset_x),
                                mix(GET_CHANNEL(val[2], 2),
                                    GET_CHANNEL(val[3], 2), offset_x),
                                offset_y));
                    }
                }
            }
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

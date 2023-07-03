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

inline int clamp(int x, int min_val, int max_val) {
    if (x < min_val)
        return min_val;
    else if (x > max_val)
        return max_val;
    else
        return x;
}

inline float mix(float x, float y, float a) { return x * (1.0 - a) + y * a; }

// vec3 to_lin(vec3 x) { return pow(x, vec3(2.2)); }

// vec3 to_srgb(vec3 x) { return pow(x, vec3(1.0 / 2.2)); }

#define OFFSET_TOL 1.0e-4f

int main(int argc, char* argv[]) {
    if (argc < 4) {
        printf("Usage: %s <input_path> <target_width> <target_height>\n",
               argv[0]);
        return 1;
    }

    const std::string input_path = argv[1];
    int in_width, in_height, channels;
    unsigned char* in_img_data =
        stbi_load(input_path.c_str(), &in_width, &in_height, &channels, 0);
    if (in_img_data == nullptr) {
        printf("Failed to load image.\n");
        return 1;
    }
    if (channels != 3) {
        printf("Only 3 channel images are supported. Image has %d channels.\n",
               channels);
        return 1;
    }
    const int in_img_bytes = in_width * in_height * channels;

    printf("Image loaded successfully.\n");
    printf("Image width: %d\n", in_width);
    printf("Image height: %d\n", in_height);
    printf("Number of channels: %d\n", channels);

    // Allocate memory for the output image
    const int out_width = std::stoi(argv[2]);
    const int out_height = std::stoi(argv[3]);
    if (out_width < in_width || out_height < in_height) {
        printf("Error: Target size is smaller than the input image size.\n");
        stbi_image_free(in_img_data);
        return 1;
    }
    const int output_size = out_width * out_height * channels;
    std::unique_ptr<unsigned char[]> out_img_data(
        new unsigned char[output_size]);

    // Measure performance
    const auto start = std::chrono::high_resolution_clock::now();
    constexpr int num_perf_passes = 100;

    // Iterate over all pixels in the output image
    for (int perf_pass = 0; perf_pass < num_perf_passes; ++perf_pass) {
        // Stuff that's constant over the whole image
        const float filter_width = static_cast<float>(out_width) / in_width;
        const float filter_height = static_cast<float>(out_height) / in_height;
        const float in_x_step = static_cast<float>(in_width) / out_width;
        const float in_y_step = static_cast<float>(in_height) / out_height;
        const float transition_start_x = 0.5f - 0.5f * in_x_step;
        const float transition_start_y = 0.5f - 0.5f * in_y_step;

        const int in_neighbor_offsets[] = {channels, in_width * channels,
                                           in_width * channels + channels};

        float in_y = 0.5f * in_y_step - 0.5f;
        for (int y = 0; y < out_height; ++y) {
            const int out_row_offset = y * out_width * channels;

            const int period_y = in_y;
            const float phase_y = in_y - period_y;
            const float s = fmaxf(
                0.0f,
                fminf(1.0f, (phase_y - transition_start_y) * filter_height));
            const float offset_y = s * s * (3.0f - 2.0f * s);

            const int in_row_offset = period_y * in_width * channels;

            float in_x = 0.5f * in_x_step - 0.5f;
            for (int x = 0; x < out_width; ++x) {
                const int period_x = in_x;
                const float phase_x = in_x - period_x;
                const float t = fmaxf(
                    0.0f,
                    fminf(1.0f, (phase_x - transition_start_x) * filter_width));
                const float offset_x = t * t * (3.0f - 2.0f * t);

                // Calc. and write output pixel
                // Do a bilinear sampling with branching for often-occuring 0
                // and 1 weight samples.
                const int base_in_idx = in_row_offset + period_x * channels;
                const int base_out_idx = out_row_offset + x * channels;

                if (offset_y < OFFSET_TOL || offset_y > 1.0f - OFFSET_TOL) {
                    const int extra_in_idx_offset =
                        int(offset_y + 0.5f) * in_width * channels;
                    if (offset_x < OFFSET_TOL || offset_x > 1.0f - OFFSET_TOL) {
                        // Need 1 sample, no mixing
                        for (int c = 0; c < channels; ++c) {
                            out_img_data.get()[base_out_idx + c] =
                                in_img_data[clamp(
                                    base_in_idx + extra_in_idx_offset +
                                        int(offset_x + 0.5f) * channels + c,
                                    0, in_img_bytes - 1)];
                        }
                    } else {
                        // Need 2 samples, mix with offset_x
                        for (int c = 0; c < channels; ++c) {
                            const unsigned char val[] = {
                                in_img_data[clamp(
                                    base_in_idx + extra_in_idx_offset + c, 0,
                                    in_img_bytes - 1)],
                                in_img_data[clamp(base_in_idx +
                                                      extra_in_idx_offset +
                                                      channels + c,
                                                  0, in_img_bytes - 1)]};
                            out_img_data.get()[base_out_idx + c] =
                                mix(val[0], val[1], offset_x);
                        }
                    }
                } else {
                    if (offset_x < OFFSET_TOL || offset_x > 1.0f - OFFSET_TOL) {
                        // Need 2 samples, mix with offset_y
                        const int extra_in_idx_offset =
                            int(offset_x + 0.5f) * channels;
                        for (int c = 0; c < channels; ++c) {
                            const unsigned char val[] = {
                                in_img_data[clamp(
                                    base_in_idx + extra_in_idx_offset + c, 0,
                                    in_img_bytes - 1)],
                                in_img_data[clamp(base_in_idx +
                                                      extra_in_idx_offset +
                                                      in_width * channels + c,
                                                  0, in_img_bytes - 1)]};
                            out_img_data.get()[base_out_idx + c] =
                                mix(val[0], val[1], offset_y);
                        }
                    } else {
                        // Need 4 samples, mix with offset_x and offset_y
                        for (int c = 0; c < channels; ++c) {
                            const unsigned char val[] = {
                                in_img_data[clamp(base_in_idx + c, 0,
                                                  in_img_bytes - 1)],
                                in_img_data[clamp(
                                    base_in_idx + c + in_neighbor_offsets[0], 0,
                                    in_img_bytes - 1)],
                                in_img_data[clamp(
                                    base_in_idx + c + in_neighbor_offsets[1], 0,
                                    in_img_bytes - 1)],
                                in_img_data[clamp(
                                    base_in_idx + c + in_neighbor_offsets[2], 0,
                                    in_img_bytes - 1)],
                            };
                            out_img_data.get()[base_out_idx + c] =
                                mix(mix(val[0], val[1], offset_x),
                                    mix(val[2], val[3], offset_x), offset_y);
                        }
                    }
                }
                in_x += in_x_step;
            }
            in_y += in_y_step;
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

#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>

#include "glm/glm.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

using namespace glm;

struct image_handle {
    unsigned char* data;
    int width, height, channels;
    int index(int x, int y) const { return channels * (x + y * width); }
    unsigned char& at(int x, int y, int c) { return data[index(x, y) + c]; }
};

template <typename T>
T slopestep(T edge0, T edge1, T x, float slope) {
    x = clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    const T s = sign(x - 0.5f);
    const T o = (1.0f + s) * 0.5f;
    return o - 0.5f * s * pow(2.0f * (o - s * x), T{slope});
}

float smoothstep(float edge0, float edge1, float x) {
    float t = fmaxf(0.0f, fminf(1.0f, (x - edge0) / (edge1 - edge0)));
    return t * t * (3.0f - 2.0f * t);
}

vec3 to_lin(vec3 x) { return pow(x, vec3(2.2)); }

vec3 to_srgb(vec3 x) { return pow(x, vec3(1.0 / 2.2)); }

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
    image_handle in_img{in_img_data, in_width, in_height, channels};

    printf("Image loaded successfully.\n");
    printf("Image width: %d\n", in_width);
    printf("Image height: %d\n", in_height);
    printf("Number of channels: %d\n", channels);

    // Allocate memory for the output image
    const int out_width = std::stoi(argv[2]);
    const int out_height = std::stoi(argv[3]);
    if (out_width < in_width || out_height < in_height) {
        printf("Error: Target size is smaller than the input image size.\n");
        stbi_image_free(in_img.data);
        return 1;
    }
    const int output_size = out_width * out_height * channels;
    std::unique_ptr<unsigned char[]> out_img_data(
        new unsigned char[output_size]);
    image_handle out_img{out_img_data.get(), out_width, out_height, channels};

    // Stuff that's constant over the whole image
    const vec2 in_size{in_img.width, in_img.height};
    const vec2 out_size{out_img.width, out_img.height};
    const vec2 tx_per_pix = in_size / out_size;
    const vec2 transition_start = vec2(0.5f) - 0.5f * tx_per_pix;
    const vec2 transition_end = vec2(0.5f) + 0.5f * tx_per_pix;
    const float in_x_step = static_cast<float>(in_width) / out_width;
    const float in_y_step = static_cast<float>(in_height) / out_height;

    // Measure performance
    const auto start = std::chrono::high_resolution_clock::now();
    constexpr int num_perf_passes = 100;

    // Iterate over all pixels in the output image
    for (int perf_pass = 0; perf_pass < num_perf_passes; ++perf_pass) {
        float in_y = 0.5f * in_y_step - 0.5f;
        for (int y = 0; y < out_img.height; ++y) {
            const int out_row_offset = y * out_img.width * channels;

            const float period_y = floor(in_y);
            const float phase_y = fract(in_y);
            const float offset_y =
                smoothstep(transition_start.y, transition_end.y, phase_y);

            const int in_row_offset = period_y * in_img.width * channels;

            float in_x = 0.5f * in_x_step - 0.5f;
            for (int x = 0; x < out_img.width; ++x) {
                const float period_x = floor(in_x);
                const float phase_x = fract(in_x);
                const float offset_x =
                    smoothstep(transition_start.x, transition_end.x, phase_x);

                // Calc. and write output pixel
                for (int c = 0; c < channels; ++c) {
                    const unsigned char val[] = {
                        in_img.data[clamp(
                            in_row_offset + int(period_x) * channels + c, 0,
                            in_img_bytes - 1)],
                        in_img.data[clamp(
                            in_row_offset + int(period_x + 1) * channels + c, 0,
                            in_img_bytes - 1)],
                        in_img.data[clamp(in_row_offset +
                                              in_img.width * channels +
                                              int(period_x) * channels + c,
                                          0, in_img_bytes - 1)],
                        in_img.data[clamp(in_row_offset +
                                              in_img.width * channels +
                                              int(period_x + 1) * channels + c,
                                          0, in_img_bytes - 1)],
                    };
                    out_img.data[out_row_offset + x * channels + c] =
                        mix(mix(val[0], val[1], offset_x),
                            mix(val[2], val[3], offset_x), offset_y);
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
        stbi_image_free(in_img.data);
        return 1;
    }

    printf("Output image saved successfully: %s\n", output_path.c_str());

    stbi_image_free(in_img.data);

    return 0;
}

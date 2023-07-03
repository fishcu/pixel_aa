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

// Function to handle "mirror repeat" texture addressing
ivec2 mirror_repeat(const ivec2& coord, const ivec2& size) {
    ivec2 result;
    result.x = coord.x % (2 * size.x);
    result.y = coord.y % (2 * size.y);
    if (result.x < 0) result.x += 2 * size.x;
    if (result.y < 0) result.y += 2 * size.y;
    if (result.x >= size.x) result.x = 2 * size.x - result.x - 1;
    if (result.y >= size.y) result.y = 2 * size.y - result.y - 1;
    return result;
}

// Function to perform nearest-neighbor texture lookup
vec3 tex_nn(const image_handle& img, const ivec2& tex_coord) {
    // Handle texture addressing beyond image bounds using "mirror repeat"
    const ivec2 tex_coord_bounded =
        mirror_repeat(tex_coord, ivec2(img.width, img.height));

    // Calculate the pixel index
    const int pixel_index = img.index(tex_coord_bounded.x, tex_coord_bounded.y);

    // Extract the pixel values
    vec3 pixel;
    for (int c = 0; c < img.channels; ++c) {
        pixel[c] = static_cast<float>(img.data[pixel_index + c]) / 255.0f;
    }

    return pixel;
}

// Function to perform bilinear interpolation for texture lookup
vec3 tex(const image_handle& img, const vec2& tex_coord) {
    // Calculate the four nearest texel coordinates
    const ivec2 texel1(floor(tex_coord.x - 0.5), floor(tex_coord.y - 0.5));
    const ivec2 texel2(texel1.x + 1, texel1.y);
    const ivec2 texel3(texel1.x, texel1.y + 1);
    const ivec2 texel4(texel1.x + 1, texel1.y + 1);

    // Calculate the weights for bilinear interpolation
    const vec2 weight(tex_coord.x - 0.5 - texel1.x,
                      tex_coord.y - 0.5 - texel1.y);

    // Perform bilinear interpolation
    const vec3 pixel1 = tex_nn(img, texel1);
    const vec3 pixel2 = tex_nn(img, texel2);
    const vec3 pixel3 = tex_nn(img, texel3);
    const vec3 pixel4 = tex_nn(img, texel4);

    return mix(mix(pixel1, pixel2, weight.x), mix(pixel3, pixel4, weight.x),
               weight.y);
}

template <typename T>
T slopestep(T edge0, T edge1, T x, float slope) {
    x = clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    const T s = sign(x - 0.5f);
    const T o = (1.0f + s) * 0.5f;
    return o - 0.5f * s * pow(2.0f * (o - s * x), T{slope});
}

vec3 to_lin(vec3 x) { return pow(x, vec3(2.2)); }

vec3 to_srgb(vec3 x) { return pow(x, vec3(1.0 / 2.2)); }

// Params:
// pix_coord: Coordinate in source pixel coordinates
vec3 sample_aa(const image_handle& in_img, const image_handle& out_img,
               vec2 pix_coord, bool gamma_correct, float sharpness,
               vec2 pix_center) {
    // The offset for interpolation is a periodic function with
    // a period length of 1 pixel in source pixel coordinates.
    // The input coordinate is shifted so that the center of the pixel
    // aligns with the start of the period.
    const vec2 pix_coord_shifted = pix_coord - pix_center;
    // Get the period and phase.
    vec2 period;
    vec2 phase = modf(pix_coord_shifted, period);

    // Debug: Some normalization
    // period = abs(period);

    // Get texels per pixel
    const vec2 in_size{in_img.width, in_img.height};
    const vec2 out_size{out_img.width, out_img.height};
    const vec2 tx_per_pix = in_size / out_size;
    // The function starts at 0, then starts transitioning at
    // 0.5 - 0.5 / pixels_per_texel, then reaches 0.5 at 0.5,
    // Then reaches 1 at 0.5 + 0.5 / pixels_per_texel.
    // For sharpness values < 1.0, transition to bilinear filtering.
    const vec2 transition_start =
        min(1.0f, sharpness) * (vec2(0.5f) - 0.5f * tx_per_pix);
    const vec2 transition_end =
        vec2(1.0f) -
        min(1.0f, sharpness) * (vec2(1.0f) - (vec2(0.5f) + 0.5f * tx_per_pix));
    const vec2 offset = slopestep(transition_start, transition_end, phase,
                                  max(1.0f, sharpness));

    // if (gamma_correct) {
    //     printf("input shifted         x = %.3f,\t y = %.3f\n",
    //            pix_coord_shifted.x, pix_coord_shifted.y);
    //     printf("period                x = %.3f,\t y = %.3f\n", period.x,
    //            period.y);
    //     printf("phase                 x = %.3f,\t y = %.3f\n", phase.x,
    //            phase.y);
    //     printf("1 / tx_per_pix        x = %.3f,\t y = %.3f\n",
    //            1.0 / tx_per_pix.x, 1.0 / tx_per_pix.y);
    //     printf("tx_per_pix            x = %.3f,\t y = %.3f\n", tx_per_pix.x,
    //            tx_per_pix.y);
    //     printf("offset trans start    x = %.3f,\t y = %.3f\n",
    //            transition_start.x, transition_start.y);
    //     printf("offset transition end x = %.3f,\t y = %.3f\n",
    //     transition_end.x,
    //            transition_end.y);
    //     printf("OFFSET                x = %.3f,\t y = %.3f\n", offset.x,
    //            offset.y);
    //     printf("\n");
    // }

    // With gamma correct blending, we have to do 4 taps and blend manually.
    // Without it, we can make use of a single tap using bilinear interpolation.
    if (gamma_correct) {
        const vec3 samples[] = {to_lin(tex(in_img, period + vec2(0.5f))),
                                to_lin(tex(in_img, period + vec2(1.5f, 0.5f))),
                                to_lin(tex(in_img, period + vec2(0.5f, 1.5f))),
                                to_lin(tex(in_img, period + vec2(1.5f)))};
        return to_srgb(mix(mix(samples[0], samples[1], offset.x),
                           mix(samples[2], samples[3], offset.x), offset.y));
    } else {
        return tex(in_img, period + 0.5f + offset);
    }
}

vec3 sample_aa(const image_handle& in_img, const image_handle& out_img,
               vec2 pix_coord, bool gamma_correct, float sharpness) {
    // The default pixel center is at 0.5, 0.5 offset from the corner.
    // Make this configurable for subpixel AA.
    return sample_aa(in_img, out_img, pix_coord, gamma_correct, sharpness,
                     vec2(0.5));
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        printf("Usage: %s <input_path> <target_width> <target_height>\n",
               argv[0]);
        return 1;
    }

    const std::string input_path = argv[1];
    const ivec2 out_res(std::stoi(argv[2]), std::stoi(argv[3]));

    int in_width, in_height, channels;
    unsigned char* in_img_data =
        stbi_load(input_path.c_str(), &in_width, &in_height, &channels, 0);
    if (in_img_data == nullptr) {
        printf("Failed to load image.\n");
        return 1;
    }
    image_handle in_img{in_img_data, in_width, in_height, channels};

    printf("Image loaded successfully.\n");
    printf("Image width: %d\n", in_width);
    printf("Image height: %d\n", in_height);
    printf("Number of channels: %d\n", channels);

    if (out_res.x < in_width || out_res.y < in_height) {
        printf("Error: Target size is smaller than the input image size.\n");
        stbi_image_free(in_img.data);
        return 1;
    }

    // Allocate memory for the output image
    const int output_size = out_res.x * out_res.y * channels;
    std::unique_ptr<unsigned char[]> out_img_data(
        new unsigned char[output_size]);
    image_handle out_img{out_img_data.get(), out_res.x, out_res.y, channels};

    constexpr bool gamma_correct = true;
    constexpr bool subpix_interpolation = true;
    constexpr bool subpix_bgr = false;
    constexpr float sharpness = 1.5f;

    // Iterate over all pixels in the output image
    for (int y = 0; y < out_img.height; ++y) {
        // Compute the y pointer offset
        for (int x = 0; x < out_img.width; ++x) {
            const vec2 out_coord{x + 0.5, y + 0.5};
            vec2 in_coord(out_coord.x * in_width / out_res.x,
                          out_coord.y * in_height / out_res.y);

            // Bilinear sampling
            /*
            const vec3 sampled = tex(in_img, input_coord);
            */

            ////////////////////////////////////////////
            // Pixel AA
            vec3 sampled;
            if (!subpix_interpolation) {
                sampled = sample_aa(in_img, out_img, in_coord, gamma_correct,
                                    sharpness);
            } else {
                // Subpixel sampling: Shift the sampling by 1/3rd of an output
                // pixel, assuming that the output size is at monitor
                // resolution.
                for (int i = -1; i < 2; ++i) {
                    const vec2 subpix_offset =
                        vec2((subpix_bgr ? -i : i) / 3.0f, 0.0f);
                    const vec2 subpix_coord =
                        in_coord + subpix_offset *
                                       vec2(in_img.width, in_img.height) /
                                       vec2(out_img.width, out_img.height);

                    // if (x < 12 && y == 0) {
                    //     if (i == -1) {
                    //         printf(
                    //             "RED channel at output x = %.3f,\t y =
                    //             %.3f\n", out_coord.x, out_coord.y);
                    //         printf(
                    //             "in_coord              x = %.3f,\t y =
                    //             %.3f\n", in_coord.x, in_coord.y);
                    //         printf(
                    //             "subpix offset         x = %.3f,\t y =
                    //             %.3f\n", subpix_offset.x, subpix_offset.y);
                    //         printf(
                    //             "subpix coord (input)  x = %.3f,\t y =
                    //             %.3f\n", subpix_coord.x, subpix_coord.y);
                    //         printf(
                    //             "subpix center (input) x = %.3f,\t y =
                    //             %.3f\n", subpix_center.x, subpix_center.y);
                    //     }
                    // } else {
                    //     return 0;
                    // }

                    sampled[i + 1] = sample_aa(in_img, out_img, subpix_coord,
                                               gamma_correct, sharpness)[i + 1];
                }
            }
            /////////////////////////////////////////////

            // Write output pixel
            for (int c = 0; c < channels; ++c) {
                out_img.at(x, y, c) = sampled[c] * 255.0;
            }
        }
    }

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

    if (stbi_write_png(output_path.c_str(), out_res.x, out_res.y, channels,
                       out_img_data.get(), out_res.x * channels) == 0) {
        printf("Failed to save the output image.\n");
        stbi_image_free(in_img.data);
        return 1;
    }

    printf("Output image saved successfully: %s\n", output_path.c_str());

    stbi_image_free(in_img.data);

    return 0;
}

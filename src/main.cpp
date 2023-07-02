#include <iostream>
#include <memory>
#include <string>

#include "glm/glm.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

using namespace glm;

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
vec3 tex_nn(const unsigned char* img, int width, int height, int channels,
            const ivec2& tex_coord) {
    // Handle texture addressing beyond image bounds using "mirror repeat"
    const ivec2 tex_coord_bounded =
        mirror_repeat(tex_coord, ivec2(width, height));

    // Calculate the pixel index
    const int pixel_index =
        (tex_coord_bounded.y * width + tex_coord_bounded.x) * channels;

    // Extract the pixel values
    vec3 pixel;
    for (int c = 0; c < channels; ++c) {
        pixel[c] = static_cast<float>(img[pixel_index + c]) / 255.0f;
    }

    return pixel;
}

// Function to perform bilinear interpolation for texture lookup
vec3 tex(const unsigned char* img, int width, int height, int channels,
         const vec2& tex_coord) {
    // Calculate the four nearest texel coordinates
    const ivec2 texel1(floor(tex_coord.x - 0.5), floor(tex_coord.y - 0.5));
    const ivec2 texel2(texel1.x + 1, texel1.y);
    const ivec2 texel3(texel1.x, texel1.y + 1);
    const ivec2 texel4(texel1.x + 1, texel1.y + 1);

    // Calculate the weights for bilinear interpolation
    const vec2 weight(tex_coord.x - 0.5 - texel1.x,
                      tex_coord.y - 0.5 - texel1.y);

    // Perform bilinear interpolation
    const vec3 pixel1 = tex_nn(img, width, height, channels, texel1);
    const vec3 pixel2 = tex_nn(img, width, height, channels, texel2);
    const vec3 pixel3 = tex_nn(img, width, height, channels, texel3);
    const vec3 pixel4 = tex_nn(img, width, height, channels, texel4);

    return mix(mix(pixel1, pixel2, weight.x), mix(pixel3, pixel4, weight.x),
               weight.y);
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
    unsigned char* in_img =
        stbi_load(input_path.c_str(), &in_width, &in_height, &channels, 0);
    if (!in_img) {
        printf("Failed to load image.\n");
        return 1;
    }

    printf("Image loaded successfully.\n");
    printf("Image width: %d\n", in_width);
    printf("Image height: %d\n", in_height);
    printf("Number of channels: %d\n", channels);

    if (out_res.x < in_width || out_res.y < in_height) {
        printf("Error: Target size is smaller than the input image size.\n");
        stbi_image_free(in_img);
        return 1;
    }

    // Allocate memory for the output image
    const int output_size = out_res.x * out_res.y * channels;
    std::unique_ptr<unsigned char[]> out_img(new unsigned char[output_size]);

    // Structure the memory for 2D addressing
    ivec2 out_stride(channels, channels * out_res.x);

    // Iterate over all pixels in the output image
    for (int y = 0; y < out_res.y; ++y) {
        // Compute the y pointer offset
        int y_offset = y * out_stride.y;
        for (int x = 0; x < out_res.x; ++x) {
            const vec2 out_coord{x + 0.5, y + 0.5};

            // Example: Copy corresponding pixel from the input image
            vec2 input_coord(out_coord.x * in_width / out_res.x,
                             out_coord.y * in_height / out_res.y);
            const vec3 sampled =
                tex(in_img, in_width, in_height, channels, input_coord);

            // Write output pixel
            unsigned char* pixel = out_img.get() + y_offset + x * out_stride.x;
            for (int c = 0; c < channels; ++c) {
                pixel[c] = sampled[c] * 255.0;
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
                       out_img.get(), out_res.x * channels) == 0) {
        printf("Failed to save the output image.\n");
        stbi_image_free(in_img);
        return 1;
    }

    printf("Output image saved successfully: %s\n", output_path.c_str());

    stbi_image_free(in_img);

    return 0;
}

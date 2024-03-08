#include "processing.h"
#include <array>
#include <limits>
#include <string>
#include <sys/types.h>
#include <variant>
#include <vector>

#include <png.h>
using Color = std::array<u_int8_t, 4>;
using Mat = std::vector<std::vector<Color>>;
using Error = std::string;
using Success = bool;


void initializeCenter(Color &center, const Mat &image) {
  int width = image[0].size();
  int height = image.size();
  int x = std::rand() % width;
  int y = std::rand() % height;
  center = image[y][x];
}


int distanceSquared(const Color& a, const Color& b) {
    int distance = 0;
    for (int i = 0; i < 4; ++i) {
        distance += (a[i] - b[i]) * (a[i] - b[i]);
    }
    return distance;
}

// Find the index of the closest center to a given color
int findClosestCenterIndex(const std::vector<Color>& centers, const Color& color) {
    int minDistance = std::numeric_limits<int>::max();
    int index = 0;
    for (int i = 0; i < centers.size(); ++i) {
        int dist = distanceSquared(color, centers[i]);
        if (dist < minDistance) {
            minDistance = dist;
            index = i;
        }
    }
    return index;
}

// Update the center to be the mean of all colors assigned to it
void updateCenter(Color& center, const std::vector<Color>& assignedColors) {
    if (assignedColors.empty()) return;

    std::array<long, 4> sum = {0, 0, 0, 0};
    for (const auto& color : assignedColors) {
        for (int i = 0; i < 4; ++i) {
            sum[i] += color[i];
        }
    }

    for (int i = 0; i < 4; ++i) {
        center[i] = sum[i] / assignedColors.size();
    }
}

void runKmeans(Mat& image, int K, int N) {
    std::vector<Color> centers(K);
    for (int i = 0; i < K; ++i) {
        initializeCenter(centers[i], image);
    }

    for (int iteration = 0; iteration < N; ++iteration) {
        std::vector<std::vector<Color>> clusters(K); // Each cluster holds assigned colors

        // Assign pixels to the nearest center
        for (const auto& row : image) {
            for (const auto& pixel : row) {
                int centerIndex = findClosestCenterIndex(centers, pixel);
                clusters[centerIndex].push_back(pixel);
            }
        }

        // Update centers
        for (int i = 0; i < K; ++i) {
            updateCenter(centers[i], clusters[i]);
        }
    }

    // Optionally: Assign pixels in the image to their cluster's center color
    for (auto& row : image) {
        for (auto& pixel : row) {
            int centerIndex = findClosestCenterIndex(centers, pixel);
            pixel = centers[centerIndex];
        }
    }
}

std::variant<Mat, Error> readPng(const char *imagePath) {
  FILE *fp = fopen(imagePath, "rb");
  if (!fp) {
    return Error("File could not be opened for reading");
  }

  png_structp png =
      png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!png) {
    fclose(fp);
    return Error("Failed to create PNG read structure");
  }

  png_infop info = png_create_info_struct(png);
  if (!info) {
    png_destroy_read_struct(&png, NULL, NULL);
    fclose(fp);
    return Error("Failed to create PNG info structure");
  }

  if (setjmp(png_jmpbuf(png))) {
    png_destroy_read_struct(&png, &info, NULL);
    fclose(fp);
    return Error("Error during PNG creation");
  }

  png_init_io(png, fp);
  png_read_info(png, info);

  int width = png_get_image_width(png, info);
  int height = png_get_image_height(png, info);
  png_byte color_type = png_get_color_type(png, info);
  png_byte bit_depth = png_get_bit_depth(png, info);

  // Convert PNG to a format we can work with
  if (bit_depth == 16)
    png_set_strip_16(png);

  if (color_type == PNG_COLOR_TYPE_PALETTE)
    png_set_palette_to_rgb(png);

  // PNG_COLOR_TYPE_GRAY_ALPHA is always 8 or 16bit depth.
  if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
    png_set_expand_gray_1_2_4_to_8(png);

  if (png_get_valid(png, info, PNG_INFO_tRNS))
    png_set_tRNS_to_alpha(png);

  // These color_type don't have an alpha channel then fill it with 0xff.
  if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY ||
      color_type == PNG_COLOR_TYPE_PALETTE)
    png_set_filler(png, 0xFF, PNG_FILLER_AFTER);

  if (color_type == PNG_COLOR_TYPE_GRAY ||
      color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
    png_set_gray_to_rgb(png);

  png_read_update_info(png, info);

  Mat result(height, std::vector<Color>(width));
  png_bytep *row_pointers = (png_bytep *)malloc(sizeof(png_bytep) * height);
  for (int y = 0; y < height; y++) {
    row_pointers[y] = (png_byte *)malloc(png_get_rowbytes(png, info));
  }

  png_read_image(png, row_pointers);

  // Convert each pixel to grayscale
  for (int y = 0; y < height; y++) {
    png_bytep row = row_pointers[y];
    for (int x = 0; x < width; x++) {
      png_bytep px = &(row[x * 4]); // PNG is RGBA
      // Simple luminosity formula: 0.3 R + 0.59 G + 0.11 B
      result[y][x][0] = px[0];
      result[y][x][1] = px[1];
      result[y][x][2] = px[2];
      result[y][x][3] = px[3];
    }
    free(row_pointers[y]);
  }
  free(row_pointers);

  png_destroy_read_struct(&png, &info, NULL);
  fclose(fp);

  return result;
}
// Function to calculate the mean color of a specific area in the image
Color calculateMeanColor(const Mat& image, int startX, int startY, int endX, int endY) {
    unsigned long long total[4] = {0}; // Accumulate sums for each channel
    int count = 0; // Number of pixels in the specified area
    
    // Iterate over the specified area and accumulate the color values
    for (int y = startY; y < endY; ++y) {
        for (int x = startX; x < endX; ++x) {
            // Add up the color channels for each pixel in the area
            for (int c = 0; c < 4; ++c) {
                total[c] += image[y][x][c];
            }
            ++count; // Increment pixel count
        }
    }
    
    // Calculate mean color values
    Color meanColor;
    if (count > 0) { // Ensure division by zero is not possible
        for (int c = 0; c < 4; ++c) {
            meanColor[c] = static_cast<u_int8_t>(total[c] / count);
        }
    } else {
        // In case the specified area has no pixels, return a default color (e.g., transparent)
        meanColor = {0, 0, 0, 0}; // Assuming a fully transparent pixel as default
    }
    
    return meanColor;
}
// Resize the image to newWidth x newHeight
// using the mean of the pixels in the area
// of the original image that maps to each pixel
// also, keeps the aspect ratio by adding 0 alpha padding
Mat resize(Mat image, int newWidth, int newHeight) {
    int originalHeight = image.size();
    int originalWidth = image[0].size();

    // Calculate aspect ratios
    float originalAspect = static_cast<float>(originalWidth) / originalHeight;
    float newAspect = static_cast<float>(newWidth) / newHeight;

    // Calculate effective width and height after considering aspect ratio
    int effectiveWidth, effectiveHeight;
    if (originalAspect > newAspect) {
        // Width is the limiting dimension
        effectiveWidth = newWidth;
        effectiveHeight = static_cast<int>(newWidth / originalAspect);
    } else {
        // Height is the limiting dimension
        effectiveWidth = static_cast<int>(newHeight * originalAspect);
        effectiveHeight = newHeight;
    }

    // Initialize new image with padding if necessary
    Mat newImage(newHeight, std::vector<Color>(newWidth, {0, 0, 0, 0})); // Default to transparent for padding

    float xRatio = static_cast<float>(originalWidth) / effectiveWidth;
    float yRatio = static_cast<float>(originalHeight) / effectiveHeight;

    for (int i = 0; i < effectiveHeight; i++) {
        for (int j = 0; j < effectiveWidth; j++) {
            int startY = static_cast<int>(i * yRatio);
            int startX = static_cast<int>(j * xRatio);
            int endY = std::min(static_cast<int>((i + 1) * yRatio + 1), originalHeight);
            int endX = std::min(static_cast<int>((j + 1) * xRatio + 1), originalWidth);

            Color meanColor = calculateMeanColor(image, startX, startY, endX, endY);

            // Place the calculated mean color in the new image, adjusting position for any padding
            int newY = i + (newHeight - effectiveHeight) / 2; // Adjust for vertical padding
            int newX = j + (newWidth - effectiveWidth) / 2; // Adjust for horizontal padding
            newImage[newY][newX] = meanColor;
        }
    }

    return newImage;
}

std::variant<Success, Error> writePng(const char *imagePath, const Mat &image) {
  FILE *fp = fopen(imagePath, "wb");
  if (!fp) {
    return Error("File could not be opened for writing.");
  }

  png_structp png_ptr =
      png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!png_ptr) {
    fclose(fp);
    return Error("Failed to create PNG write structure.");
  }

  png_infop info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr) {
    png_destroy_write_struct(&png_ptr, NULL);
    fclose(fp);
    return Error("Failed to create PNG info structure.");
  }

  if (setjmp(png_jmpbuf(png_ptr))) {
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);
    return Error("Error during PNG creation.");
  }

  png_init_io(png_ptr, fp);

  int bit_depth = 8;
  int color_type = PNG_COLOR_TYPE_RGBA;
  int width = image[0].size();
  int height = image.size();

  png_set_IHDR(png_ptr, info_ptr, width, height, bit_depth, color_type,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
               PNG_FILTER_TYPE_DEFAULT);

  png_write_info(png_ptr, info_ptr);

  // Allocate memory for rows
  png_bytep row = (png_bytep)malloc(4 * width * sizeof(png_byte));
  if (!row) {
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);
    return Error("Failed to allocate memory for PNG row.");
  }

  // Write image data
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      row[x * 4 + 0] = static_cast<png_byte>(image[y][x][0]); // Red
      row[x * 4 + 1] = static_cast<png_byte>(image[y][x][1]); // Green
      row[x * 4 + 2] = static_cast<png_byte>(image[y][x][2]); // Blue
      row[x * 4 + 3] = static_cast<png_byte>(image[y][x][3]); // Alpha
    }
    png_write_row(png_ptr, row);
  }

  png_write_end(png_ptr, NULL);

  // Cleanup
  free(row);
  png_destroy_write_struct(&png_ptr, &info_ptr);
  fclose(fp);

  return Success(true);
}

bool scaleImage(const char *imagePath, const char *newImagePath, int newWidth,
                int newHeight) {
  auto image = readPng(imagePath);
  if (std::holds_alternative<Error>(image)) {
    fprintf(stderr, "Error: %s\n", std::get<Error>(image).c_str());
    return false;
  }
  Mat imageMat = std::get<Mat>(image);
  Mat newImageMat = resize(imageMat, newWidth, newHeight);
  auto result = writePng(newImagePath, newImageMat);

  if (std::holds_alternative<Error>(result)) {
    fprintf(stderr, "Error: %s\n", std::get<Error>(result).c_str());
    return false;
  }

  return true;
}

// Quantize the image to N colors using k-means
bool quantizeImage(const char *imagePath, const char *newImagePath, int N) {

  // Read the image
  auto image = readPng(imagePath);
  if (std::holds_alternative<Error>(image)) {
    fprintf(stderr, "Error: %s\n", std::get<Error>(image).c_str());
    return false;
  }
  Mat imageMat = std::get<Mat>(image);
  runKmeans(imageMat, N, 50);

  // Write the new image
  auto result = writePng(newImagePath, imageMat);
  if (std::holds_alternative<Error>(result)) {
    fprintf(stderr, "Error: %s\n", std::get<Error>(result).c_str());
    return false;
  }

  return true;
}

#ifdef TESTING

int main() {
  char imagePath[] = "image.png";
  char newImagePath[] = "newImage.png";
  bool result = scaleImage(imagePath, newImagePath, 500, 300);
  printf("Scale Result: %d\n", result);
  char reducedColorImagePath[] = "reducedColorImage.png";
  result = quantizeImage(newImagePath, reducedColorImagePath, 8);
  printf("Quantize Result: %d\n", result);
  return 0;
}
#endif

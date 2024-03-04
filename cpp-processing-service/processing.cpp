#include "processing.h"

#include <opencv2/opencv.hpp>

void scaleImage(char *imagePath, char *newImagePath, int newWidth,
                int newHeight) {
  cv::Mat image = cv::imread(imagePath);
  cv::resize(image, image, cv::Size(newWidth, newHeight));
  cv::imwrite(newImagePath, image);
}

// Quantize the image to N colors using k-means
void quantizeImage(char *imagePath, char *newImagePath, int N) {
  // Read the image
  cv::Mat image = cv::imread(imagePath);
  if (image.empty()) {
    std::cerr << "Could not read the image: " << imagePath << std::endl;
    return;
  }

  // Reshape and convert the image to a 2D matrix of floats
  cv::Mat data = image.reshape(1, image.total());
  data.convertTo(data, CV_32F);

  // Apply k-means clustering to find N colors
  cv::Mat labels, centers;
  cv::kmeans(data, N, labels,
             cv::TermCriteria(
                 cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 10, 1.0),
             1, cv::KMEANS_PP_CENTERS, centers);

  // Convert centers to 8-bit colors
  centers.convertTo(centers, CV_8U);

  // Create an output image of zeros with the same size as the input image
  cv::Mat newImage(image.size(), image.type(), cv::Scalar::all(0));

  // Map each pixel to its corresponding center
  for (int i = 0; i < data.rows; i++) {
    int cluster_id = labels.at<int>(i);
    cv::Vec3b color = centers.at<cv::Vec3b>(cluster_id);
    newImage.at<cv::Vec3b>(i / image.cols, i % image.cols) = color;
  }

  std::vector<int> compression_params;
  std::string newImagePathStr(newImagePath);
  if (newImagePathStr.find(".png") != std::string::npos) {
    compression_params.push_back(cv::IMWRITE_PNG_COMPRESSION);
    compression_params.push_back(9); // Set compression level from 0 to 9
  }
  if (newImagePathStr.find(".jpg") != std::string::npos) {
    compression_params.push_back(cv::IMWRITE_JPEG_QUALITY);
    compression_params.push_back(
        100); // Set quality level from 0 to 100 (high to low quality)
  }
  // Write the quantized image to the new path
  cv::imwrite(newImagePath, newImage, compression_params);
}

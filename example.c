#include "example.h"

#include <opencv2/opencv.hpp>

int example(int a, int b){
    return a + b;
}



void scaleImage(char* imagePath, char* newImagePath, int newWidth, int newHeight){
    cv::Mat image = cv::imread(imagePath);
    cv::resize(image, image, cv::Size(newWidth, newHeight));
    cv::imwrite(newImagePath, image);
}

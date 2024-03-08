#ifndef EXAMPLE_H
#define EXAMPLE_H
#ifdef __cplusplus
extern "C" {
#endif
bool scaleImage(const char* imagePath, const char* newImagePath, int newWidth, int newHeight);
bool quantizeImage(const char* imagePath, const char* newImagePath, int N);
#ifdef __cplusplus
}
#endif
#endif

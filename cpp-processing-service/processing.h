#ifndef EXAMPLE_H
#define EXAMPLE_H
#ifdef __cplusplus
extern "C" {
#endif
void scaleImage(char* imagePath, char* newImagePath, int newWidth, int newHeight);
void quantizeImage(char* imagePath, char* newImagePath, int N);
#ifdef __cplusplus
}
#endif
#endif

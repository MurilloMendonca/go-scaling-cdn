#ifndef EXAMPLE_H
#define EXAMPLE_H
#ifdef __cplusplus
extern "C" {
#endif


#include <stdio.h>
int example(int a, int b);

void scaleImage(char* imagePath, char* newImagePath, int newWidth, int newHeight);
#ifdef __cplusplus
}
#endif
#endif

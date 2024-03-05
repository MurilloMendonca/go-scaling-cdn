#ifndef TASK_H
#define TASK_H
#include <stdlib.h>
#ifdef __cplusplus
extern "C" {
#endif
#include <stdio.h>
struct TaskPackage {
  char *imagePath;
  char *resizedImagePath;
  char *quantizedImagePath;
  int newWidth;
  int newHeight;
};

struct TaskResult {
  char *resizedImagePath;
  char *quantizedImagePath;
  int SUCCESS;
};
struct TaskPackage makeTaskPackage(char *imagePath, char *resizedImagePath,
                                   char *quantizedImagePath, int newWidth,
                                   int newHeight);

void destroyTaskPackage(struct TaskPackage taskPackage);

char *encodeTask(struct TaskPackage taskPackage);

struct TaskPackage decodeTask(char *buffer);

struct TaskResult makeTaskResult(char *resizedImagePath,
                                 char *quantizedImagePath, int SUCCESS);

void destroyTaskResult(struct TaskResult taskResult);

char *encodeTaskResult(struct TaskResult t);

struct TaskResult decodeTaskResult(char *buffer);

#ifdef __cplusplus
}
#endif
#endif

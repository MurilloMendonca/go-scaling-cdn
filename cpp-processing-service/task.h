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
                                   int newHeight) {
  struct TaskPackage taskPackage = {.imagePath = imagePath,
                                    .resizedImagePath = resizedImagePath,
                                    .quantizedImagePath = quantizedImagePath,
                                    .newWidth = newWidth,
                                    .newHeight = newHeight};
  return taskPackage;
}

void destroyTaskPackage(struct TaskPackage taskPackage) {
  free(taskPackage.imagePath);
  free(taskPackage.resizedImagePath);
  free(taskPackage.quantizedImagePath);
}


char *encodeTask(struct TaskPackage taskPackage) {
  char *buffer = (char *)malloc(2048 * sizeof(char));
  sprintf(buffer, "%s %s %s %d %d", taskPackage.imagePath,
          taskPackage.resizedImagePath, taskPackage.quantizedImagePath,
          taskPackage.newWidth, taskPackage.newHeight);
  return buffer;
}

struct TaskPackage decodeTask(char *buffer) {
  struct TaskPackage t = {};
  t.imagePath = (char *)malloc(2048 * sizeof(char));
  t.resizedImagePath = (char *)malloc(2048 * sizeof(char));
  t.quantizedImagePath = (char *)malloc(2048 * sizeof(char));
  sscanf(buffer, "%s %s %s %d %d", t.imagePath, t.resizedImagePath,
         t.quantizedImagePath, &t.newWidth, &t.newHeight);
  return t;
}

struct TaskResult makeTaskResult(char *resizedImagePath,
                                 char *quantizedImagePath, int SUCCESS) {
  struct TaskResult taskResult = {.resizedImagePath = resizedImagePath,
                                  .quantizedImagePath = quantizedImagePath,
                                  .SUCCESS = SUCCESS};
  return taskResult;
}

void destroyTaskResult(struct TaskResult taskResult) {
  free(taskResult.resizedImagePath);
  free(taskResult.quantizedImagePath);
}

char *encodeTaskResult(struct TaskResult t) {
  char *buffer = (char *)malloc(2048 * sizeof(char));
  sprintf(buffer, "%s %s %d", t.resizedImagePath, t.quantizedImagePath,
          t.SUCCESS);
  return buffer;
}

struct TaskResult decodeTaskResult(char *buffer) {
  struct TaskResult t = {};
  t.resizedImagePath = (char *)malloc(2048 * sizeof(char));
  sscanf(buffer, "%s %s %d", t.resizedImagePath, t.quantizedImagePath,
         &t.SUCCESS);
  return t;
}

#ifdef __cplusplus
}
#endif
#endif

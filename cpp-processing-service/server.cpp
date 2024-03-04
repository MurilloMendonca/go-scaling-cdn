#include "processing.h"
#include "task.h"
#include <arpa/inet.h>
#include <optional>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

#define DEBUG 1
int openSocket(int port) {
  int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (serverSocket < 0) {
    return -1;
  }

  struct sockaddr_in serverAddress;
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(port);
  serverAddress.sin_addr.s_addr = INADDR_ANY;

  if (bind(serverSocket, (struct sockaddr *)&serverAddress,
           sizeof(serverAddress)) < 0) {
    return -1;
  }

  if (listen(serverSocket, 10) < 0) {
    return -1;
  }

  return serverSocket;
}

int acceptConnection(int serverSocket) {
  struct sockaddr_in clientAddress;
  socklen_t clientAddressLength = sizeof(clientAddress);
  return accept(serverSocket, (struct sockaddr *)&clientAddress,
                &clientAddressLength);
}

std::optional<TaskPackage> receiveTask(int clientSocket) {
  char buffer[2048];
  int bytesRead = recv(clientSocket, buffer, 2048, 0);
  if (bytesRead < 0) {
    return std::nullopt;
  }

  return decodeTask(buffer);
}

bool processTask(TaskPackage task) {
  scaleImage(task.imagePath, task.resizedImagePath, task.newWidth,
             task.newHeight);
  quantizeImage(task.imagePath, task.quantizedImagePath, 8);
  return true;
}

bool sendResult(int clientSocket, TaskResult result) {
  auto buffer = encodeTaskResult(result);
  int bytesSent = send(clientSocket, buffer, 2048, 0);
  return bytesSent >= 0;
}

void handleClient(int clientSocket) {
  auto task = receiveTask(clientSocket);

  if (!task.has_value()) {
    printf("Failed to receive task\n");
    printf("Closing sockets\n");
    close(clientSocket);
    return;
  }

#if DEBUG
  printf("Received task: %s %s %s %d %d\n", task->imagePath,
         task->resizedImagePath, task->quantizedImagePath, task->newWidth,
         task->newHeight);
#endif

  bool status = processTask(task.value());
  TaskResult result = makeTaskResult(task->resizedImagePath,
                                     task->quantizedImagePath, status ? 1 : 0);

  status = sendResult(clientSocket, result);

  if (!status) {
    printf("Failed to send result\n");
  }

  close(clientSocket);
}

int main() {
  int serverSocket = openSocket(8989);
  if (serverSocket < 0) {
    return -1;
  }

  while (1) {
    int clientSocket = acceptConnection(serverSocket);
    if (clientSocket < 0) {
      continue;
    }

    auto thread = std::thread(handleClient, clientSocket);
    thread.detach();
  }

  close(serverSocket);

  return 0;
}

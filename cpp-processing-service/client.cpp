#include "task.h"
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>



int main() {
  int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (clientSocket < 0) {
    return -1;
  }

  struct sockaddr_in serverAddress;
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(8989);
    serverAddress.sin_addr.s_addr = inet_addr("0.0.0.0");

    if (connect(clientSocket, (struct sockaddr *)&serverAddress,
                sizeof(serverAddress)) < 0) {
        return -1;
    }

    TaskPackage task = makeTaskPackage("flower.jpg", "resImage.jpg", "lessColorsImage.jpg", 100, 100);
    send(clientSocket, encodeTask(task), 2048, 0);

    char buffer[1024];
    int bytesRead = recv(clientSocket, buffer, 1024, 0);
    if (bytesRead < 0) {
        return -1;
    }

    TaskResult result = decodeTaskResult(buffer);

    printf("New image path: %s\n", result.quantizedImagePath);
    printf("Success: %d\n", result.SUCCESS);

    close(clientSocket);
    return 0;
}

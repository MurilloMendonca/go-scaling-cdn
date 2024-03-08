#include "processing.h"
#include <arpa/inet.h>
#include <optional>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <variant>

struct ScaleTask {
  std::string imagePath;
  std::string resizedImagePath;
  int newWidth;
  int newHeight;
};

struct QuantizeTask {
  std::string imagePath;
  std::string quantizedImagePath;
  int levels;
};

struct TaskResult {};

using Error = std::string;

// The task package is a string of the form:
//<task_type>:<task_data>:
// where task_type is either 's' for scale or 'q' for quantize
std::variant<ScaleTask, QuantizeTask, Error>
parseTask(const std::string &buffer) {

  if (buffer.size() < 3) {
    return "Invalid task: too short";
  }

  if (buffer[1] != ':') {
    return "Invalid task: missing colon on position 1";
  }

  if (buffer[0] == 's') {
    auto colonPos = buffer.find(':', 2);
    if (colonPos == std::string::npos) {
      return "Invalid task: missing colon on position 2 for scale task";
    }

    auto colonPos2 = buffer.find(':', colonPos + 1);
    if (colonPos2 == std::string::npos) {
      return "Invalid task: missing colon on position 3 for scale task";
    }

    auto colonPos3 = buffer.find(':', colonPos2 + 1);
    if (colonPos3 == std::string::npos) {
      return "Invalid task: missing colon on position 4 for scale task";
    }

    auto imagePath = buffer.substr(2, colonPos - 2);
    auto resizedImagePath =
        buffer.substr(colonPos + 1, colonPos2 - colonPos - 1);
    auto newWidth =
        std::stoi(buffer.substr(colonPos2 + 1, colonPos3 - colonPos2 - 1));
    auto newHeight = std::stoi(buffer.substr(colonPos3 + 1));

    return ScaleTask{imagePath, resizedImagePath, newWidth, newHeight};
  } else if (buffer[0] == 'q') {
      //quantize task example: "q:/path/to/image:/path/to/quantized/image:256:"
    auto imagePathEnd = buffer.find(':', 2);
    if (imagePathEnd == std::string::npos) {
      return "Invalid task: missing colon on position 2 for quantize task";
    }

    auto quantizedImagePathEnd = buffer.find(':', imagePathEnd + 1);
    if (quantizedImagePathEnd == std::string::npos) {
      return "Invalid task: missing colon on position 3 for quantize task";
    }

    auto levelsEnd = buffer.find(':', quantizedImagePathEnd + 1);
    if (levelsEnd == std::string::npos) {
      return "Invalid task: missing colon on position 4 for quantize task";
    }

    auto imagePath = buffer.substr(2, imagePathEnd - 2);
    auto quantizedImagePath =
        buffer.substr(imagePathEnd + 1, quantizedImagePathEnd - imagePathEnd - 1);
    auto levels = std::stoi(buffer.substr(quantizedImagePathEnd + 1, levelsEnd - quantizedImagePathEnd - 1));

    return QuantizeTask{imagePath, quantizedImagePath, levels};
  } else {
    return "Invalid task";
  }
}

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

std::variant<ScaleTask, QuantizeTask, Error> receiveTask(int clientSocket) {
  char buffer[2048];
  int bytesRead = recv(clientSocket, buffer, 2048, 0);
  if (bytesRead < 0) {
    return Error("Failed to receive task");
  }
  if (bytesRead == 0) {
    return Error("Client disconnected");
  }

  std::string buff(buffer, bytesRead);
  return parseTask(buff);
}

bool processTask(std::variant<ScaleTask, QuantizeTask, Error> task) {
  bool status = true;
  if (std::holds_alternative<ScaleTask>(task)) {
    auto scaleTask = std::get<ScaleTask>(task);
    status = status && scaleImage(scaleTask.imagePath.c_str(),
                                  scaleTask.resizedImagePath.c_str(),
                                  scaleTask.newWidth, scaleTask.newHeight);
  } else if (std::holds_alternative<QuantizeTask>(task)) {
    auto quantizeTask = std::get<QuantizeTask>(task);
    status = status && quantizeImage(quantizeTask.imagePath.c_str(),
                                     quantizeTask.quantizedImagePath.c_str(),
                                     quantizeTask.levels);
  }
  return status;
}

bool sendResult(int clientSocket, std::string result) {
  const char* buffer = result.c_str();
  int bytesSent = send(clientSocket, buffer, sizeof(char) * sizeof(buffer), 0);
  return bytesSent >= 0;
}

void handleClient(int clientSocket) {
  while (1) {
    auto task = receiveTask(clientSocket);

    if (std::holds_alternative<Error>(task)) {
      printf("%s\n", std::get<Error>(task).c_str());
      sendResult(clientSocket, std::get<Error>(task).c_str());
      break;
    }

    bool status = processTask(task);

    status = sendResult(clientSocket, status ? "OK" : "Failed");

    if (!status) {
      printf("Failed to send result\n");
      break;
    }
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

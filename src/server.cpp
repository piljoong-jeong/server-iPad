#include "server.hpp"
#include <iostream>

namespace CG {
bool Server::isRunning = false;

Server::Server(int IP_PORT, int PACKET_SIZE) {
  net = new CG::Network("0.0.0.0", IP_PORT, PACKET_SIZE, Network::SERVER);
  GetImage::initRecv(net);
  isRunning = true;
  firstReceived = false;

  thr = new std::thread([this]() {
    int prevID = -1;
    while (isRunning) {
      uint8_t *color;
      uint16_t *depth;
      float *arr = nullptr;
      float cx, cy, fx, fy;
      int frameID;

      GetImage::getImage(color, depth, arr, cx, cy, fx, fy, frameID);

      if (frameID != -1 && prevID != frameID) {
        firstReceived = true;
        prevID = frameID;
        float poseArr[16];
        for (int i = 0; i < 16; i++) {
          poseArr[i] = arr[i];
        }

        if (arr != nullptr)
          delete[] arr;
        for (auto cb : callbacks_.image) {
          cb(color, depth, poseArr, cx, cy, fx, fy, frameID);
        }
      }

      delete[] color;
      delete[] depth;
    }

    for (auto cb : callbacks_.end) {
      cb();
    }

    isRunning = false;
  });

  setPriority(thr, 2);
}

void Server::sendColor(uint8_t *data, int frameID, int width, int height) {
  if (isRunning) {
    unsigned long file_length;

    uint8_t *jpegImg = writeJPEG(data, width, height, file_length);
    net->sendColor(frameID, jpegImg, static_cast<int>(file_length));
    delete[] jpegImg;
  } else {
    std::cerr
        << "Warning : Server is not running, but called server::send function"
        << std::endl;
  }
}

void Server::sendDepth(uint8_t *data, int frameID, int width, int height) {
  if (isRunning) {
    unsigned long file_length;

    uint8_t *jpegImg = writeJPEG(data, width, height, file_length);
    net->sendDepth(frameID, jpegImg, static_cast<int>(file_length));
    delete[] jpegImg;
  } else {
    std::cerr
        << "Warning : Server is not running, but called server::send function"
        << std::endl;
  }
}

Server::~Server() {
  isRunning = false;
  thr->join();
  // TO-DO : If you end up the server, you should delete net!
  // delete net;
}
} // namespace CG

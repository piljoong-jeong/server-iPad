#pragma once

#if defined(_MSC_VER)
#include <WinSock2.h>
#pragma comment(lib, "Ws2_32.lib")
typedef int socklen_t;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "common.h"
#include "safequeue.hpp"
#include <iostream>
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <unordered_map>
#include <unordered_set>

namespace CG {

class Meta {
public:
  static int getFrame(uint8_t *);
  static int getPacket(uint8_t *);
  static int getFile(uint8_t *);
  static int getLastPacket(int, int);
  static int getMode(uint8_t *);
};

class Data {
public:
  int packet_length;
  uint8_t **packets;

  Data(int _p, uint8_t **_ps) {
    packet_length = _p;
    packets = _ps;
  }
};

class Frame {
public:
  std::unordered_set<int> packets;
  int num_pack;
  int max_pack, packet_size;
  uint8_t *file;

  Frame(int file_length, int _packet_num, int _packet_size) {
    file = new uint8_t[file_length];

    max_pack = _packet_num;
    packet_size = _packet_size;

    num_pack = 0;
  }

  uint8_t *add(uint8_t *buf) {
    int pkt_id = Meta::getPacket(buf);
    int pkt_data_size = packet_size - 8;
    int frame_file_length = Meta::getFile(buf);
    int pkt_last_id = Meta::getLastPacket(frame_file_length, packet_size);
    int pkt_last_size = frame_file_length - pkt_last_id * pkt_data_size;

    if (packets.find(pkt_id) == packets.end()) {
      if (pkt_id == pkt_last_id)
        memcpy(file + pkt_id * pkt_data_size, buf + 8, pkt_last_size);
      else
        memcpy(file + pkt_id * pkt_data_size, buf + 8, pkt_data_size);
      num_pack++;
      if (num_pack == max_pack) {
        return file;
      }
    }

    return nullptr;
  }
};

class Network {
public:
  enum type1 { COLOR, DEPTH, META, SYS };
  enum type2 { SERVER, CLIENT };

  Network(const char *, int, int, int);
  ~Network();

  // Related to Send Feature
  void sendColor(int, uint8_t *, int);
  void sendDepth(int, uint8_t *, int);
  void sendMeta(int, uint8_t *, int);
  void sendEnd();
  // Related to Recv Feature
  uint8_t *recvData(int &, int &, int &);

private:
  // Related to Send Feature
  void sendClear();
  void sendSys(uint8_t);
  void sendData(int, uint8_t *, int, int);

  // Member
  SafeQueue<Data> q;
  struct sockaddr_in sock_con, cli_con;
  int packet_size, sock_fd, type;
  std::unordered_map<int, Frame *> depth_frames, color_frames, meta_frames;
  std::thread sender;

  int clean_color = 0, clean_meta = 0, clean_depth = 0;

  // Memory Clean
  static void gc(int);
  static bool isRunning;
};

} // namespace CG

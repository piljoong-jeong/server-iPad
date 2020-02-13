//
//  Network.cpp
//  Viewer
//
//  Created by  CGLAB_MAC on 2020/01/06.
//

#include "./Network.hpp"

#define IP_PROTOCOL 0
#define sendrecvflag 0

// using namespace std;

namespace rtabmap {

int Meta::getMode(uint8_t *pkt) {
  int mod_id = 0;
  mod_id |= pkt[2] & 0xC0;
  mod_id >>= 6;

  return mod_id;
}

int Meta::getFrame(uint8_t *pkt) {
  int frm_id = 0;
  frm_id |= pkt[2] & 0x3F;
  frm_id <<= 8;
  frm_id |= pkt[1] & 0xFF;
  frm_id <<= 8;
  frm_id |= pkt[0];

  return frm_id;
}

int Meta::getPacket(uint8_t *pkt) {
  int pkt_id = 0;
  pkt_id |= pkt[4] & 0xFF;
  pkt_id <<= 8;
  pkt_id |= pkt[3] & 0xFF;

  return pkt_id;
}

int Meta::getLastPacket(int file_length, int packet_size) {
  int recv_pkt_data_size = packet_size - 8;
  int recv_pkt_last_size = file_length % recv_pkt_data_size;
  int recv_pkt_last_id = file_length / recv_pkt_data_size;

  if (recv_pkt_last_size == 0) {
    recv_pkt_last_size = recv_pkt_data_size;
    recv_pkt_last_id--;
  }

  return recv_pkt_last_id;
}

int Meta::getFile(uint8_t *pkt) {
  int file_length = 0;
  file_length |= pkt[7] & 0xFF;
  file_length <<= 8;
  file_length |= pkt[6] & 0xFF;
  file_length <<= 8;
  file_length |= pkt[5] & 0xFF;

  return file_length;
}

bool Network::isRunning = true;

Network::Network(const char *IP_ADDR, int PORT, int _packet_size, int _type) {
  type = _type;
  sock_fd = socket(AF_INET, SOCK_DGRAM, IP_PROTOCOL);

  if (sock_fd < 0) {
    std::cerr << "Error : Socket is not opened\n" << std::endl;
    exit(EXIT_FAILURE);
  }

  if (_packet_size <= 8) {
    std::cerr << "Error : Packet size is under or equal as 8" << std::endl;
    exit(EXIT_FAILURE);
  }

  sock_con.sin_family = AF_INET;
  if (type == CLIENT) {
    sock_con.sin_addr.s_addr = inet_addr(IP_ADDR);
    sock_con.sin_port = htons(PORT);
  } else {
    sock_con.sin_addr.s_addr = INADDR_ANY;
    sock_con.sin_port = htons(PORT);

    if (bind(sock_fd, (struct sockaddr *)&sock_con, sizeof(sock_con)) < 0) {
      std::cerr << "Binding error" << std::endl;
      exit(EXIT_FAILURE);
    }
  }

  packet_size = _packet_size;

  sender = std::thread([this]() {
    while (isRunning) {
      Data d = q.dequeue();

      for (int i = 0; i < d.packet_length; i++) {
        if (type == CLIENT)
          sendto(sock_fd, d.packets[i], packet_size, sendrecvflag,
                 (struct sockaddr *)&sock_con, sizeof(sock_con));
        else
          sendto(sock_fd, d.packets[i], packet_size, sendrecvflag,
                 (struct sockaddr *)&cli_con, sizeof(cli_con));

        delete[] d.packets[i];
      }
      delete[] d.packets;
    }
  });
  sender.detach();

  if (type == CLIENT)
    sendSys(); // To clear up the previous works
}

Network::~Network() {
  isRunning = false;
}

void Network::sendColor(int frame_num, uint8_t *content, int file_length) {
  sendData(frame_num, content, file_length, COLOR);
}

void Network::sendDepth(int frame_num, uint8_t *content, int file_length) {
  sendData(frame_num, content, file_length, DEPTH);
}

void Network::sendMeta(int frame_num, uint8_t *content, int file_length) {
  sendData(frame_num, content, file_length, META);
}

void Network::sendSys() {
  uint8_t *a = new uint8_t[1];
  a[0] = 0x00;
  sendData(0, a, 1, SYS);
}

void Network::sendData(int frame_num, uint8_t *content, int file_length,
                       int mode) {
  if (sock_fd < 0) {
    printf("fd is not same\n");
    exit(EXIT_FAILURE);
  }

  // printf("Sending %d %d\n", frame_num, file_length);
  uint8_t block[8] = {
      static_cast<uint8_t>(frame_num & 0xFF),
      static_cast<uint8_t>((frame_num & 0xFF00) >> 8),
      static_cast<uint8_t>(
          ((((frame_num & 0xFF0000) >> 16) & 0x3F) | (mode << 6))),
      0,
      0,
      static_cast<uint8_t>(file_length & 0xFF),
      static_cast<uint8_t>((file_length & 0xFF00) >> 8),
      static_cast<uint8_t>((file_length & 0xFF0000) >> 16)};

  int send_pkt_data_size = packet_size - 8;
  int send_pkt_last_size = file_length % send_pkt_data_size;
  int send_pkt_last_id = file_length / send_pkt_data_size;

  if (send_pkt_last_size == 0) {
    send_pkt_last_size = send_pkt_data_size;
    send_pkt_last_id--;
  }

  uint8_t **buffer = new uint8_t *[send_pkt_last_id + 1];
  for (int i = 0; i <= send_pkt_last_id; i++) {
    buffer[i] = new uint8_t[packet_size];
    memcpy(buffer[i], block, 8);
    buffer[i][3] = static_cast<uint8_t>(i & 0xFF);
    buffer[i][4] = static_cast<uint8_t>((i & 0xFF00) >> 8);

    if (i == send_pkt_last_id)
      memcpy(&buffer[i][8], content + i * send_pkt_data_size,
             send_pkt_last_size);
    else
      memcpy(&buffer[i][8], content + i * send_pkt_data_size,
             send_pkt_data_size);
  }

  q.enqueue(Data(send_pkt_last_id + 1, buffer));
}

uint8_t *Network::recvData(int &mode, int &frame, int &file_length) {
  uint8_t *buf = (uint8_t *)malloc(sizeof(uint8_t) * packet_size);

  do {
    size_t nBytes;
    socklen_t size = sizeof(sock_con);
    if (type == CLIENT)
      nBytes = recvfrom(sock_fd, buf, packet_size, sendrecvflag,
                        (struct sockaddr *)&sock_con, &size);
    else
      nBytes = recvfrom(sock_fd, buf, packet_size, sendrecvflag,
                        (struct sockaddr *)&cli_con, &size);

    if (nBytes != packet_size)
      continue;

    int frame_id = Meta::getFrame(buf);
    int mode_temp = Meta::getMode(buf);
    int frame_file_length = Meta::getFile(buf);
    int frame_pkt_size =
        Meta::getLastPacket(frame_file_length, packet_size) + 1;

    Frame *res;
    switch (mode_temp) {
    case COLOR:
      if (frame_id < clean_color)
        continue;

      if (color_frames.find(frame_id) == color_frames.end()) {
        color_frames.insert({frame_id, new Frame(frame_file_length,
                                                 frame_pkt_size, packet_size)});
      }

      res = color_frames[frame_id];
      break;

    case DEPTH:
      if (frame_id < clean_depth)
        continue;

      if (type == CLIENT) {
        std::cout
            << "Warning : Unexpected Depth data receive in iOS Application"
            << std::endl;
        continue;
      }

      if (depth_frames.find(frame_id) == depth_frames.end()) {
        depth_frames.insert({frame_id, new Frame(frame_file_length,
                                                 frame_pkt_size, packet_size)});
      }

      res = depth_frames[frame_id];
      break;

    case META:
      if (frame_id < clean_meta)
        continue;

      if (type == CLIENT) {
        std::cout << "Warning : Unexpected Meta data receive in iOS Application"
                  << std::endl;
        continue;
      }

      if (meta_frames.find(frame_id) == meta_frames.end()) {
        meta_frames.insert({frame_id, new Frame(frame_file_length,
                                                frame_pkt_size, packet_size)});
      }

      res = meta_frames[frame_id];
      break;

    case SYS:
      if (type == CLIENT) {
        std::cout
            << "Warning : Unexpected System data receive in iOS Application"
            << std::endl;
        continue;
      } else {
        depth_frames.clear();
        color_frames.clear();
        meta_frames.clear();

        clean_color = 0;
        clean_depth = 0;
        clean_meta = 0;
        std::cout << "Server Network HashMap cleaned" << std::endl;
      }

      mode = mode_temp;
      frame = -1;
      return nullptr;

    default:
      res = NULL;
      std::cout << "Warning : Sth wrong is coming" << std::endl;
      continue;
    }

    uint8_t *result = res->add(buf);
    if (result != nullptr) {
      mode = mode_temp;
      file_length = frame_file_length;
      frame = frame_id;

      if (mode == COLOR)
        color_frames.erase(color_frames.find(frame_id));
      else if (mode == DEPTH)
        depth_frames.erase(depth_frames.find(frame_id));
      else if (mode == META)
        meta_frames.erase(meta_frames.find(frame_id));

      delete[] buf;
      return result;
    }
  } while (isRunning);
    return nullptr;
}

} // namespace rtabmap

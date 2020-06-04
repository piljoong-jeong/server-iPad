#include "getimage.hpp"

namespace CG {

std::mutex GetImage::color_m, GetImage::depth_m, GetImage::meta_m,
    GetImage::mat_m;
uint8_t *GetImage::color_buf, *GetImage::depth_buf, *GetImage::meta_buf;
int GetImage::color_frame_id, GetImage::depth_frame_id, GetImage::meta_frame_id;
int GetImage::color_file_len, GetImage::depth_file_len, GetImage::meta_file_len;

uint8_t *GetImage::color_arr;
uint16_t *GetImage::depth_arr;
float *GetImage::meta_arr;
int GetImage::frame_sync, GetImage::meta_arr_size, GetImage::internal_frame;
bool GetImage::isRunning = true;

void GetImage::stop() { isRunning = false; }

bool GetImage::run() { return isRunning; }

const int width = 640;
const int height = 480;
const int color_channel = 3;
void GetImage::getImage(uint8_t *&color, uint16_t *&depth, float *&arr,
                        float &cx, float &cy, float &fx, float &fy,
                        int &frameID) {
  uint8_t *color_copied;
  uint8_t *depth_copied;
  float *meta_copied;

  while (true) {
    color_m.lock();
    depth_m.lock();
    meta_m.lock();
    if (color_buf == nullptr || depth_buf == nullptr || meta_buf == nullptr) {
      /*
      if (color_buf == nullptr) printf("Color ");
      if (depth_buf == nullptr) printf("Depth ");
      if (meta_buf == nullptr) printf("Meta ");
      printf(" is nullptr\n"); */
      frameID = -1;
      color_m.unlock();
      depth_m.unlock();
      meta_m.unlock();
      return;
    } else if (color_frame_id == depth_frame_id &&
               depth_frame_id == meta_frame_id) {
      color_copied = new uint8_t[color_file_len];
      depth_copied = new uint8_t[depth_file_len];
      meta_copied = new float[meta_file_len / 4];
      memcpy(color_copied, color_buf, color_file_len);
      memcpy(depth_copied, depth_buf, depth_file_len);
      memcpy(meta_copied, meta_buf, meta_file_len);
      frameID = color_frame_id;
      color_m.unlock();
      depth_m.unlock();
      meta_m.unlock();
      break;
    } else {
    }
    meta_m.unlock();
    depth_m.unlock();
    color_m.unlock();
  }

  int width, height;
  color = readJPEG(color_copied, width, height, color_file_len);
  depth = readZSTD(depth_copied, depth_file_len);
  float *arr_val = new float[16];
  memcpy(arr_val, meta_copied, 16 * sizeof(float));
  arr = arr_val;

  if (meta_file_len == 80) {
    cx = meta_copied[16];
    cy = meta_copied[17];
    fx = meta_copied[18];
    fy = meta_copied[19];
  }
  delete[] color_copied;
  delete[] depth_copied;
  delete[] meta_copied;
}

void GetImage::initRecv(Network *net) {
  color_frame_id = -1;
  depth_frame_id = -1;
  meta_frame_id = -1;
  frame_sync = -1;
  internal_frame = -1;

  std::thread t1 = std::thread([net]() {
    int mode, frame, file_length;
    while (isRunning) {
      uint8_t *buf = net->recvData(mode, frame, file_length);

      if (mode == Network::COLOR) {
        color_m.lock();
        if (color_buf != nullptr)
          delete[] color_buf;
        color_buf = buf;
        color_frame_id = frame;
        color_file_len = file_length;
        color_m.unlock();
      } else if (mode == Network::DEPTH) {
        depth_m.lock();
        if (depth_buf != nullptr)
          delete[] depth_buf;
        depth_buf = buf;
        depth_frame_id = frame;
        depth_file_len = file_length;
        depth_m.unlock();
      } else if (mode == Network::META) {
        meta_m.lock();
        if (meta_buf != nullptr)
          delete[] meta_buf;
        meta_buf = buf;
        meta_frame_id = frame;
        meta_file_len = file_length;
        meta_m.unlock();
      } else if (mode == Network::SYS) {
        color_m.lock();
        depth_m.lock();
        meta_m.lock();

        color_frame_id = depth_frame_id = meta_frame_id = -1;
        if (color_buf != nullptr)
          delete[] color_buf;
        color_buf = nullptr;
        if (depth_buf != nullptr)
          delete[] depth_buf;
        depth_buf = nullptr;
        if (meta_buf != nullptr)
          delete[] meta_buf;
        meta_buf = nullptr;

        color_m.unlock();
        depth_m.unlock();
        meta_m.unlock();

        if (buf[8] == 0x1) {
          // END
          stop();
          break;
        }

        free(buf);
      }
    }
  });

  CG::setPriority(&t1, 1);
  t1.detach();
}

} // namespace CG
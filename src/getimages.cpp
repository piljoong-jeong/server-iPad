#include "./getimage.hpp"

namespace rtabmap {

std::mutex GetImage::color_m, GetImage::depth_m, GetImage::meta_m,
    GetImage::mat_m;
uint8_t *GetImage::color_buf, *GetImage::depth_buf, *GetImage::meta_buf;
int GetImage::color_frame_id, GetImage::depth_frame_id, GetImage::meta_frame_id;
int GetImage::color_file_len, GetImage::depth_file_len, GetImage::meta_file_len;

cv::Mat GetImage::color_mat, GetImage::depth_mat;
float *GetImage::meta_arr;
int GetImage::frame_sync, GetImage::meta_arr_size, GetImage::internal_frame;
bool GetImage::isRunning = true;

void GetImage::stop() {
    isRunning = false;
}

void GetImage::getImage(cv::Mat &color, cv::Mat &depth, float *arr, float &cx,
                        float &cy, float &fx, float &fy, int &frameID) {
  mat_m.lock();
  if (color_mat.empty() || depth_mat.empty() || meta_arr == nullptr) {
    frameID = -1;
  } else {
    color = color_mat;
    depth = depth_mat;
    frameID = frame_sync;

    arr = new float[16];
    memcpy(arr, meta_arr, 16 * sizeof(float));
    cx = meta_arr[16];
    cy = meta_arr[17];
    fx = meta_arr[18];
    fy = meta_arr[19];
    // printf("%f %f %f %f %f %f\n", arr[0], arr[1], cx, cy, fx, fy);
  }
  mat_m.unlock();
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
      }
    }
      
  });
  t1.detach();

  std::thread t2 = std::thread([]() {
    while (isRunning) {
      bool color_acq = color_m.try_lock();
      bool depth_acq = depth_m.try_lock();
      bool meta_acq = meta_m.try_lock();

      if (color_acq && depth_acq && meta_acq && color_frame_id != -1 &&
          color_frame_id != internal_frame &&
          color_frame_id == depth_frame_id && depth_frame_id == meta_frame_id) {
        //printf("ID : %d\n", color_frame_id);
        internal_frame = color_frame_id;
        uint8_t *color_tmp_buf = new uint8_t[color_file_len];
        uint8_t *depth_tmp_buf = new uint8_t[depth_file_len];
        uint8_t *meta_tmp_buf = new uint8_t[meta_file_len];

        memcpy(color_tmp_buf, color_buf, color_file_len);
        memcpy(depth_tmp_buf, depth_buf, depth_file_len);
        memcpy(meta_tmp_buf, meta_buf, meta_file_len);

        int color_tmp_len = color_file_len;
        int depth_tmp_len = depth_file_len;
        int meta_tmp_len = meta_file_len;
        int frame_tmp = color_frame_id;

        std::thread t3 = std::thread(
            [color_tmp_buf, depth_tmp_buf, meta_tmp_buf, color_tmp_len,
             depth_tmp_len, meta_tmp_len, frame_tmp]() {
              cv::Mat color_tmp;
              auto t1 = std::chrono::high_resolution_clock::now();
              std::vector<uint8_t> ImVec(color_tmp_buf,
                                         color_tmp_buf + color_tmp_len);
              cv::Mat color_tmp_org = cv::imdecode(ImVec, 1);
              color_tmp = cv::Mat(480, 640, CV_8UC3);
              color_tmp_org.convertTo(color_tmp, CV_8UC3);

              cv::Mat depth_tmp = cv::Mat(480, 640, CV_16UC1);

              std::vector<uint8_t> ImVec2(depth_tmp_buf,
                                          depth_tmp_buf + depth_tmp_len);
              cv::Mat depth_tmp_org = cv::imdecode(ImVec2, -1);
              depth_tmp = depth_tmp_org;
              float *meta_tmp = new float[meta_file_len / sizeof(float)];
              memcpy(meta_tmp, meta_tmp_buf, meta_file_len);

              mat_m.lock();
              color_mat = color_tmp;
              depth_mat = depth_tmp;
              if (meta_arr != nullptr)
                delete[] meta_arr;
              meta_arr = meta_tmp;
              meta_arr_size = meta_file_len / sizeof(float);
              frame_sync = frame_tmp;
              mat_m.unlock();

              delete[] color_tmp_buf;
              delete[] depth_tmp_buf;
              delete[] meta_tmp_buf;
              // delete[] depth_decomp;

              auto t2 = std::chrono::high_resolution_clock::now();
              auto duration =
                  std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1)
                      .count();
              std::cout << "T3 end " << duration << std::endl;
            });
        t3.detach();
      }

      if (meta_acq)
        meta_m.unlock();
      if (depth_acq)
        depth_m.unlock();
      if (color_acq)
        color_m.unlock();
    }
  });
  t2.detach();
}
} // namespace rtabmap

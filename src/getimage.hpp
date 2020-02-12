#pragma once

#include "./Network.hpp"
#include "./decompress.hpp"
#include <mutex>
#include <opencv2/opencv.hpp>

namespace rtabmap {

class GetImage {
  static std::mutex color_m, depth_m, meta_m, mat_m;
  static uint8_t *color_buf, *depth_buf, *meta_buf;
  static int color_frame_id, depth_frame_id, meta_frame_id;
  static int color_file_len, depth_file_len, meta_file_len;

  static cv::Mat color_mat, depth_mat;
  static float *meta_arr;
  static int frame_sync, meta_arr_size, internal_frame;

public:
  static void getImage(cv::Mat &color, cv::Mat &depth, float *arr, float &cx,
                       float &cy, float &fx, float &fy, int &frameID);
  static void initRecv(Network *net);
};

} // namespace rtabmap

#pragma once

#include "decompress.hpp"
#include "jpeglib.h"
#include "network.hpp"
#include "png.h"
#include <mutex>
#include <opencv2/opencv.hpp>

namespace CG {

class GetImage {
  static std::mutex color_m, depth_m, meta_m, mat_m;
  static uint8_t *color_buf, *depth_buf, *meta_buf;
  static int color_frame_id, depth_frame_id, meta_frame_id;
  static int color_file_len, depth_file_len, meta_file_len;

  static uint8_t *color_arr;
  static uint16_t *depth_arr;
  static float *meta_arr;
  static int frame_sync, meta_arr_size, internal_frame;
  static bool isRunning;

public:
  static void getImage(uint8_t *&color, uint16_t *&depth, float *&arr,
                       float &cx, float &cy, float &fx, float &fy,
                       int &frameID);
  static void initRecv(Network *net);
  static void stop();
  static bool run();
};

} // namespace CG

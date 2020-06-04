#include "getimage.hpp"
#include "network.hpp"
#include "server.hpp"
#include <iostream>
#include <mutex>
#include <opencv2/core/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include <thread>

#include <iomanip>
#include <string>

std::string type2str(int type) {
  std::string r;

  uchar depth = type & CV_MAT_DEPTH_MASK;
  uchar chans = 1 + (type >> CV_CN_SHIFT);

  switch (depth) {
  case CV_8U:
    r = "8U";
    break;
  case CV_8S:
    r = "8S";
    break;
  case CV_16U:
    r = "16U";
    break;
  case CV_16S:
    r = "16S";
    break;
  case CV_32S:
    r = "32S";
    break;
  case CV_32F:
    r = "32F";
    break;
  case CV_64F:
    r = "64F";
    break;
  default:
    r = "User";
    break;
  }

  r += "C";
  r += (chans + '0');

  return r;
}

int main(int argc, char **argv) {

  const std::string IP_ADDR = "0.0.0.0";
  int network_ip_port = 12540;
  int network_packet_size = 1024;
  CG::Server *server = new CG::Server(network_ip_port, network_packet_size);

  std::vector<cv::Mat> v_received_rgb;
  v_received_rgb.reserve(10000);
  std::vector<cv::Mat> v_received_depth;
  v_received_depth.reserve(10000);
  std::vector<Eigen::Matrix4f> v_received_pose;
  v_received_pose.reserve(10000);

  size_t seconds = 1;
  bool receiving = false;
  while (server->firstReceived == false) {
    std::cout << receiving << " [DEBUG] Waiting for camera connection ...("
              << seconds++ << "s)\n";

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  }

  server->setImageCallback([&](uint8_t *color, uint16_t *depth, float arr[16],
                               float cx, float cy, float fx, float fy,
                               int frameID) {
    server->firstReceived = true;
    int width = 640, height = 480;
    cv::Mat color_org = cv::Mat(height, width, CV_8UC3, color);
    cv::Mat color_mat = cv::Mat(height, width, CV_8UC3);
    cv::cvtColor(color_org, color_mat, CV_BGR2RGB);
    cv::Mat depth_mat = cv::Mat(height, width, CV_16UC1);
    std::memcpy(depth_mat.data, depth, height * width * 2);

    // server->sendColor(color_org.data, frameID, width, height);
    // server->sendDepth(color_org.data, frameID, width, height);

    float *arr_copy = new float[16];
    memcpy(arr_copy, arr, 16 * sizeof(float));
    Eigen::Matrix4f pose_from_sdk =
        Eigen::Map<Eigen::Matrix<float, 4, 4>>(arr_copy);

    // std::cout << pose_from_sdk << "\n";

    v_received_rgb.emplace_back(cv::Mat(color_mat));
    v_received_depth.emplace_back(cv::Mat(depth_mat));
    v_received_pose.emplace_back(
        Eigen::Map<Eigen::Matrix<float, 4, 4>>(arr_copy));
  });

  size_t wait_until = 10;
  size_t elapsed = 0;
  while (elapsed < wait_until) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    ++elapsed;

    std::cout << "[Info] elapsed = " << elapsed << "s\n";
  }

  std::string dir_dataset = "./received/";
  for (size_t i = 0; i < v_received_rgb.size(); i++) {

    std::cout << "[Info] saving color / depth / pose for frame #" << i
              << "... ";

    std::ostringstream out_filename_rgb;
    out_filename_rgb << dir_dataset << "color/" << std::setw(5)
                     << std::setfill('0') << i << ".png";
    cv::imwrite(out_filename_rgb.str(), v_received_rgb[i]);

    std::ostringstream out_filename_depth;
    out_filename_depth << dir_dataset << "depth/" << std::setw(5)
                       << std::setfill('0') << i << ".png";
    cv::imwrite(out_filename_depth.str(), v_received_depth[i]);

    std::ostringstream out_filename_pose;
    out_filename_pose << dir_dataset << "odometry/" << std::setw(5)
                      << std::setfill('0') << i << ".txt";
    auto out_file_pose = std::ofstream(out_filename_pose.str());
    out_file_pose << v_received_pose[i] << "\n";
    out_file_pose.close();

    std::cout << "done.\n";
  }

  std::exit(EXIT_SUCCESS);
}
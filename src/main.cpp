#include "Network.hpp"
#include "decompress.hpp"
#include "getimage.hpp"
#include <iostream>
#include <mutex>
#include <opencv2/core/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include <thread>

#include <string>

using namespace rtabmap;

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

std::mutex m;
cv::Mat lookUpTable(1, 256, CV_8U);
Network *net;
/* void runSth(uint8_t *data_c, uint8_t *data_d, int color_frame, int
depth_frame, int file_length) { cout << "Processing Color " << endl; Mat img;
    vector<uint8_t> ImVec(data_c, data_c + file_length);

    img = imdecode(ImVec, 1);
    Mat res = img.clone();

    cout << res.cols << res.rows << endl;
    LUT(img, lookUpTable, res);
    if (depth_map != NULL) {
        Mat r1 = Mat(480, 640, CV_32F, depth_map);
        Mat r2 = Mat(480, 640, CV_32FC3);
        res.convertTo(r2, CV_32FC3);
        Mat r3 = Mat(480, 640, CV_32FC3);
        cvtColor(r1, r3, COLOR_GRAY2RGB);
        Mat r4 = r2.mul(r3);
        Mat r5 = Mat(480, 640, CV_8UC3);
        r4.convertTo(r5, CV_8UC3);
        res = r5;
    }
    vector<uint8_t> buff;//buffer for coding
    vector<int> param(2);
    param[0] = cv::IMWRITE_JPEG_QUALITY;
    param[1] = 95;
    imencode(".jpg", res, buff, param);
    file_length = buff.size();

    net->sendColor(color_frame, &buff[0], file_length);
    cout << "Processing Color done " << endl;
} */

int main(int argc, char **argv) {
  if (argc != 4) {
    exit(EXIT_FAILURE);
  }

  uint8_t *p = lookUpTable.ptr();
  for (int i = 0; i < 256; ++i)
    p[i] = cv::saturate_cast<uchar>(pow(i / 255.0, 0.4f) * 255.0);

  char *IP_ADDR = argv[1];
  int IP_PORT = atoi(argv[2]);
  int PACKET_SIZE = atoi(argv[3]);
  net = new Network(IP_ADDR, IP_PORT, PACKET_SIZE, Network::SERVER);
  GetImage::initRecv(net);

  int prev_frame = -1;
  while (1) {
    cv::Mat color, depth;
    float *arr;
    float cx, cy, fx, fy;
    int frameID;

    GetImage::getImage(color, depth, arr, cx, cy, fx, fy, frameID);
    if (frameID != -1) {

      if (prev_frame != frameID) {
        // std::cout << "[DEBUG] received new frame; id = " << frameID << "\n";

        prev_frame = frameID;

        cv::imshow("color_received", color);
        cv::imshow("depth_received", depth);
        cv::imwrite("color_received.jpg", color);
        cv::imwrite("depth_received.png", depth);
        std::exit(0);

        continue;

        int min = 256;
        int max = -1;
        cv::Mat rgb = cv::Mat(480, 640, CV_8UC3);
        // printf("%f %f %f %f\n", cx, cy, fx, fy);

        for (int i = 0; i < 480; i++) {
          for (int j = 0; j < 640; j++) {
            uint16_t elem1 = depth.at<uint16_t>(i, j);
            uint16_t elem2 = ((elem1 & 0xFF) << 8) + ((elem1 & 0xFF00) >> 8);
            float elem = (float)elem2;
            // cout << type2str(depth.type()) << endl;
            int lim0 = 0, lim1 = 64 * 40, lim2 = 128 * 40, lim3 = 192 * 40,
                lim4 = 256 * 40;
            /* int lim0 = 13;
            int lim1 = 23;
            int lim2 = 35;
            int lim3 = 60;
            int lim4 = 90; */

            if (elem == 0) {
              rgb.at<cv::Vec3b>(i, j)[2] = 255;
              rgb.at<cv::Vec3b>(i, j)[1] = 255;
              rgb.at<cv::Vec3b>(i, j)[0] = 255;
            } else if (elem < 64.0 * 40.0) {
              rgb.at<cv::Vec3b>(i, j)[2] = 255;
              rgb.at<cv::Vec3b>(i, j)[1] = static_cast<uint8_t>(
                  (255.0 * (elem - 0.0 * 40.0) / (float)(64.0 * 40.0)));
              rgb.at<cv::Vec3b>(i, j)[0] = 0;
            } else if (elem < 128.0 * 40.0) {
              rgb.at<cv::Vec3b>(i, j)[2] = static_cast<uint8_t>(
                  (255.0 * (elem - 64.0 * 40.0) / (float)(64.0 * 40.0)));
              rgb.at<cv::Vec3b>(i, j)[1] = 255;
              rgb.at<cv::Vec3b>(i, j)[0] = 0;
            } else if (elem < 192.0 * 40.0) {
              rgb.at<cv::Vec3b>(i, j)[2] = 0;
              rgb.at<cv::Vec3b>(i, j)[1] = 255;
              rgb.at<cv::Vec3b>(i, j)[0] = static_cast<uint8_t>(
                  (255.0 * (elem - 128.0 * 40.0) / (float)(64.0 * 40.0)));
            } else {
              rgb.at<cv::Vec3b>(i, j)[2] = 0;
              rgb.at<cv::Vec3b>(i, j)[1] =
                  static_cast<uint8_t>((255.0 * (elem - 192.0 * 40.0) /
                                        (float)(255.0 * 40.0 - 192.0 * 40.0)));
              rgb.at<cv::Vec3b>(i, j)[0] = 255;
            }
          }
        }
        // cout << min << " " << max << endl;

        /*Mat r1 = Mat(480, 640, CV_16UC3);
        cvtColor(depth, r1, COLOR_GRAY2RGB);
        Mat r2 = Mat(480, 640, CV_8UC3);
        r1.convertTo(r2, CV_8UC3);
        r2 = 0.5 * r2;
        r2 = r2 + 0.5 * color; */

        // Mat r2 = color;
        cv::Mat r2 = rgb;
        std::vector<uint8_t> buff;
        std::vector<int> param(2);
        param[0] = cv::IMWRITE_JPEG_QUALITY;
        param[1] = 100;
        cv::imencode(".jpg", r2, buff, param);

        net->sendColor(frameID, &buff[0], buff.size());
      }
    }
  }

  std::exit(EXIT_SUCCESS);
}
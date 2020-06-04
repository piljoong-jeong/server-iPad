#include <cstdint>
#include <iostream>
#include <thread>
#include <type_traits>

#include <Eigen/Dense>
#include <Eigen/Geometry>
#include <Eigen/SVD>

#include <jpeglib.h>
#include <png.h>
#include <zstd.h>

namespace CG {
void boostInit();
void setPriority(std::thread *thread, int priority);
uint8_t *readJPEG(uint8_t *data, int &width, int &height, int data_length);
uint16_t *readPNG(uint8_t *data, int &width, int &height, int data_length);
uint8_t *writeJPEG(uint8_t *img, int width, int height,
                   unsigned long &file_length);
uint8_t *writePNG(uint16_t *img, int width, int height,
                  unsigned long &file_length);
void writeFile(uint8_t *data, int data_length, char *name);
uint8_t *writeZSTD(uint16_t *img, int width, int height,
                   unsigned long &file_length);
uint16_t *readZSTD(uint8_t *file, unsigned long file_length);
uint8_t *readZSTDColor(uint8_t *file, unsigned long file_length);
uint8_t *writeZSTDColor(uint8_t *img, int width, int height,
                        unsigned long &file_length);
} // namespace CG

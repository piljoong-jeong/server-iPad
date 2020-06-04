#include "common.h"
#if defined(_MSC_VER)
#include <Windows.h>
#endif

#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
#include <pthread.h>
#include <sched.h>
#include <sys/resource.h>
#endif

namespace CG {
void boostInit() {
#if defined(_MSC_VER)
  // auto mask = (static_cast<DWORD_PTR>(1) << 0);
  // SetProcessAffinityMask(GetCurrentProcess(), mask);
  SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
#elif defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
  setpriority(PRIO_PROCESS, 0, -20);
  // cpu_set_t mask;
  // CPU_ZERO(&mask);
  // CPU_SET(0, &mask);
  // sched_setaffinity(0, sizeof(mask), &mask);

  struct sched_param params;
  int policy;
  pthread_getschedparam(pthread_self(), &policy, &params);
  params.sched_priority = sched_get_priority_min(policy);
  pthread_setschedparam(pthread_self(), policy, &params);
#else
  printf("Unrecognized OS\n");
#endif
}

void setPriority(std::thread *thread, int priority) {
  if (priority >= std::thread::hardware_concurrency()) {
  } else {
#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(priority, &cpuset);
    pthread_setaffinity_np(thread->native_handle(), sizeof(cpu_set_t), &cpuset);
    printf("Pthread affinity : %d\n", priority);
#elif defined(_MSC_VER)
    auto mask = (static_cast<DWORD_PTR>(1) << priority);
    SetThreadAffinityMask(thread->native_handle(), mask);
    printf("Winthread affinity : %d\n", priority);
#else
    printf("Warning : Thread affinity was not set to sender function");
#endif
  }

#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
  struct sched_param params;
  int policy;
  pthread_getschedparam(thread->native_handle(), &policy, &params);
  params.sched_priority = sched_get_priority_min(policy);
  pthread_setschedparam(thread->native_handle(), policy, &params);
#elif defined(_MSC_VER)
  SetThreadPriority(thread->native_handle(), THREAD_PRIORITY_HIGHEST);
#endif
}

uint8_t *readJPEG(uint8_t *data, int &width, int &height, int data_length) {
  // gist.github.com/PhirePhly/3080633

  jpeg_decompress_struct cinfo;
  jpeg_error_mgr jerr;

  unsigned long bmp_size;
  uint8_t *bmp_buffer;
  int row_stride, pixel_size;

  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_decompress(&cinfo);

  uint8_t *jpeg_mem = (uint8_t *)malloc(data_length);
  memcpy(jpeg_mem, data, data_length);
  jpeg_mem_src(&cinfo, jpeg_mem, data_length);
  int rc = jpeg_read_header(&cinfo, TRUE);

  width = 640;
  height = 480;
  pixel_size = 3;
  bmp_size = width * height * pixel_size;

  bmp_buffer = (uint8_t *)malloc(bmp_size);

  row_stride = width * pixel_size;
  jpeg_start_decompress(&cinfo);

  while (cinfo.output_scanline < height) {
    unsigned char *buffer_array[1];
    buffer_array[0] = bmp_buffer + (cinfo.output_scanline) * row_stride;
    jpeg_read_scanlines(&cinfo, buffer_array, 1);
  }

  jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);

  uint8_t *retVal = new uint8_t[bmp_size];
  memcpy(retVal, bmp_buffer, bmp_size);

  free(bmp_buffer);
  free(jpeg_mem);

  return retVal;
}

struct mem_decode {
  char *buffer;
  size_t now;
  size_t fileSize;
};

void readData(png_structp png, png_bytep data, png_size_t length) {
  mem_decode *ptr = (mem_decode *)png_get_io_ptr(png);
  if (ptr->now + length <= ptr->fileSize)
    memcpy(data, ptr->buffer + ptr->now, length);
  else {
    printf("WARNING\n");
    memcpy(data, ptr->buffer + ptr->now, (ptr->fileSize - ptr->now));
  }

  ptr->now += length;
}

uint16_t *readPNG(uint8_t *data, int &width, int &height, int data_length) {
  mem_decode mem;
  mem.buffer = reinterpret_cast<char *>(data);
  mem.now = 0;
  mem.fileSize = data_length;

  png_structp png =
      png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  png_infop info = png_create_info_struct(png);

  png_set_read_fn(png, &mem, readData);
  png_read_info(png, info);
  width = png_get_image_width(png, info);
  height = png_get_image_height(png, info);
  int color_type = png_get_color_type(png, info);
  int bit_depth = png_get_bit_depth(png, info);

  png_read_update_info(png, info);
  png_bytep *row_pointer = (png_bytep *)malloc(sizeof(png_bytep) * height);
  int rowbytes = png_get_rowbytes(png, info);
  for (int y = 0; y < height; y++) {
    row_pointer[y] = (png_byte *)malloc(png_get_rowbytes(png, info));
  }

  png_read_image(png, row_pointer);
  png_destroy_read_struct(&png, &info, nullptr);

  uint16_t *retVal = new uint16_t[width * height];
  for (int i = 0; i < height; i++) {
    memcpy(retVal + i * width, row_pointer[i], rowbytes);
    free(row_pointer[i]);
  }

  free(row_pointer);
  return retVal;
}

uint16_t *readZSTD(uint8_t *file, unsigned long file_length) {
  unsigned long long const rSize = ZSTD_getFrameContentSize(file, file_length);
  uint8_t *buffer = new uint8_t[rSize];
  size_t const dSize = ZSTD_decompress(buffer, rSize, file, file_length);
  uint16_t *retVal = new uint16_t[rSize / 2];
  memcpy(retVal, buffer, rSize);

  delete[] buffer;
  return retVal;
}

uint8_t *readZSTDColor(uint8_t *file, unsigned long file_length) {
  unsigned long long const rSize = ZSTD_getFrameContentSize(file, file_length);
  uint8_t *buffer = new uint8_t[rSize];
  size_t const dSize = ZSTD_decompress(buffer, rSize, file, file_length);

  return buffer;
}

uint8_t *writeZSTD(uint16_t *img, int width, int height,
                   unsigned long &file_length) {
  uint8_t *readBuffer = new uint8_t[2 * width * height];
  memcpy(readBuffer, img, 2 * width * height);
  uint8_t *buffer = new uint8_t[2 * width * height];
  size_t const cSize = ZSTD_compress(buffer, 2 * width * height, readBuffer,
                                     2 * width * height, 1);
  uint8_t *retVal = new uint8_t[cSize];
  memcpy(retVal, buffer, cSize);

  file_length = cSize;
  delete[] buffer;
  delete[] readBuffer;
  return retVal;
}

uint8_t *writeZSTDColor(uint8_t *img, int width, int height,
                        unsigned long &file_length) {
  uint8_t *buffer = new uint8_t[3 * width * height];
  size_t const cSize =
      ZSTD_compress(buffer, 3 * width * height, img, 3 * width * height, 1);
  uint8_t *retVal = new uint8_t[cSize];
  memcpy(retVal, buffer, cSize);

  file_length = cSize;
  delete[] buffer;
  return retVal;
}

uint8_t *writeJPEG(uint8_t *img, int width, int height,
                   unsigned long &file_length) {
  jpeg_compress_struct cinfo;
  jpeg_error_mgr jerr;

  JSAMPROW row_pointer[1];
  int row_stride;

  int output_size;
  uint8_t *output = nullptr;

  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_compress(&cinfo);

  jpeg_mem_dest(&cinfo, &output, &file_length);

  cinfo.image_width = width;
  cinfo.image_height = height;
  cinfo.input_components = 3;
  cinfo.in_color_space = JCS_RGB;

  jpeg_set_defaults(&cinfo);
  jpeg_set_quality(&cinfo, 90, TRUE);

  jpeg_start_compress(&cinfo, TRUE);
  row_stride = cinfo.image_width * cinfo.input_components;
  while (cinfo.next_scanline < cinfo.image_height) {
    row_pointer[0] = &img[cinfo.next_scanline * row_stride];
    jpeg_write_scanlines(&cinfo, row_pointer, 1);
  }

  jpeg_finish_compress(&cinfo);
  jpeg_destroy_compress(&cinfo);

  uint8_t *retVal = new uint8_t[file_length];
  memcpy(retVal, output, file_length);

  return retVal;
}

// https://stackoverflow.com/questions/1821806

struct mem_encode {
  char *buffer;
  size_t size;
};

void my_png_flush(png_structp png_ptr) {}
void my_png_write_data(png_structp png_ptr, png_bytep data, png_size_t length) {
  struct mem_encode *p = (struct mem_encode *)png_get_io_ptr(png_ptr);
  size_t nsize = p->size + length;

  if (p->buffer)
    p->buffer = (char *)realloc(p->buffer, nsize);
  else
    p->buffer = (char *)malloc(nsize);

  memcpy(p->buffer + p->size, data, length);
  p->size += length;
}

uint8_t *writePNG(uint16_t *img, int width, int height,
                  unsigned long &file_length) {
  struct mem_encode mem;
  mem.buffer = nullptr;
  mem.size = 0;
  png_structp png =
      png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  png_infop info = png_create_info_struct(png);

  png_set_write_fn(png, &mem, my_png_write_data, my_png_flush);

  png_set_IHDR(png, info, width, height, 16, PNG_COLOR_TYPE_GRAY,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
               PNG_FILTER_TYPE_DEFAULT);
  png_set_compression_level(png, 3);

  png_write_info(png, info);
  png_set_swap(png);

  for (int i = 0; i < height; i++) {
    png_write_row(png, (png_const_bytep)&img[i * width]);
  }
  png_write_end(png, info);

  png_destroy_write_struct(&png, &info);

  file_length = mem.size;
  uint8_t *retVal = new uint8_t[mem.size];
  memcpy(retVal, mem.buffer, mem.size);
  free(mem.buffer);

  return retVal;
}

void writeFile(uint8_t *data, int data_length, char *name) {
  FILE *f = fopen(name, "wb");
  fwrite(data, 1, data_length, f);
  fclose(f);
}
} // namespace CG

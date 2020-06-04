#include <functional>
#include <list>
#include <thread>

#include "getimage.hpp"
#include "network.hpp"

namespace CG {

class Server {
  using ImageCallback =
      std::function<void(uint8_t *color, uint16_t *depth, float arr[16],
                         float cx, float cy, float fx, float fy, int frameID)>;
  using EndCallback = std::function<void(void)>;

public:
  Server(int IP_PORT, int PACKET_SIZE);
  ~Server();

  void setImageCallback(ImageCallback image) {
    callbacks_.image.push_back(image);
  }

  void setEndCallback(EndCallback end) { callbacks_.end.push_back(end); }

  void clearImageCallback() { callbacks_.image.clear(); }

  void sendColor(uint8_t *data, int frameID, int width, int height);
  void sendDepth(uint8_t *data, int frameID, int width, int height);
  bool firstReceived = false;

protected:
  struct {
    std::list<ImageCallback> image;
    std::list<EndCallback> end;
  } callbacks_;

private:
  Network *net;
  std::thread *thr;
  static bool isRunning;
};

} // namespace CG

#include <cstdio>

#include "pigpio.h"

#include "Uv.h"

using namespace common;

namespace {

struct Pin {
  const char *name;
  unsigned gpio;
};

constexpr Pin red{"red", 0};
constexpr Pin green{"green", 1};
constexpr Pin blue{"blue", 4};
constexpr Pin white{"white", 17};

constexpr Pin pins[] = {red, green, blue, white};

struct GPIOGuard {
  ~GPIOGuard() { gpioTerminate(); }
};

void static_buffer_alloc_cb(size_t, uv_buf_t * buf) {
  constexpr size_t length = 64*1024;
  static char buffer[length];
  buf->base = buffer;
  buf->len = length;
}

}

int main(int argc, char **argv) {
  using namespace std::literals;

  if(gpioInitialise() < 0) {
    fprintf(stderr, "GPIO initialization failed\n");
    return 1;
  }
  GPIOGuard guard;

  for(auto &pin : pins) {
    gpioSetMode(pin.gpio, PI_OUTPUT);
    gpioWrite(pin.gpio, true);
  }

  uv::Loop loop;
  uv::UDP udp(loop);
  uv::Timer timer(loop);

  auto shutdown_cb = [&](int){
    udp.close();
    timer.close();

  };

  uv::Signal sigint(loop, shutdown_cb, SIGINT);
  sigint.unref();
  uv::Signal sigterm(loop, shutdown_cb, SIGTERM);
  sigterm.unref();

  struct sockaddr_in6 addr {};
  uv_ip6_addr("::", 4242, &addr);
  udp.bind(reinterpret_cast<struct sockaddr *>(&addr));
  udp.recvStart(static_buffer_alloc_cb, [&](ssize_t result, const uv_buf_t *buf, const struct sockaddr *cAddr, unsigned flags) {
      if(timer.active())
        timer.stop();

      if(result < 0) {
        fprintf(stderr, "read error: %s\n", uv_strerror(result));
        shutdown_cb(0);
        return;
      }

      if(result != 4) {
        fprintf(stderr, "illegal %d-byte command received\n", result);
        return;
      }
      printf("set:");
      for(int i = 0; i < 4; ++i) {
        printf(" %s=%d", pins[i].name, buf->base[i]);
        gpioPWM(pins[i].gpio, buf->base[i]);
      }
      printf("\n");
    });

  unsigned i = 0;

  timer.start([&](){
      gpioWrite(pins[i].gpio, true);
      i = (i + 1) % 4;
      gpioWrite(pins[i].gpio, false);
    },
    0ms, 500ms);

  loop.run();

  sigint.close();
  sigterm.close();
  loop.run();
}

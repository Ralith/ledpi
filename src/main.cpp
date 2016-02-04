#include <cstdio>

#include "pigpio.h"

#include "Uv.h"

using namespace common;

namespace {

struct GPIOGuard {
  ~GPIOGuard() { gpioTerminate(); }
};

namespace pin {

static constexpr unsigned R = 0, G = 1, B = 4, W = 17;

const char *name[4] = {"red", "green", "blue", "white"};
unsigned pins[4] = {pin::R, pin::G, pin::B, pin::W};

void init(unsigned pin) {
  gpioSetMode(pin, PI_OUTPUT);
  gpioWrite(pin, true);
}

}

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

  pin::init(pin::R);
  pin::init(pin::G);
  pin::init(pin::B);
  pin::init(pin::W);

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
        printf(" %s=%d", pin::name[i], buf->base[i]);
        gpioPWM(pin::pins[i], buf->base[i]);
      }
      printf("\n");
    });

  unsigned i = 0;

  timer.start([&](){
      gpioWrite(pin::pins[i], true);
      i = (i + 1) % 4;
      gpioWrite(pin::pins[i], false);
    },
    0ms, 500ms);

  loop.run();

  sigint.close();
  sigterm.close();
  loop.run();
}

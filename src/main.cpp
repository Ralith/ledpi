#include <cstdio>
#include <limits>

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

using Power = uint8_t;
constexpr Power POWER_MAX = std::numeric_limits<Power>::max();

struct State {
  Power power[4];
};

void apply(const State &state) {
  for(int i = 0; i < 4; ++i) {
    gpioPWM(pins[i].gpio, POWER_MAX - state.power[i]);
  }
}

}

int main(int, char **) {
  if(gpioInitialise() < 0) {
    fprintf(stderr, "GPIO initialization failed\n");
    return 1;
  }
  GPIOGuard guard;

  // Init and shut off all channels
  for(auto &pin : pins) {
    gpioSetMode(pin.gpio, PI_OUTPUT);
    gpioWrite(pin.gpio, true);
  }

  // Default warm white
  State state = {{POWER_MAX, POWER_MAX / 10, 0, POWER_MAX / 4}};
  apply(state);

  uv::Loop loop;
  uv::UDP udp(loop);

  auto shutdown_cb = [&](int){
    udp.close();
  };

  uv::Signal sigint(loop, shutdown_cb, SIGINT);
  sigint.unref();
  uv::Signal sigterm(loop, shutdown_cb, SIGTERM);
  sigterm.unref();

  struct sockaddr_in6 addr {};
  uv_ip6_addr("::", 4242, &addr);
  udp.bind(reinterpret_cast<struct sockaddr *>(&addr));
  udp.recvStart(static_buffer_alloc_cb, [&](ssize_t result, const uv_buf_t *buf, const struct sockaddr *cAddr, unsigned flags) {
      (void)flags;
      if(result < 0) {
        fprintf(stderr, "read error: %s\n", uv_strerror(result));
        shutdown_cb(0);
        return;
      }

      if(result != 4) {
        if(cAddr != nullptr) {
          fprintf(stderr, "illegal %zd-byte command received\n", result);
        }
        return;
      }
      printf("set:");
      for(int i = 0; i < 4; ++i) {
        printf(" %s=%d", pins[i].name, buf->base[i]);
        state.power[i] = POWER_MAX - buf->base[i];
      }
      apply(state);
      printf("\n");
    });

  loop.run();

  sigint.close();
  sigterm.close();

  // Cleanup iteration
  loop.run();
}

#include <cstdio>

#include <capnp/serialize.h>

#include "pigpio.h"
#include "Uv.h"
#include "command.capnp.h"
#include "state.capnp.h"

using namespace common;

namespace {

constexpr size_t N_CHANNELS = 4;

struct Pin {
  const char *name;
  unsigned gpio;
};

constexpr Pin red{"red", 0};
constexpr Pin green{"green", 1};
constexpr Pin blue{"blue", 4};
constexpr Pin white{"white", 17};

constexpr Pin pins[N_CHANNELS] = {red, green, blue, white};

struct GPIOGuard {
  ~GPIOGuard() { gpioTerminate(); }
};

void static_buffer_alloc_cb(size_t, uv_buf_t * buf) {
  constexpr size_t length = 64*1024;
  static char buffer[length];
  buf->base = buffer;
  buf->len = length;
}

using Power = uint16_t;
constexpr Power POWER_MAX = PI_MAX_DUTYCYCLE_RANGE;

struct State {
  Power power[N_CHANNELS];
};

void apply(const State &state) {
  printf("set:");
  for(size_t i = 0; i < N_CHANNELS; ++i) {
    printf(" %s=%d", pins[i].name, state.power[i]);
    gpioPWM(pins[i].gpio, POWER_MAX - state.power[i]);
  }
  printf("\n");
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
    gpioSetPWMrange(pin.gpio, POWER_MAX);
  }

  // Default warm white
  State state = {{POWER_MAX, POWER_MAX / 10, 0, POWER_MAX / 4}};
  apply(state);

  uv::Loop loop;
  uv::UDP udp(loop);

  auto shutdown_cb = [&](int){
    udp.close();
    for(auto &x : state.power) {
      x = 0;
    }
    apply(state);
  };

  uv::Signal sigint(loop, shutdown_cb, SIGINT);
  sigint.unref();
  uv::Signal sigterm(loop, shutdown_cb, SIGTERM);
  sigterm.unref();

  struct sockaddr_in6 addr;
  uv_ip6_addr("::", 4242, &addr);
  udp.bind(reinterpret_cast<struct sockaddr *>(&addr));
  udp.recvStart(static_buffer_alloc_cb, [&](ssize_t result, const uv_buf_t *buf, const struct sockaddr *cAddr, unsigned flags) {
      (void)flags;
      if(result < 0) {
        fprintf(stderr, "read error: %s\n", uv_strerror(result));
        shutdown_cb(0);
        return;
      }

      if(result == 0 && cAddr == nullptr)
        return;

      if(result % sizeof(capnp::word)) {
        fprintf(stderr, "malformed message: size %zu not a multiple of %zu\n", result, sizeof(capnp::word));
      }

      capnp::SegmentArrayMessageReader reader({kj::arrayPtr(reinterpret_cast<const capnp::word*>(buf->base),
                                                            buf->len/sizeof(capnp::word))});

      try {
        auto msg = reader.getRoot<proto::Command>();
        switch(msg.which()) {
        case proto::Command::SET_POWER:
          for(auto instr : msg.getSetPower()) {
            size_t channel = instr.getChannel();
            if(channel > 4) {
              printf("message attempted to modify nonexistent channel %zu\n", channel);
              continue;
            }
            switch(instr.which()) {
            case proto::Command::SetPower::SET:
              state.power[channel] = POWER_MAX * instr.getSet() / UINT16_MAX;
              break;
            case proto::Command::SetPower::MULTIPLY:
              state.power[channel] *= std::max(static_cast<float>(0), std::min(static_cast<float>(POWER_MAX), instr.getMultiply()));
              break;
            }
          }
          apply(state);
          break;
        default:
          puts("unimplemented command");
          break;
        }
      } catch(kj::Exception & e) {
        printf("malformed message: %s\n", e.getDescription().cStr());
      }
    });

  loop.run();

  sigint.close();
  sigterm.close();

  // Cleanup iteration
  loop.run();
}

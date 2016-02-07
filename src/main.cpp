#include <cstdio>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <vector>
#include <memory>

#include <capnp/serialize.h>
#include <capnp/serialize-packed.h>

#include "pigpio.h"
#include "Uv.h"
#include "command.capnp.h"
#include "state.capnp.h"

using namespace common;

namespace {

const char *STATE_PATH = "/var/lib/ledpi-state";
const char *TMP_STATE_PATH = "/var/lib/ledpi-state.tmp";

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

void apply(proto::State::Builder state) {
  auto levels = state.getLevels();
  auto channels = state.getChannels();
  printf("set:");
  for(size_t i = 0; i < levels.size(); ++i) {
    printf(" %s=%d", channels[i].getName().cStr(), levels[i]);
    gpioPWM(channels[i].getGpio(), (static_cast<uint32_t>(UINT16_MAX - levels[i]) * PI_MAX_DUTYCYCLE_RANGE) / UINT16_MAX);
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

  uv::Loop loop;
  uv::UDP udp(loop);

  struct sockaddr_in6 addr;
  uv_ip6_addr("::", 4242, &addr);
  udp.bind(reinterpret_cast<struct sockaddr *>(&addr));

  printf("loading state...");
  fflush(stdout);

  int state_fd = open(STATE_PATH, O_RDONLY);
  if(state_fd < 0) {
    if(errno == ENOENT) {
      state_fd = open(TMP_STATE_PATH, O_RDONLY);
      if(state_fd >= 0) {
        int res = rename(TMP_STATE_PATH, STATE_PATH);
        if(res >= 0) goto success;
      }
    }

    fprintf(stderr, "failed to open state file at %s: %s\n", STATE_PATH, strerror(errno));
    return 1;

  success:
    (void)0;
  }

  capnp::MallocMessageBuilder state_builder;

  try {
    capnp::PackedFdMessageReader reader{kj::AutoCloseFd(state_fd)};
    state_builder.setRoot(reader.getRoot<proto::State>());
  } catch(kj::Exception & e) {
    fprintf(stderr, "failed to load state file: %s\n", e.getDescription().cStr());
    return 1;
  }

  auto state = state_builder.getRoot<proto::State>();
  puts(" done");

  // Set up PWM outputs
  for(auto channel : state.getChannels()) {
    gpioSetPWMrange(channel.getGpio(), PI_MAX_DUTYCYCLE_RANGE);
    gpioPWM(channel.getGpio(), PI_MAX_DUTYCYCLE_RANGE);
  }

  apply(state);

  auto shutdown_cb = [&](int){
    udp.close();

    // Shut off LEDs
    auto levels = state.getLevels();
    std::vector<Power> old_levels(levels.size());
    for(size_t i = 0; i < levels.size(); ++i) {
      old_levels[i] = levels[i];
      levels.set(i, 0);
    }
    apply(state);
    for(size_t i = 0; i < levels.size(); ++i) {
      levels.set(i, old_levels[i]);
    }
  };

  uv::Signal sigint(loop, shutdown_cb, SIGINT);
  sigint.unref();
  uv::Signal sigterm(loop, shutdown_cb, SIGTERM);
  sigterm.unref();

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
              state.getLevels().set(channel, instr.getSet());
              break;
            case proto::Command::SetPower::MULTIPLY:
              state.getLevels().set(channel, state.getLevels()[channel] * instr.getMultiply());
              break;
            }
          }
          apply(state);
          break;

        case proto::Command::SET_NAME:
          state.setName(msg.getSetName());
          break;

        case proto::Command::GET_NAME: {
          auto response_builder = std::make_shared<capnp::MallocMessageBuilder>();
          auto response_msg = response_builder->initRoot<proto::Response>();
          response_msg.setName(state.getName().asString());

          auto send_req = std::make_shared<uv::UDPSend>();
          auto segs = response_builder->getSegmentsForOutput();
          std::vector<uv_buf_t> bufs(segs.size());
          for(size_t i = 0; i < segs.size(); ++i) {
            bufs[i].base = const_cast<char*>(reinterpret_cast<const char*>(segs[i].begin()));
            bufs[i].len = sizeof(segs[i]) * segs[i].size();
          }
          send_req->send(udp, &bufs[0], bufs.size(), cAddr,
                         [response_builder, send_req](int result) {
                           if(result < 0) {
                             fprintf(stderr, "error sending response: %s\n", uv_strerror(result));
                           }
                         });

          break;
        }

        default:
          puts("unsupported command");
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

  // Save state
  printf("saving state...");
  fflush(stdout);
  state_fd = open(TMP_STATE_PATH, O_WRONLY | O_CREAT);
  if(state_fd < 0) {
    fprintf(stderr, "failed to open state file at %s: %s\n", TMP_STATE_PATH, strerror(errno));
    return 1;
  }
  capnp::writePackedMessageToFd(state_fd, state_builder);
  fsync(state_fd);
  close(state_fd);
  int res = rename(TMP_STATE_PATH, STATE_PATH);
  if(res < 0) {
    fprintf(stderr, "failed to store state file to %s: %s\n", STATE_PATH, strerror(errno));
    return 1;
  }
  puts(" done");
}

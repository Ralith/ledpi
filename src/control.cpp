#include <cstdio>
#include <cstring>
#include <vector>

#include <capnp/message.h>

#include "Uv.h"
#include "command.capnp.h"

using namespace common;

constexpr const char* channels[] = {"red", "green", "blue", "white"};

int main(int argc, char **argv) {
  if(argc != 5) {
    fprintf(stderr, "Usage: %s <channel>{4}\n\tchannel = real | \"x\" real\n", argv[0]);
    return 1;
  }

  capnp::MallocMessageBuilder message;
  auto cmd = message.initRoot<proto::Command>();
  auto levels = cmd.initSetPower(4);

  for(int i = 0; i < 4; ++i) {
    const char *arg = argv[i+1];

    bool multiply = false;
    if(arg[0] == 'x') {
      multiply = true;
      arg += 1;
    }

    char *endptr;
    float x = strtof(arg, &endptr);

    if(endptr != arg + strlen(arg)) {
      fprintf(stderr, "invalid %s channel intensity (should be a real): %s\n", channels[i], arg);
      return 1;
    }

    if(!multiply && (x < 0 || x > 1)) {
      fprintf(stderr, "invalid %s channel absolute intensity (should be between 0 and 1 inclusive): %s\n", channels[i], arg);
      return 1;
    }

    levels[i].setChannel(i);
    if(multiply) {
      levels[i].setMultiply(x);
    } else {
      levels[i].setSet(x * UINT16_MAX);
    }
  }

  auto segments = message.getSegmentsForOutput();
  std::vector<uv_buf_t> bufs(segments.size());
  for(size_t i = 0; i < segments.size(); ++i) {
    bufs[i].len = segments[i].size() * sizeof(segments[i][0]);
    bufs[i].base = const_cast<char*>(reinterpret_cast<const char*>(segments[i].begin()));
  }

  uv::Loop loop;
  uv::UDP udp(loop);
  uv::UDPSend send;

  struct sockaddr_in6 localAddr{};
  uv_ip6_addr("::", 0, &localAddr);
  udp.bind(reinterpret_cast<struct sockaddr *>(&localAddr));
  udp.set_broadcast(true);

  struct sockaddr_in remoteAddr{};
  uv_ip4_addr("255.255.255.255", 4242, &remoteAddr);

  int rc = 0;
  send.send(udp, &bufs[0], bufs.size(), reinterpret_cast<struct sockaddr *>(&remoteAddr), [&](int result){
      if(result < 0) {
        fprintf(stderr, "failed to send command: %s\n", uv_strerror(result));
        rc = 1;
      }
      udp.close();
    });
  loop.run();

  return rc;
}

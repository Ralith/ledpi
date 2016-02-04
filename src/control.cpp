#include <cstdio>
#include <cstring>

#include "Uv.h"

using namespace common;

constexpr const char* channels[] = {"red", "green", "blue", "white"};

int main(int argc, char **argv) {
  if(argc != 5) {
    fprintf(stderr, "specify intensity scale for each channel\n");
    return 1;
  }

  char data[4];
  uv_buf_t buf;
  buf.base = data;
  buf.len = sizeof(data);

  for(int i = 0; i < 4; ++i) {
    char *endptr;
    float x = strtof(argv[i+1], &endptr);

    if(endptr != argv[i+1] + strlen(argv[i+1])) {
      fprintf(stderr, "invalid %s channel intensity (should be a real): %s\n", channels[i], argv[i+1]);
      return 1;
    }

    if(x < 0 || x > 1) {
      fprintf(stderr, "invalid %s channel intensity (should be between 0 and 1 inclusive): %s\n", channels[i], argv[i+1]);
      return 1;
    }

    data[i] = x * 255;
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
  send.send(udp, &buf, 1, reinterpret_cast<struct sockaddr *>(&remoteAddr), [&](int result){
      if(result < 0) {
        fprintf(stderr, "failed to send command: %s\n", uv_strerror(result));
        rc = 1;
      }
      udp.close();
    });
  loop.run();

  return rc;
}

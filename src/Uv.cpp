#include "Uv.h"

#include <cassert>
#include <cstdio>
#include <memory>
#include <string>

using namespace std;
using namespace common::uv;


static void walk_cb(uv_handle_t *handle, void *)
{
  // See uv_handle_type in uv.h
#define XX(uc, lc) #lc,
  static const char *const handle_names[] = {
    "unknown handle",
    UV_HANDLE_TYPE_MAP(XX)
    "file",
    nullptr
  };
#undef XX
  fprintf(stderr, "handle: %s active: %s\n", handle_names[handle->type],
          uv_is_active(handle) ? "true" : "false");
}

void Loop::dump() {
  uv_walk(&loop, walk_cb, nullptr);
}

void Signal::callback_(uv_signal_t *handle, int signum) {
  Signal &self = *reinterpret_cast<Signal*>(handle);
  self.func_(signum);
}

void Async::callback(uv_async_t *handle) {
  Async &self = *reinterpret_cast<Async*>(handle);
  self.func();
}

void FsEvent::callback(uv_fs_event_t *handle, const char *filename, int events, int status) {
  FsEvent &self = *reinterpret_cast<FsEvent*>(handle);
  self.func_(filename, events, status);
}

string FsEvent::path() {
  char buf[1024] = "";
  size_t len = sizeof(buf);

  assert(!empty);
  switch(uv_fs_event_getpath(&handle, buf, &len)) {
  case UV_ENOBUFS: {
      std::unique_ptr<char[]> temp(static_cast<char*>(::operator new(len)));
      uv_fs_event_getpath(&handle, temp.get(), &len);
      return string(temp.get(), len);
    }
  case UV_EINVAL: return "";
  }

  return string(buf, len);
}

void Timer::callback(uv_timer_t *handle) {
  Timer &self = *reinterpret_cast<Timer*>(handle);
  self.func_();
}

void Poll::callback_(uv_poll_t *handle, int status, int events) {
  Poll &self = *reinterpret_cast<Poll*>(handle);
  self.func_(status, events);
}

void UDP::allocCB_(uv_handle_t *handle, size_t suggestedSize, uv_buf_t *buf) {
  UDP &self = *reinterpret_cast<UDP*>(handle);
  self.allocFunc_(suggestedSize, buf);
}

void UDP::recvCB_(uv_udp_t *handle, ssize_t result, const uv_buf_t *buf, const struct sockaddr *cAddr, unsigned flags) {
  UDP &self = *reinterpret_cast<UDP*>(handle);
  self.recvFunc_(result, buf, cAddr, flags);
}

void UDPSend::finish_(uv_udp_send_t *req, int status) {
  UDPSend &self = *reinterpret_cast<UDPSend*>(req);
  self.finishCB_(status);
}

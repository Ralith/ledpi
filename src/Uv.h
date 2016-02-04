#ifndef COMMON_UV_H
#define COMMON_UV_H

#include <uv.h>
#include <functional>
#include <cassert>
#include <chrono>
#include <string>

namespace common {
namespace uv {

struct Clock {
  typedef uint64_t rep;
  typedef std::chrono::milliseconds::period period;
  typedef std::chrono::duration<rep, period> duration;
  typedef std::chrono::time_point<Clock> time_point;
  constexpr static bool is_steady = true;

  [[deprecated("inefficient; use Loop::now() instead")]]
  time_point now() noexcept;
};

struct HRClock {
  typedef uint64_t rep;
  typedef std::chrono::nanoseconds::period period;
  typedef std::chrono::duration<rep, period> duration;
  typedef std::chrono::time_point<HRClock> time_point;
  constexpr static bool is_steady = true;
  time_point now() noexcept { return time_point{duration{uv_hrtime()}}; }
};

class Loop {
  uv_loop_t loop;

public:
  Loop() {
    uv_loop_init(&loop);
  }
  ~Loop() {
    int res = uv_loop_close(&loop);
    assert(res != UV_EBUSY);
    (void)res;
  }

  Loop(const Loop &) = delete;
  Loop(Loop &&) = delete;
  Loop &operator=(const Loop &) = delete;
  Loop &operator=(Loop &&) = delete;

  operator uv_loop_t *() { return &loop; }

  uv_loop_t &value() {
    return loop;
  }

  int run(uv_run_mode mode = UV_RUN_DEFAULT) {
    return uv_run(&loop, mode);
  }

  void stop() {
    uv_stop(&loop);
  }

  bool alive() const { return uv_loop_alive(&loop); }

  void dump();

  Clock::time_point now() const { return Clock::time_point{Clock::duration{uv_now(&loop)}}; }
};

inline Clock::time_point Clock::now() noexcept {
  Loop loop;
  return loop.now();
}

template<typename T>
class Handle {
protected:
  T handle;  // Must be the first element
  bool empty = false;

private:
  std::function<void()> close_func_;
  static void close_callback_(uv_handle_t *handle);

public:
  Handle() {}
  Handle(Handle&&) = delete;
  Handle(const Handle&) = delete;
  ~Handle() { assert(empty); }

  Handle &operator=(const Handle &) = delete;
  Handle &operator=(Handle &&) = delete;

  void close() {
    assert(!empty);
    uv_close(reinterpret_cast<uv_handle_t*>(&handle), nullptr);
    empty = true;
  }

  void close(std::function<void()> &&close_func) {
    assert(!empty);
    close_func_ = std::move(close_func);
    uv_close(reinterpret_cast<uv_handle_t*>(&handle), close_callback_);
    empty = true;
  }

  void unref() { uv_unref(reinterpret_cast<uv_handle_t*>(&handle)); }

  bool active() const { assert(!empty); return uv_is_active(reinterpret_cast<const uv_handle_t*>(&handle)); }
};

template<typename T>
inline void Handle<T>::close_callback_(uv_handle_t *uvhandle) {
  auto &handle = reinterpret_cast<Handle<T>&>(*uvhandle);
  handle.close_func_();
}

class Signal : public Handle<uv_signal_t> {
  std::function<void (int signum)> func_;

  static void callback_(uv_signal_t *handle, int signum);

public:
  Signal(Loop &loop) {
    uv_signal_init(loop, &handle);
  }

  Signal(Loop &loop, std::function<void (int signum)> func, int signum)
      : Signal(loop) {
    start(std::move(func), signum);
  }

  void start(std::function<void (int signum)> func, int signum) {
    func_ = std::move(func);
    uv_signal_start(&handle, callback_, signum);
  }

  void stop() { uv_signal_stop(&handle); }
};

class Async : public Handle<uv_async_t> {
  std::function<void ()> func;

  static void callback(uv_async_t *handle);

public:
  Async(Loop &loop, std::function<void ()> func)
    : func(std::move(func)) {
    uv_async_init(&loop.value(), &handle, &callback);
  }

  void send() { assert(!empty); uv_async_send(&handle); }
};

class FsEvent : public Handle<uv_fs_event_t> {
  std::function<void (const char *path, int events, int status)> func_;

  static void callback(uv_fs_event_t *handle, const char *filename, int events, int status);

public:
  FsEvent(Loop &loop) {
    uv_fs_event_init(&loop.value(), &handle);
  }
  FsEvent(Loop &loop, std::function<void (const char*, int, int)> func, const char *path)
    : FsEvent(loop) {
    int res = start(std::move(func), path);
    assert(!res);
    (void)res;
  }

  int start(std::function<void (const char*, int, int)> func, const char *path, unsigned flags = 0) {
    func_ = std::move(func);
    return uv_fs_event_start(&handle, callback, path, flags);
  }

  void stop() {
    uv_fs_event_stop(&handle);
  }

  std::string path();
};

class Timer : public Handle<uv_timer_t> {
  std::function<void ()> func_;

  static void callback(uv_timer_t *handle);

public:
  Timer(Loop &loop) {
    uv_timer_init(&loop.value(), &handle);
  }
  template<typename Rep1, typename Period1,
           typename Rep2 = Clock::duration::rep, typename Period2 = Clock::duration::period>
  Timer(Loop &loop, std::function<void()> func, std::chrono::duration<Rep1, Period1> timeout,
        std::chrono::duration<Rep2, Period2> repeat = std::chrono::duration<Rep2, Period2>(0))
      : Timer(loop) {
    start(std::move(func), timeout, repeat);
  }

  template <typename Rep1, typename Period1,
            typename Rep2 = Clock::duration::rep, typename Period2 = Clock::duration::period>
  void start(std::function<void()> func, std::chrono::duration<Rep1, Period1> timeout,
             std::chrono::duration<Rep2, Period2> repeat = std::chrono::duration<Rep2, Period2>(0)) {
    assert(!empty);
    func_ = std::move(func);
    uv_timer_start(&handle, &callback,
                   std::chrono::duration_cast<Clock::duration>(timeout).count(),
                   std::chrono::duration_cast<Clock::duration>(repeat).count());
  }

  void stop() { assert(!empty); uv_timer_stop(&handle); }
  void again() { assert(!empty); uv_timer_again(&handle); }

  auto getRepeat() const { return Clock::duration{uv_timer_get_repeat(&handle)}; }

  template<typename Rep, typename Period>
  void setRepeat(std::chrono::duration<Rep, Period> repeat) {
    uv_timer_set_repeat(&handle, std::chrono::duration_cast<Clock::duration>(repeat).count());
  }
};

class Poll : public Handle<uv_poll_t> {
public:
  typedef void Callback(int status, int events);

private:
  std::function<Callback> func_;

  static void callback_(uv_poll_t *handle, int status, int events);

public:
  Poll(Loop &loop, int fd) {
    uv_poll_init(loop, &handle, fd);
  }

  void start(int events, std::function<Callback> func) {
    func_ = std::move(func);
    uv_poll_start(&handle, events, callback_);
  }

  void stop() { uv_poll_stop(&handle); }
};

typedef void AllocCallback(size_t minimumSize, uv_buf_t *out);

class UDP : public Handle<uv_udp_t> {
  friend class UDPSend;
public:
  typedef void Callback(ssize_t result, const uv_buf_t *buf, const struct sockaddr *cAddr, unsigned flags);

private:
  std::function<AllocCallback> allocFunc_;
  std::function<Callback> recvFunc_;

  static void allocCB_(uv_handle_t *handle, size_t suggestedSize, uv_buf_t *buf);
  static void recvCB_(uv_udp_t *handle, ssize_t result, const uv_buf_t *buf, const struct sockaddr *cAddr, unsigned flags);

public:
  UDP(Loop &loop) {
    uv_udp_init(loop, &handle);
  }

  int bind(const struct sockaddr *address, unsigned flags = 0) {
    assert(!empty);
    return uv_udp_bind(&handle, address, flags);
  }

  int recvStart(std::function<AllocCallback> allocCallback, std::function<Callback> recvCallback) {
    assert(!empty);
    allocFunc_ = std::move(allocCallback);
    recvFunc_ = std::move(recvCallback);
    return uv_udp_recv_start(&handle, allocCB_, recvCB_);
  }
  int recvStop() { return uv_udp_recv_stop(&handle); }

  int set_broadcast(bool state) { return uv_udp_set_broadcast(&handle, state); }
};

template<typename T>
class Request {
protected:
  T handle;  // Must be the first element

public:
  Request() {}

  int cancel() { return uv_cancel(reinterpret_cast<uv_req_t*>(&handle)); }
};

class UDPSend : public Request<uv_udp_send_t> {
  static void finish_(uv_udp_send_t *req, int status);

  std::function<void(int)> finishCB_;

public:
  void send(UDP &udp, const uv_buf_t bufs[], unsigned nbufs, const struct sockaddr *addr,
            std::function<void(int)> finishCB) {
    finishCB_ = std::move(finishCB);
    uv_udp_send(&handle, &udp.handle, bufs, nbufs, addr, finish_);
  }
};
}
}

#endif

// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef sw_x0_ServerSocket_h
#define sw_x0_ServerSocket_h

#include <base/Api.h>
#include <base/Defines.h>
#include <base/Logging.h>
#include <string>
#include <vector>
#include <ev++.h>
#include <memory>
#include <sys/socket.h>  // for isLocal() and isTcp()

namespace base {

class Socket;
class SocketSpec;
class SocketDriver;
class IPAddress;

/**
 * Represents a TCP listening socket.
 *
 * @example examples/tcp-echo-server.cpp
 * @example examples/tcp-echo-server-splice.cpp
 *
 * @see Socket
 * @see SocketDriver
 */
class BASE_API ServerSocket
#ifndef NDEBUG
    : public Logging
#endif
      {
 private:
  struct ev_loop* loop_;
  int flags_;
  int typeMask_;
  int backlog_;
  int addressFamily_;
  int fd_;
  bool reusePort_;
  bool deferAccept_;
  size_t multiAcceptCount_;
  ev::io io_;
  SocketDriver* socketDriver_;
  std::string errorText_;

  void (*callback_)(std::unique_ptr<Socket>&&, ServerSocket*);
  void* callbackData_;

  std::string address_;
  int port_;

 public:
  explicit ServerSocket(struct ev_loop* loop);
  ~ServerSocket();

  ServerSocket* clone(struct ev_loop* loop);

  void setBacklog(int value);
  int backlog() const { return backlog_; }

  void setReusePort(bool enabled);
  bool reusePort() const { return reusePort_; }

  bool deferAccept() const;
  bool setDeferAccept(bool enabled);

  bool open(const std::string& ipAddress, int port, int flags);
  bool open(const std::string& localAddress, int flags);
  bool open(const SocketSpec& spec, int flags);
  int handle() const { return fd_; }
  bool isOpen() const { return fd_ >= 0; }
  bool isStarted() const { return io_.is_active(); }
  void start();
  void stop();
  void close();

  int addressFamily() const { return addressFamily_; }
  bool isLocal() const { return addressFamily_ == AF_UNIX; }
  bool isTcp() const {
    return addressFamily_ == AF_INET || addressFamily_ == AF_INET6;
  }

  bool isCloseOnExec() const;
  bool setCloseOnExec(bool enable);

  bool isNonBlocking() const;
  bool setNonBlocking(bool enable);

  size_t multiAcceptCount() const { return multiAcceptCount_; }
  void setMultiAcceptCount(size_t value);

  void setSocketDriver(SocketDriver* sd);
  SocketDriver* socketDriver() { return socketDriver_; }
  const SocketDriver* socketDriver() const { return socketDriver_; }

  template <typename K, void (K::*cb)(std::unique_ptr<Socket>&&, ServerSocket*)>
  void set(K* object);

  const std::string& errorText() const { return errorText_; }

  const std::string& address() const { return address_; }
  int port() const { return port_; }

  std::string serialize() const;
  static std::vector<int> getInheritedSocketList();

 private:
  template <typename K, void (K::*cb)(std::unique_ptr<Socket>&&, ServerSocket*)>
  static void callback_thunk(std::unique_ptr<Socket>&& cs, ServerSocket* ss);

  void accept(ev::io&, int);

  inline bool acceptOne();
};

// {{{
template <typename K, void (K::*cb)(std::unique_ptr<Socket>&&, ServerSocket*)>
void ServerSocket::set(K* object) {
  callback_ = &callback_thunk<K, cb>;
  callbackData_ = object;
}

template <typename K, void (K::*cb)(std::unique_ptr<Socket>&&, ServerSocket*)>
void ServerSocket::callback_thunk(std::unique_ptr<Socket>&& cs,
                                  ServerSocket* ss) {
  (static_cast<K*>(ss->callbackData_)->*cb)(std::move(cs), ss);
}
// }}}

}  // namespace base

#endif

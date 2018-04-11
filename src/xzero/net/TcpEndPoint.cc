// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/net/TcpEndPoint.h>
#include <xzero/net/TcpConnector.h>
#include <xzero/net/TcpUtil.h>
#include <xzero/net/TcpConnection.h>
#include <xzero/util/BinaryReader.h>
#include <xzero/io/FileUtil.h>
#include <xzero/executor/Executor.h>
#include <xzero/logging.h>
#include <xzero/RuntimeError.h>
#include <xzero/Buffer.h>
#include <xzero/sysconfig.h>
#include <xzero/defines.h>

#include <stdexcept>
#include <fcntl.h>
#include <errno.h>

#if defined(XZERO_OS_UNIX)
#include <netinet/tcp.h>
#include <unistd.h>
#endif

#if defined(XZERO_OS_WINDOWS)
#include <winsock2.h>
#endif

namespace xzero {

TcpEndPoint::TcpEndPoint(FileDescriptor&& socket,
                         int addressFamily,
                         Duration readTimeout,
                         Duration writeTimeout,
                         Executor* executor,
                         std::function<void(TcpEndPoint*)> onEndPointClosed)
    : io_(),
      executor_(executor),
      readTimeout_(readTimeout),
      writeTimeout_(writeTimeout),
      inputBuffer_(),
      inputOffset_(0),
      handle_(std::move(socket)),
      addressFamily_(addressFamily),
      isCorking_(false),
      onEndPointClosed_(onEndPointClosed),
      connection_() {
}

void TcpEndPoint::onTimeout() {
  if (connection()) {
    if (connection()->onReadTimeout()) {
      close();
    }
  }
}

TcpEndPoint::~TcpEndPoint() {
  if (isOpen()) {
    close();
  }
}

std::optional<InetAddress> TcpEndPoint::remoteAddress() const {
  Result<InetAddress> addr = TcpUtil::getRemoteAddress(handle_, addressFamily());
  if (addr.isSuccess())
    return *addr;
  else {
    logError("TcpEndPoint: remoteAddress: ({}) {}",
        addr.error().category().name(),
        addr.error().message().c_str());
    return std::nullopt;
  }
}

std::optional<InetAddress> TcpEndPoint::localAddress() const {
  Result<InetAddress> addr = TcpUtil::getLocalAddress(handle_, addressFamily());
  if (addr.isSuccess())
    return *addr;
  else {
    logError("TcpEndPoint: localAddress: ({}) {}",
        addr.error().category().name(),
        addr.error().message().c_str());
    return std::nullopt;
  }
}

bool TcpEndPoint::isOpen() const XZERO_NOEXCEPT {
  return handle_ >= 0;
}

void TcpEndPoint::close() {
  if (isOpen()) {
    if (onEndPointClosed_) {
      onEndPointClosed_(this);
    }

    handle_.close();
  }
}

void TcpEndPoint::setConnection(std::unique_ptr<TcpConnection>&& c) {
  connection_ = std::move(c);
}

bool TcpEndPoint::isBlocking() const {
  return !(fcntl(handle_, F_GETFL) & O_NONBLOCK);
}

void TcpEndPoint::setBlocking(bool enable) {
  FileUtil::setBlocking(handle_, enable);
}

bool TcpEndPoint::isCorking() const {
  return isCorking_;
}

void TcpEndPoint::setCorking(bool enable) {
  if (isCorking_ != enable) {
    TcpUtil::setCorking(handle_, enable);
    isCorking_ = enable;
  }
}

bool TcpEndPoint::isTcpNoDelay() const {
  return TcpUtil::isTcpNoDelay(handle_);
}

void TcpEndPoint::setTcpNoDelay(bool enable) {
  TcpUtil::setTcpNoDelay(handle_, enable);
}

std::string TcpEndPoint::toString() const {
  char buf[32];
  snprintf(buf, sizeof(buf), "TcpEndPoint(%d)@%p", handle(), this);
  return buf;
}

void TcpEndPoint::startDetectProtocol(bool dataReady,
                                      ProtocolCallback createConnection) {
  inputBuffer_.reserve(256);

  if (dataReady) {
    onDetectProtocol(createConnection);
  } else {
    executor_->executeOnReadable(
        handle(),
        std::bind(&TcpEndPoint::onDetectProtocol, this, createConnection));
  }
}

void TcpEndPoint::onDetectProtocol(ProtocolCallback createConnection) {
  size_t n = read(&inputBuffer_);

  if (n == 0) {
    close();
    return;
  }

  // XXX detect magic byte (0x01) to protocol detection
  if (inputBuffer_[0] == TcpConnector::MagicProtocolSwitchByte) {
    BinaryReader reader(inputBuffer_);
    reader.parseVarUInt(); // skip magic
    std::string protocol = reader.parseString();
    inputOffset_ = inputBuffer_.size() - reader.pending();
    createConnection(protocol, this);
  } else {
    // create TcpConnection object for given endpoint
    createConnection("", this);
  }

  if (connection_) {
    connection_->onOpen(true);
  } else {
    close();
  }
}

size_t TcpEndPoint::readahead(size_t maxBytes) {
  const size_t nprefilled = readBufferSize();
  if (nprefilled > 0)
    return nprefilled;

  inputBuffer_.reserve(maxBytes);
  return read(&inputBuffer_);
}

size_t TcpEndPoint::read(Buffer* sink) {
  int space = sink->capacity() - sink->size();
  if (space < 4 * 1024) {
    sink->reserve(sink->capacity() + 8 * 1024);
    space = sink->capacity() - sink->size();
  }

  return read(sink, space);
}

size_t TcpEndPoint::read(Buffer* result, size_t count) {
  assert(count <= result->capacity() - result->size());

  if (inputOffset_ < inputBuffer_.size()) {
    count = std::min(count, inputBuffer_.size() - inputOffset_);
    result->push_back(inputBuffer_.ref(inputOffset_, count));
    inputOffset_ += count;
    if (inputOffset_ == inputBuffer_.size()) {
      inputBuffer_.clear();
      inputOffset_ = 0;
    }
    return count;
  }

  ssize_t n = ::read(handle(), result->end(), count);
  if (n < 0) {
    // don't raise on soft errors, such as there is simply no more data to read.
    switch (errno) {
      case EBUSY:
      case EAGAIN:
#if defined(EWOULDBLOCK) && (EWOULDBLOCK != EAGAIN)
      case EWOULDBLOCK:
#endif
        break;
      default:
        RAISE_ERRNO(errno);
    }
  } else {
    result->resize(result->size() + n);
  }

  return n;
}

size_t TcpEndPoint::write(const BufferRef& source) {
  ssize_t rv = ::write(handle(), source.data(), source.size());
  if (rv < 0)
    RAISE_ERRNO(errno);

  // EOF exception?

  return rv;
}

size_t TcpEndPoint::write(const FileView& view) {
  return TcpUtil::sendfile(handle(), view);
}

void TcpEndPoint::wantRead() {
  // TODO: abstract away the logic of TCP_DEFER_ACCEPT

  if (!io_) {
    io_ = executor_->executeOnReadable(
        handle(),
        std::bind(&TcpEndPoint::fillable, this),
        readTimeout(),
        std::bind(&TcpEndPoint::onTimeout, this));
  }
}

void TcpEndPoint::fillable() {
  std::shared_ptr<TcpEndPoint> _guard = shared_from_this();

  try {
    io_.reset();
    connection()->onReadable();
  } catch (const std::exception& e) {
    connection()->onInterestFailure(e);
  }
}

void TcpEndPoint::wantWrite() {
  if (!io_) {
    io_ = executor_->executeOnWritable(
        handle(),
        std::bind(&TcpEndPoint::flushable, this),
        writeTimeout(),
        std::bind(&TcpEndPoint::onTimeout, this));
  }
}

void TcpEndPoint::flushable() {
  std::shared_ptr<TcpEndPoint> _guard = shared_from_this();

  try {
    io_.reset();
    connection()->onWriteable();
  } catch (const std::exception& e) {
    connection()->onInterestFailure(e);
  }
}

Duration TcpEndPoint::readTimeout() const noexcept {
  return readTimeout_;
}

Duration TcpEndPoint::writeTimeout() const noexcept {
  return writeTimeout_;
}

void onConnectComplete(InetAddress address,
                       int fd,
                       Duration readTimeout,
                       Duration writeTimeout,
                       Executor* executor,
                       Promise<std::shared_ptr<TcpEndPoint>> promise) {
  int val = 0;
  socklen_t vlen = sizeof(val);
  if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &val, &vlen) == 0) {
    if (val == 0) {
      promise.success(std::make_shared<TcpEndPoint>(FileDescriptor{fd},
                                                    address.family(),
                                                    readTimeout,
                                                    writeTimeout,
                                                    executor,
                                                    nullptr));
    } else {
      logDebug("Connecting to {} failed. {}", address, strerror(val));
      promise.failure(std::make_error_code(static_cast<std::errc>(val)));
    }
  } else {
    logDebug("Connecting to {} failed. {}", address, strerror(val));
    promise.failure(std::make_error_code(static_cast<std::errc>(val)));
  }
}

Future<std::shared_ptr<TcpEndPoint>> TcpEndPoint::connect(const InetAddress& address,
                                                          Duration connectTimeout,
                                                          Duration readTimeout,
                                                          Duration writeTimeout,
                                                          Executor* executor) {
  int flags = 0;
#if defined(SOCK_CLOEXEC)
  flags |= SOCK_CLOEXEC;
#endif
#if defined(SOCK_NONBLOCK)
  flags |= SOCK_NONBLOCK;
#endif

  Promise<std::shared_ptr<TcpEndPoint>> promise;

  int fd = socket(address.family(), SOCK_STREAM | flags, IPPROTO_TCP);
  if (fd < 0) {
    promise.failure(std::make_error_code(static_cast<std::errc>(errno)));
    return promise.future();
  }

#if !defined(SOCK_NONBLOCK)
  FileUtil::setBlocking(fd, false);
#endif

  std::error_code ec = TcpUtil::connect(fd, address);
  if (!ec) {
    promise.success(std::make_shared<TcpEndPoint>(FileDescriptor{fd},
                                                  address.family(),
                                                  readTimeout,
                                                  writeTimeout,
                                                  executor,
                                                  nullptr));
  } else if (ec == std::errc::operation_in_progress) {
    executor->executeOnWritable(fd,
        std::bind(&onConnectComplete, address, fd, readTimeout, writeTimeout, executor, promise),
        connectTimeout,
        [fd, promise]() { FileUtil::close(fd); promise.failure(std::errc::timed_out); });
  } else {
    promise.failure(std::make_error_code(static_cast<std::errc>(errno)));
  }
  return promise.future();
}

} // namespace xzero

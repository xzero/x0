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

TcpEndPoint::TcpEndPoint(Duration readTimeout,
                         Duration writeTimeout,
                         Executor* executor,
                         std::function<void(TcpEndPoint*)> onEndPointClosed)
    : io_(),
      executor_(executor),
      readTimeout_(readTimeout),
      writeTimeout_(writeTimeout),
      inputBuffer_(),
      inputOffset_(0),
      socket_{Socket::NonBlockingTCP},
      isCorking_(false),
      onEndPointClosed_(onEndPointClosed),
      connection_() {
}

TcpEndPoint::TcpEndPoint(Socket&& socket,
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
      socket_(std::move(socket)),
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
  Result<InetAddress> addr = socket_.getRemoteAddress();
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
  Result<InetAddress> addr = socket_.getLocalAddress();
  if (addr.isSuccess())
    return *addr;
  else {
    logError("TcpEndPoint: localAddress: ({}) {}",
        addr.error().category().name(),
        addr.error().message().c_str());
    return std::nullopt;
  }
}

bool TcpEndPoint::isOpen() const noexcept {
  return socket_.valid();
}

void TcpEndPoint::close() {
  if (isOpen()) {
    if (onEndPointClosed_) {
      onEndPointClosed_(this);
    }

    socket_.close();
  }
}

void TcpEndPoint::setConnection(std::unique_ptr<TcpConnection>&& c) {
  connection_ = std::move(c);
}

bool TcpEndPoint::isBlocking() const {
  return !(fcntl(socket_, F_GETFL) & O_NONBLOCK);
}

void TcpEndPoint::setBlocking(bool enable) {
  unsigned flags = enable ? fcntl(fd, F_GETFL) & ~O_NONBLOCK
                          : fcntl(fd, F_GETFL) | O_NONBLOCK;

  if (fcntl(fd, F_SETFL, flags) < 0) {
    RAISE_ERRNO(errno);
  }
}

bool TcpEndPoint::isCorking() const {
  return isCorking_;
}

void TcpEndPoint::setCorking(bool enable) {
  if (isCorking_ != enable) {
    TcpUtil::setCorking(socket_, enable);
    isCorking_ = enable;
  }
}

bool TcpEndPoint::isTcpNoDelay() const {
  return TcpUtil::isTcpNoDelay(socket_);
}

void TcpEndPoint::setTcpNoDelay(bool enable) {
  TcpUtil::setTcpNoDelay(socket_, enable);
}

std::string TcpEndPoint::toString() const {
  return fmt::format("TcpEndPoint({})", socket_.getRemoteAddress());
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

void TcpEndPoint::connect(const InetAddress& address,
                          Duration connectTimeout,
                          std::function<void()> onConnected,
                          std::function<void(std::error_code ec)> onFailure) {
  std::error_code ec = TcpUtil::connect(socket_, address);
  if (!ec) {
    onConnected();
  } else if (ec == std::errc::operation_in_progress) {
    executor_->executeOnWritable(socket_,
        [=]() { onConnectComplete(address, onConnected, onFailure); },
        connectTimeout,
        [=]() { onFailure(std::make_error_code(std::errc::timed_out)); });
  } else {
    onFailure(ec);
  }
}

void TcpEndPoint::onConnectComplete(InetAddress address,
                                    std::function<void()> onConnected,
                                    std::function<void(std::error_code ec)> onFailure) {
  int val = 0;
  socklen_t vlen = sizeof(val);
  if (getsockopt(socket_, SOL_SOCKET, SO_ERROR, &val, &vlen) == 0) {
    if (val == 0) {
      onConnected();
    } else {
      const std::error_code ec = std::make_error_code(static_cast<std::errc>(val));
      logDebug("Connecting to {} failed. {}", address, ec.message());
      onFailure(ec);
    }
  } else {
    const std::error_code ec = std::make_error_code(static_cast<std::errc>(val));
    logDebug("Connecting to {} failed. {}", address, ec.message());
    onFailure(ec);
  }
}

} // namespace xzero

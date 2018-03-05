// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <atomic>
#include <xzero/StringUtil.h>
#include <xzero/io/FileDescriptor.h>

namespace xzero {

enum class LogLevel { // {{{
  None = 9999,
  Fatal = 7000,
  Error = 6000,
  Warning = 5000,
  Notice = 4000,
  Info = 3000,
  Debug = 2000,
  Trace = 1000,
};

LogLevel make_loglevel(const std::string& value);

std::ostream& operator<<(std::ostream& os, LogLevel value);
std::string as_string(LogLevel value);
// }}}
class LogTarget { // {{{
 public:
  virtual ~LogTarget() {}

  virtual void log(
      LogLevel level,
      const std::string& component,
      const std::string& message) = 0;
};
// }}}
class FileLogTarget : public LogTarget { // {{{
 public:
  explicit FileLogTarget(FileDescriptor&& fd);

  void log(LogLevel level,
           const std::string& component,
           const std::string& message) override;

  void setTimestampEnabled(bool value) { timestampEnabled_ = value; }
  bool isTimestampEnabled() const noexcept { return timestampEnabled_; }

 private:
  std::string createTimestamp() const;

 private:
  FileDescriptor fd_;
  bool timestampEnabled_;
}; // }}}
class ConsoleLogTarget : public LogTarget { // {{{
 public:
  ConsoleLogTarget();

  void log(LogLevel level,
           const std::string& component,
           const std::string& message) override;

  void setTimestampEnabled(bool value) { timestampEnabled_ = value; }
  bool isTimestampEnabled() const noexcept { return timestampEnabled_; }

  static ConsoleLogTarget* get();

 private:
  std::string createTimestamp() const;

 private:
  bool timestampEnabled_;
}; // }}}
class SyslogTarget : public LogTarget { // {{{
 public:
  explicit SyslogTarget(const std::string& ident);
  ~SyslogTarget();

  void log(LogLevel level,
           const std::string& component,
           const std::string& message) override;

  static SyslogTarget* get();
};
// }}}
class Logger { // {{{
 public:
  Logger();
  static Logger* get();

  void log(
      LogLevel log_level,
      const std::string& component,
      const std::string& message);

  void logException(
      LogLevel log_level,
      const std::string& component,
      const std::exception& exception,
      const std::string& message);

  void addTarget(LogTarget* target);
  void setMinimumLogLevel(LogLevel min_level);
  LogLevel getMinimumLogLevel() { return min_level_.load(); }

 protected:
  std::atomic<LogLevel> min_level_;
  std::atomic<size_t> max_listener_index_;
  std::atomic<LogTarget*> listeners_[64];
};
// }}}
// {{{ free functions
/**
 * CRITICAL: Action should be taken as soon as possible
 */
template <typename... T>
void logFatal(const std::string& component, const std::string& msg, T... args) {
  Logger::get()->log(LogLevel::Fatal, component, StringUtil::format(msg, args...));
}

template <typename... T>
void logFatal(
    const std::string& component,
    const std::exception& e,
    const std::string& msg,
    T... args) {
  Logger::get()->logException(LogLevel::Fatal, component, e, StringUtil::format(msg, args...));
}

/**
 * ERROR: User-visible Runtime Errors
 */
template <typename... T>
void logError(const std::string& component, const std::string& msg, T... args) {
  Logger::get()->log(LogLevel::Error, component, StringUtil::format(msg, args...));
}

template <typename... T>
void logError(
    const std::string& component,
    const std::exception& e,
    const std::string& msg,
    T... args) {
  Logger::get()->logException(LogLevel::Error, component, e, StringUtil::format(msg, args...));
}

/**
 * WARNING: Something unexpected happened that should not have happened
 */
template <typename... T>
void logWarning(const std::string& component, const std::string& msg, T... args) {
  Logger::get()->log(LogLevel::Warning, component, StringUtil::format(msg, args...));
}

template <typename... T>
void logWarning(
    const std::string& component,
    const std::exception& e,
    const std::string& msg,
    T... args) {
  Logger::get()->logException(LogLevel::Warning, component, e, StringUtil::format(msg, args...));
}

/**
 * NOTICE: Normal but significant condition.
 */
template <typename... T>
void logNotice(const std::string& component, const std::string& msg, T... args) {
  Logger::get()->log(LogLevel::Notice, component, StringUtil::format(msg, args...));
}

template <typename... T>
void logNotice(
    const std::string& component,
    const std::exception& e,
    const std::string& msg,
    T... args) {
  Logger::get()->logException(LogLevel::Notice, component, e, StringUtil::format(msg, args...));
}

/**
 * INFO: Informational messages
 */
template <typename... T>
void logInfo(const std::string& component, const std::string& msg, T... args) {
  Logger::get()->log(LogLevel::Info, component, StringUtil::format(msg, args...));
}

template <typename... T>
void logInfo(
    const std::string& component,
    const std::exception& e,
    const std::string& msg,
    T... args) {
  Logger::get()->logException(LogLevel::Info, component, e, StringUtil::format(msg, args...));
}

/**
 * DEBUG: Debug messages
 */
template <typename... T>
void logDebug(const std::string& component, const std::string& msg, T... args) {
  Logger::get()->log(LogLevel::Debug, component, StringUtil::format(msg, args...));
}

template <typename... T>
void logDebug(
    const std::string& component,
    const std::exception& e,
    const std::string& msg,
    T... args) {
  Logger::get()->logException(LogLevel::Debug, component, e, StringUtil::format(msg, args...));
}

/**
 * TRACE: Trace messages
 */
template <typename... T>
void logTrace(const std::string& component, const std::string& msg, T... args) {
  Logger::get()->log(LogLevel::Trace, component, StringUtil::format(msg, args...));
}

template <typename... T>
void logTrace(
    const std::string& component,
    const std::exception& e,
    const std::string& msg,
    T... args) {
  Logger::get()->logException(LogLevel::Trace, component, e, StringUtil::format(msg, args...));
}
// }}}

} // namespace xzero

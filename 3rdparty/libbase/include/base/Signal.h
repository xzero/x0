// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef x0_signal_hpp
#define x0_signal_hpp

#include <base/Types.h>
#include <base/Api.h>
#include <list>
#include <algorithm>
#include <functional>

namespace base {

//! \addtogroup base
//@{

/**
 * multi channel signal API.
 *
 * ...
 */
template <typename SignatureT>
class Signal;

/**
 * @brief Signal API
 *
 * This API is based on the idea of Qt's signal/slot API.
 * You can connect zero or more callbacks to this signal that get invoked
 * sequentially when this signal is fired.
 *
 */
template <typename... Args>
class Signal<void(Args...)> {
  Signal(const Signal&) = delete;
  Signal& operator=(const Signal&) = delete;

 private:
  typedef std::list<std::function<void(Args...)>> list_type;

 public:
  typedef typename list_type::iterator Connection;

 public:
  Signal() = default;
  Signal(Signal&&) = default;
  Signal& operator=(Signal&&) = default;

  /**
   * Tests whether this signal contains any listeners.
   */
  bool empty() const { return listeners_.empty(); }

  /**
   * Retrieves the number of listeners to this signal.
   */
  std::size_t size() const { return listeners_.size(); }

  /**
   * Connects a listener with this signal.
   */
  template <class K, void (K::*method)(Args...)>
  Connection connect(K* object) {
    return connect([=](Args... args) {
      (static_cast<K*>(object)->*method)(args...);
    });
  }

  /**
   * @brief Connects a listener with this signal.
   *
   * @return a handle to later explicitely disconnect from this signal again.
   */
  Connection connect(std::function<void(Args...)>&& cb) {
    listeners_.push_back(std::move(cb));
    auto handle = listeners_.end();
    --handle;
    return handle;
  }

  /**
   * @brief Disconnects a listener from this signal.
   */
  void disconnect(Connection c) { listeners_.erase(c); }

  /**
   * @brief invokes all listeners with the given args
   *
   * Triggers this signal by notifying all listeners via their registered
   * callback each with the given arguments passed.
   */
  void fire(const Args&... args) const {
    for (auto listener : listeners_) {
      listener(args...);
    }
  }

  /**
   * @brief invokes all listeners with the given args
   *
   * Triggers this signal by notifying all listeners via their registered
   * callback each with the given arguments passed.
   */
  void operator()(Args... args) const {
    fire(std::forward<Args>(args)...);
  }

  /**
   * Clears all listeners to this signal.
   */
  void clear() { listeners_.clear(); }

 private:
  list_type listeners_;
};

//@}

}  // namespace base

#endif

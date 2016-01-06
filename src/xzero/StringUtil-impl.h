// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <string>

namespace xzero {

template <typename H, typename... T>
void StringUtil::toStringVImpl(
    std::vector<std::string>* target,
    H value,
    T... values) {
  target->emplace_back(toString(value));
  toStringVImpl(target, values...);
}

template <typename... T>
std::vector<std::string> StringUtil::toStringV(T... values) {
  std::vector<std::string> target;
  toStringVImpl(&target, values...);
  return target;
}

template <typename ValueType, typename... T>
void StringUtil::formatImpl(
    std::string* scratch,
    int argn,
    ValueType value,
    T... values) {
  StringUtil::replaceAll(
      scratch,
      "$" + std::to_string(argn),
      StringUtil::toString(value));

  formatImpl(scratch, argn + 1, values...);
}

template <typename ValueType>
void StringUtil::formatImpl(
    std::string* scratch,
    int argn,
    ValueType value) {
  StringUtil::replaceAll(
      scratch,
      "$" + std::to_string(argn),
      StringUtil::toString(value));
}

template <typename... T>
std::string StringUtil::format(const char* fmt, T... values) {
  std::string str = fmt;
  StringUtil::formatImpl(&str, 0, values...);
  return str;
}

template <typename... T>
std::string StringUtil::format(const std::string& fmt, T... values) {
  std::string str = fmt;
  StringUtil::formatImpl(&str, 0, values...);
  return str;
}

template <typename T>
std::string StringUtil::formatNumberMetric(T orig_value) {
  char buf[128];
  size_t len = 0;
  double value = orig_value; // FIXPAUL
  auto abs_value = fabs(value);

  if (abs_value == 0){
    len = snprintf(buf, sizeof(buf), "0");
  }

  else if (abs_value < 0.000000001){
    len = snprintf(buf, sizeof(buf), "%.1fp", value * 1000000000000);
  }

  else if (abs_value < 0.000001){
    len = snprintf(buf, sizeof(buf), "%.1fn", value * 1000000000);
  }

  else if (abs_value < 0.001){
    len = snprintf(buf, sizeof(buf), "%.1fμ", value * 1000000);
  }

  else if (abs_value < 0.1){
    len = snprintf(buf, sizeof(buf), "%.1fm", value * 1000);
  }

  else if (abs_value < 10){
    len = snprintf(buf, sizeof(buf), "%.2f", value);
  }

  else if (abs_value < 1000) {
    len = snprintf(buf, sizeof(buf), "%.1f", value);
  }

  else if (abs_value < 1000000) {
    len = snprintf(buf, sizeof(buf), "%.1fK", value / 1000);
  }

  else if (abs_value < 1000000000) {
    len = snprintf(buf, sizeof(buf), "%.1fM", value / 1000000);
  }

  else if (abs_value < 1000000000000) {
    len = snprintf(buf, sizeof(buf), "%.1fG", value / 1000000000);
  }

  else {
    len = snprintf(buf, sizeof(buf), "%.1fT", value / 1000000000000);
  }

  return std::string(buf, len);
}

} // namespace xzero

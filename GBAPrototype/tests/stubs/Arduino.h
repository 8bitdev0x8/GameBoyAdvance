#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <chrono>
extern "C" inline void* pmalloc(size_t n) { return malloc(n); }
inline uint32_t micros() {
  using namespace std::chrono;
  return uint32_t(duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count());
}

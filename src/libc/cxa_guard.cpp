#include <stdint.h>

extern "C" int __cxa_guard_acquire(uint64_t *guard) {
  auto *flag = reinterpret_cast<uint8_t *>(guard);

  if (*flag != 0)
    return 0;

  return 1;
}

extern "C" void __cxa_guard_release(uint64_t *guard) {
  auto *flag = reinterpret_cast<uint8_t *>(guard);
  *flag = 1;
}

extern "C" void __cxa_guard_abort(uint64_t *) {
  // nothing
}

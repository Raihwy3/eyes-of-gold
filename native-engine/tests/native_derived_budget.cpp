/* Bound live C++ allocations while running the actual first-launch converter.
 * A 16 MiB ceiling leaves room in the 24 MiB heap for SDK stacks, stdio and
 * the original C inflater. The previous converter exceeded even 24 MiB. */
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <new>
namespace {
union Header { std::max_align_t alignment; size_t bytes; };
size_t live, peak;
void *allocate(size_t bytes) {
  if (!bytes) bytes = 1;
  if (bytes > 16 * 1024 * 1024 || live > 16 * 1024 * 1024 - bytes) {
    std::fprintf(stderr, "Derived conversion exceeded 16 MiB C++ budget: live=%zu request=%zu\n", live, bytes);
    std::exit(77);
  }
  auto *p = static_cast<Header *>(std::malloc(sizeof(Header) + bytes));
  if (!p) std::abort();
  p->bytes = bytes;
  live += bytes;
  if (live > peak) peak = live;
  return p + 1;
}
void release(void *pointer) {
  if (!pointer) return;
  auto *p = static_cast<Header *>(pointer) - 1;
  live -= p->bytes;
  std::free(p);
}
struct Report {
  ~Report() {
    std::fprintf(stderr, "Derived C++ allocation peak: %zu bytes; remaining: %zu\n", peak, live);
    if (live) std::abort();
  }
} report;
}
void *operator new(size_t bytes) { return allocate(bytes); }
void *operator new[](size_t bytes) { return allocate(bytes); }
void *operator new(size_t bytes, const std::nothrow_t &) noexcept { return allocate(bytes); }
void *operator new[](size_t bytes, const std::nothrow_t &) noexcept { return allocate(bytes); }
void operator delete(void *p) noexcept { release(p); }
void operator delete[](void *p) noexcept { release(p); }
void operator delete(void *p, size_t) noexcept { release(p); }
void operator delete[](void *p, size_t) noexcept { release(p); }
void operator delete(void *p, const std::nothrow_t &) noexcept { release(p); }
void operator delete[](void *p, const std::nothrow_t &) noexcept { release(p); }

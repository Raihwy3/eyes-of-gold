/* Fault injection for the real cache serializer; host checks only. */
#include <cerrno>
#include <cstdio>
#include <cstdlib>
extern "C" size_t __real_fwrite(const void *, size_t, size_t, FILE *);
extern "C" int __real_fseek(FILE *, long, int);
extern "C" int __real_fclose(FILE *);
static FILE *cache;
static size_t written;
extern "C" size_t __wrap_fwrite(const void *data, size_t size, size_t count, FILE *file) {
  if (!cache) cache = file;
  const char *limit = std::getenv("EOG_TEST_CACHE_FAIL_AFTER");
  if (file == cache && limit && size == 1) {
    size_t end = std::strtoul(limit, nullptr, 10);
    size_t allowed = written < end ? end - written : 0;
    if (count > allowed) {
      size_t result = __real_fwrite(data, 1, allowed, file);
      written += result;
      errno = ENOSPC;
      return result;
    }
  }
  size_t result = __real_fwrite(data, size, count, file);
  if (file == cache) written += result * size;
  return result;
}
extern "C" int __wrap_fseek(FILE *file, long offset, int origin) {
  if (file == cache && std::getenv("EOG_TEST_CACHE_FAIL_SEEK")) {
    errno = EIO;
    return -1;
  }
  return __real_fseek(file, offset, origin);
}
extern "C" int __wrap_fclose(FILE *file) {
  bool fail = file == cache && std::getenv("EOG_TEST_CACHE_FAIL_CLOSE");
  int result = __real_fclose(file);
  if (fail) { errno = EIO; return EOF; }
  return result;
}

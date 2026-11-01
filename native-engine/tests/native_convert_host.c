#include "native_convert.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef CONVERT_EXTERNAL_INFLATE
#ifndef CONVERT_ORIGINAL_INFLATE
#include <zlib.h>
/* Host substitute for the original N64 inflater; the converter itself is
 * identical on host and ARM. The original ARM entry is checked by full build.
 */
struct huft;
uint32_t decompressdata(void *src, void *dst, struct huft *unused) {
  (void)unused;
  z_stream s = {0};
  s.next_in = (unsigned char *)src + 2;
  s.avail_in = 1048576;
  s.next_out = dst;
  s.avail_out = 1048576;
  if (inflateInit2(&s, -15) != Z_OK)
    abort();
  int result = inflate(&s, Z_FINISH);
  if (result != Z_STREAM_END)
    abort();
  uint32_t n = s.total_out;
  inflateEnd(&s);
  return n;
}
#else
void sysFatalError(const char *format, ...) {
  fprintf(stderr, "Original inflater failure: %s\n", format);
  abort();
}
#endif
#endif
#ifndef CONVERT_NO_MAIN
static int progress(const char *stage, unsigned done, unsigned total) {
  if (getenv("CONVERT_VERBOSE"))
    fprintf(stderr, "%s %u/%u\n", stage, done, total);
  const char *cancel = getenv("CONVERT_CANCEL_STAGE");
  if (cancel && !strcmp(stage, cancel) && done > 0)
    return 0;
  return getenv("CONVERT_CANCEL") == NULL;
}
int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  if (!native_convert_ensure(argv[1], progress)) {
    fprintf(stderr, "%s\n", native_convert_error());
    return 1;
  }
  return 0;
}

#endif

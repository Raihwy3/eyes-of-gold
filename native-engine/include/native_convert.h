#ifndef NATIVE_CONVERT_H
#define NATIVE_CONVERT_H
/* Runs before engine RAM, graphics and audio workers. Return 0 on error/cancel.
 * Callback returns zero to cancel. Existing completed caches are preserved. */
typedef int (*NativeConvertProgress)(const char *stage, unsigned done,
                                     unsigned total);
int native_convert_ensure(const char *directory,
                          NativeConvertProgress progress);
const char *native_convert_error(void);
#endif

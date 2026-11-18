#ifndef EOG_NATIVE_CART_H
#define EOG_NATIVE_CART_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#include <stddef.h>
/* File-backed cartridge extents. Mount/reset happen before game threads start;
 * reads are serialized per file and can run concurrently from game threads.
 * Addresses are original cart tokens, never CPU-dereferenceable pointers. */
int native_cart_mount(const char *path,uint32_t cart_start,uint32_t file_offset,uint32_t bytes);
int native_cart_contains(uint32_t address,uint32_t bytes);
int native_cart_read(uint32_t address,void *destination,uint32_t bytes);
/* Preload an immutable hot range before readers start. Failure leaves normal
 * file-backed reads available. Mount/cache/reset are startup/shutdown only. */
int native_cart_cache(uint32_t address,uint32_t bytes);
void native_cart_stats(uint32_t *cached_reads,uint32_t *file_reads);
void native_cart_reset(void);
#ifdef __cplusplus
}
#endif
#endif

#define _POSIX_C_SOURCE 200809L
#include "native_cart.h"
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static unsigned char original[16384];
static void *reader(void *arg) {
    unsigned seed=(unsigned)(uintptr_t)arg;
    for(unsigned i=0;i<1000;++i) {
        unsigned offset=(i*137+seed*13)%8000;
        unsigned char data[64];
        assert(native_cart_read(0x10000000u+offset,data,sizeof data));
        assert(!memcmp(data,original+17+offset,sizeof data));
    }
    return NULL;
}
int main(void) {
    char path[]="/tmp/eog-cart-cache-XXXXXX";
    int fd=mkstemp(path);assert(fd>=0);
    for(unsigned i=0;i<sizeof original;++i) original[i]=(i*91u+i/13u)&255u;
    assert(write(fd,original,sizeof original)==sizeof original);
    assert(native_cart_mount(path,0x10000000u,17,8192));
    assert(!native_cart_cache(0x0fffffffu,64));
    assert(!native_cart_cache(0x10001fffu,64));
    assert(!native_cart_cache(0x10000000u,0));
    assert(native_cart_cache(0x10000064u,4096));
    assert(!native_cart_cache(0x10000064u,4096));
    unsigned char data[64];
    // Exact ends and reads spanning either cache boundary use valid data.
    const unsigned edges[]={99,100,4196-64,4196-63,4196,8192-64};
    for(unsigned i=0;i<sizeof edges/sizeof edges[0];++i) {
        assert(native_cart_read(0x10000000u+edges[i],data,64));
        assert(!memcmp(data,original+17+edges[i],64));
    }
    memset(data,0xa5,sizeof data);
    assert(!native_cart_read(0x10001fffu,data,64) && data[0]==0xa5);
    pthread_t workers[4];
    for(unsigned i=0;i<4;++i) assert(!pthread_create(&workers[i],NULL,reader,(void *)(uintptr_t)i));
    for(unsigned i=0;i<4;++i) assert(!pthread_join(workers[i],NULL));
    uint32_t cached,file;native_cart_stats(&cached,&file);
    assert(cached>1000 && file>1000 && cached+file==4007);
    // Cache remains an immutable snapshot, independent of subsequent file IO.
    memset(data,0xff,sizeof data);assert(pwrite(fd,data,64,117)==64);
    assert(native_cart_read(0x10000064u,data,64));
    assert(!memcmp(data,original+117,64));
    native_cart_reset();native_cart_stats(&cached,&file);assert(!cached && !file);
    assert(!native_cart_contains(0x10000000u,1));
    assert(native_cart_mount(path,0x10000000u,17,8192));
    assert(native_cart_cache(0x10000064u,4096));
    assert(native_cart_read(0x10000064u,data,64));
    for(unsigned i=0;i<64;++i) assert(data[i]==0xff);
    native_cart_reset();close(fd);unlink(path);
    puts("PASS cartridge RAM cache: file offsets, bounds, crossing fallback, 4000 concurrent reads, immutable snapshot and reset/remount");
}

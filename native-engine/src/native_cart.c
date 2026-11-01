/* Cartridge DMA over SD files, without fixed virtual mappings or a ROM copy. */
#include "native_cart.h"
#include <stdio.h>
#include <pthread.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#define CART_BANKS 8
typedef struct {
    FILE *file;
    uint32_t cart_start,file_offset,bytes;
    pthread_mutex_t lock;
    unsigned char *cache;
    uint32_t cache_offset,cache_bytes;
} CartBank;
static CartBank banks[CART_BANKS];
static unsigned count;
static uint32_t cached_reads,file_reads;

static CartBank *find_bank(uint32_t address,uint32_t bytes)
{
    for (unsigned i=0;i<count;++i) {
        CartBank *bank=&banks[i];
        if (address<bank->cart_start) continue;
        uint32_t offset=address-bank->cart_start;
        if (offset<=bank->bytes && bytes<=bank->bytes-offset) return bank;
    }
    return NULL;
}
int native_cart_mount(const char *path,uint32_t cart_start,uint32_t file_offset,uint32_t bytes)
{
    if (!path || !bytes || count==CART_BANKS || cart_start<0x10000000u ||
        bytes>UINT32_MAX-cart_start || (uint64_t)file_offset+bytes>(uint64_t)LONG_MAX) return 0;
    for (unsigned i=0;i<count;++i) {
        uint64_t a=cart_start,b=banks[i].cart_start;
        if (a<b+banks[i].bytes && b<a+bytes) return 0;
    }
    FILE *file=fopen(path,"rb");
    if (!file) return 0;
    int valid=fseek(file,0,SEEK_END)==0;
    long size=valid?ftell(file):-1;
    if (size<0 || (uint64_t)file_offset+bytes>(uint64_t)size) {fclose(file);return 0;}
    CartBank *bank=&banks[count];
    if (pthread_mutex_init(&bank->lock,NULL)) {fclose(file);return 0;}
    bank->file=file;bank->cart_start=cart_start;bank->file_offset=file_offset;bank->bytes=bytes;
    ++count;
    return 1;
}
int native_cart_contains(uint32_t address,uint32_t bytes)
{
    return find_bank(address,bytes)!=NULL;
}
/* Existing libultra adapter uses this port-layer API before DMA. */
int romdataCartAddrValid(uint32_t address,uint32_t bytes)
{
    return native_cart_contains(address,bytes);
}
int native_cart_read(uint32_t address,void *destination,uint32_t bytes)
{
    CartBank *bank=find_bank(address,bytes);
    if (!bank || (!destination && bytes)) return 0;
    if (!bytes) return 1;
    uint32_t offset=address-bank->cart_start;
    if(bank->cache && offset>=bank->cache_offset &&
       offset-bank->cache_offset<=bank->cache_bytes &&
       bytes<=bank->cache_bytes-(offset-bank->cache_offset)) {
        memcpy(destination,bank->cache+offset-bank->cache_offset,bytes);
        __atomic_fetch_add(&cached_reads,1,__ATOMIC_RELAXED);
        return 1;
    }
    __atomic_fetch_add(&file_reads,1,__ATOMIC_RELAXED);
    pthread_mutex_lock(&bank->lock);
    int ok=fseek(bank->file,(long)(bank->file_offset+offset),SEEK_SET)==0 &&
           fread(destination,1,bytes,bank->file)==bytes;
    pthread_mutex_unlock(&bank->lock);
    return ok;
}
int native_cart_cache(uint32_t address,uint32_t bytes)
{
    CartBank *bank=find_bank(address,bytes);
    if(!bank || !bytes || bank->cache) return 0;
    unsigned char *memory=malloc(bytes);
    if(!memory) return 0;
    if(!native_cart_read(address,memory,bytes)) {free(memory);return 0;}
    bank->cache_offset=address-bank->cart_start;
    bank->cache_bytes=bytes;
    bank->cache=memory;
    return 1;
}
void native_cart_stats(uint32_t *cached,uint32_t *file)
{
    *cached=__atomic_load_n(&cached_reads,__ATOMIC_RELAXED);
    *file=__atomic_load_n(&file_reads,__ATOMIC_RELAXED);
}
void native_cart_reset(void)
{
    /* Call after readers have stopped, never from an active DMA callback. */
    while (count) {
        CartBank *bank=&banks[--count];
        free(bank->cache);bank->cache=NULL;bank->cache_bytes=0;
        fclose(bank->file);
        pthread_mutex_destroy(&bank->lock);
        bank->file=NULL;
    }
    cached_reads=file_reads=0;
}

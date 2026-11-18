/* Host reference runner. Uses the same original source as the ARM probe. */
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include "engine_probe.h"
#include "native_ram.h"
#include "native_cart.h"
void osSyncPrintf(const char *fmt,...)
{
    va_list args;
    va_start(args,fmt);
    vfprintf(stderr,fmt,args);
    va_end(args);
}
/* Enum size stays internal to the game. This boundary passes a 32-bit level. */
void sysLogPrintf(int level,const char *fmt,...)
{
    (void)level;
    va_list args;
    va_start(args,fmt);
    vfprintf(stderr,fmt,args);
    va_end(args);
}
uint64_t sysGetMicroseconds(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC,&now);
    return (uint64_t)now.tv_sec*1000000u+(uint64_t)now.tv_nsec/1000u;
}
void sysSleep(uint32_t micros)
{
    struct timespec delay={micros/1000000u,(micros%1000000u)*1000u};
    nanosleep(&delay,NULL);
}
uintptr_t sysImageBase(void){return 0;}
void sysFatalError(const char *fmt,...)
{
    va_list args;
    va_start(args,fmt);
    vfprintf(stderr,fmt,args);
    va_end(args);
    exit(1);
}
static void report(const char *name,int success,unsigned value)
{
    if (name[0] && value == 0 && !success) fflush(stderr);
    printf("%s\t%u\t%s\n",success ? "PASS" : "FAIL",value,name);
    fflush(stdout);
}
int main(int argc,char **argv)
{
    if (argc != 2) return 2;
    int result=engine_probe(argv[1],report) ? 0 : 1;
    native_cart_reset();native_ram_destroy();
    return result;
}

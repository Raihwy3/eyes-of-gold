#ifndef EOG_NATIVE_LOG_POLICY_H
#define EOG_NATIVE_LOG_POLICY_H
#include <stdio.h>
#include <sys/stat.h>
/* Opt in once, before workers start. Never truncate an old log without a flag. */
static inline FILE *native_log_open(const char *directory) {
    char path[512];
    struct stat info;
    int n=snprintf(path,sizeof path,"%s/debug-log.flag",directory);
    if(n<0 || (size_t)n>=sizeof path || stat(path,&info)!=0 || !S_ISREG(info.st_mode))return NULL;
    n=snprintf(path,sizeof path,"%s/native-game.log",directory);
    if(n<0 || (size_t)n>=sizeof path)return NULL;
    return fopen(path,"w");
}
#endif

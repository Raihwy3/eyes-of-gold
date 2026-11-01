#ifndef EOG_ENGINE_PROBE_H
#define EOG_ENGINE_PROBE_H
typedef void (*EngineReport)(const char *name,int success,unsigned value);
/* Runs original upstream routines; no recreation/gameplay substitute. */
int engine_probe(const char *path,EngineReport report);
#endif

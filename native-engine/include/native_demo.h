#ifndef EOG_NATIVE_DEMO_H
#define EOG_NATIVE_DEMO_H
/* The original replay subsystem owns this flag. Diagnostic harnesses may omit
 * that subsystem, hence the weak reference rather than a second replay state. */
extern int get_is_ramrom_flag(void) __attribute__((weak));
static inline int native_demo_active(void) {
    return get_is_ramrom_flag && get_is_ramrom_flag()!=0;
}
enum NativeDemoStop {NATIVE_DEMO_CHECKSUM,NATIVE_DEMO_INPUT,NATIVE_DEMO_END,NATIVE_DEMO_TIMER};
void native_demo_replay_begin(void);
void native_demo_replay_seed(unsigned expected,unsigned actual);
void native_demo_replay_stop(int reason);
#endif

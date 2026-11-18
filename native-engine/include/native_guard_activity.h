#ifndef EOG_NATIVE_GUARD_ACTIVITY_H
#define EOG_NATIVE_GUARD_ACTIVITY_H
/* Game-thread only; these never change render-thread state. */
struct ChrRecord;
struct PropRecord;
int native_guard_activity_beyond_fog(struct ChrRecord *chr);
int native_guard_activity_drawable(struct PropRecord *prop);
int native_guard_activity_awake(struct ChrRecord *chr);
int native_guard_activity_sight_allowed(struct ChrRecord *chr);
int native_guard_activity_ai_due(struct ChrRecord *chr);
void native_guard_activity_sleep(struct ChrRecord *chr);
void native_guard_activity_reset(void);
void native_guard_activity_report(void);
#endif

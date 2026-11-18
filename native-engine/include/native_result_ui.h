#ifndef EOG_RESULT_UI_H
#define EOG_RESULT_UI_H
/* MENU_MISSION_FAILED and MENU_MISSION_COMPLETE are report/statistics pages,
 * not the outcome. Both must show the same original outcome flags. */
static inline const char *native_result_title(int briefing,int kia,int aborted,int success) {
    return briefing?"MISSION BRIEFING":kia?"KILLED IN ACTION":aborted?"MISSION ABORTED":success?"MISSION COMPLETE":"MISSION FAILED";
}
#endif

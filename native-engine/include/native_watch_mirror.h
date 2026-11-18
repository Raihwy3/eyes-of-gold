#ifndef EOG_WATCH_MIRROR_H
#define EOG_WATCH_MIRROR_H
/* Lay out the authored UI content, independently of the top watch camera.
 * The five page strips occupy y226..232 and bypass this content mapping. */
#define EOG_WATCH_LEFT 48
#define EOG_WATCH_TOP 20
#define EOG_WATCH_WIDTH 232
#define EOG_WATCH_HEIGHT 192
static inline float native_watch_content_x(float x){return 8.f+(x-EOG_WATCH_LEFT)*(304.f/EOG_WATCH_WIDTH);}
static inline float native_watch_content_y(float y){return 8.f+(y-EOG_WATCH_TOP)*(208.f/EOG_WATCH_HEIGHT);}
/* Status/ammo/abort uses a narrower authored column than Options. Expand
 * that page independently, and use the exact inverse for touch hit testing. */
static inline float native_watch_page_content_x(float x,int page){return page==0?8.f+(x-64.f)*(304.f/192.f):native_watch_content_x(x);}
static inline float native_watch_page_content_y(float y,int page){return page==0?8.f+(y-40.f)*(208.f/172.f):native_watch_content_y(y);}
static inline int native_watch_page_touch_y(int y,int page){return page==0?40+(y-8)*172/208:EOG_WATCH_TOP+(y-8)*EOG_WATCH_HEIGHT/208;}
static inline int native_watch_page_touch_x(int x,int page){return page==0?64+(x-8)*192/304:EOG_WATCH_LEFT+(x-8)*EOG_WATCH_WIDTH/304;}
static inline int native_watch_touch_x(int x){return EOG_WATCH_LEFT+(x-8)*EOG_WATCH_WIDTH/304;}
static inline int native_watch_touch_y(int y){return EOG_WATCH_TOP+(y-8)*EOG_WATCH_HEIGHT/208;}
#endif

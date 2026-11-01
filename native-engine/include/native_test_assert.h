#ifndef NATIVE_TEST_ASSERT_H
#define NATIVE_TEST_ASSERT_H
/* N64 assert.h only prints. Host regressions must fail their process. */
#include <stdio.h>
#include <stdlib.h>
#undef assert
#define assert(condition) do { if(!(condition)) { \
 fprintf(stderr,"Assertion failed: %s (%s:%d)\n",#condition,__FILE__,__LINE__);abort(); \
} } while(0)
#endif

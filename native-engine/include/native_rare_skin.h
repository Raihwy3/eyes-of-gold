#ifndef EOG_NATIVE_RARE_SKIN_H
#define EOG_NATIVE_RARE_SKIN_H
#include <stdint.h>
/* This generator is private to cosmetics and never advances gameplay RNG.
 * xorshift32 visits nonzero uint32 values. Accept a multiple of 100 values
 * from that range so all 100 outcomes have the same number of inputs. */
static inline uint32_t native_skin_next(uint32_t *state) {
    uint32_t x=*state;x^=x<<13;x^=x>>17;x^=x<<5;*state=x;return x;
}
static inline int native_skin_outcome(uint32_t value) {
    return !value || value>4294967200u?-1:value%100u==0;
}
#endif

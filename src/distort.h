#ifndef _DISTORT_H_
#define _DISTORT_H_

#include "types.h"
#include "libfixmath/fix16.h"

typedef struct {
    struct {
        fix16_t ratio, shift, bend, compress;
    } param;
    struct {
        fix16_t factor, offset;
    } state;
} distort_t;

extern void distort_init(distort_t *d);
extern void distort_set_ratio(distort_t *d, fix16_t v);
extern void distort_set_shift(distort_t *d, fix16_t v);
extern void distort_set_bend(distort_t *d, fix16_t v);
extern void distort_set_compress(distort_t *d, fix16_t v);

extern fix16_t distort_phase(distort_t *d, fix16_t phase);
extern fix16_t distort_shape(distort_t *d, fix16_t in);

#endif // _DISTORT_H_
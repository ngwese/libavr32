#include "distort.h"

static fix16_t fix16_half;
static fix16_t fix16_neg_one;
static fix16_t shape_sf;
static fix16_t shape_rmax;

static inline fix16_t fix16_lo(fix16_t v) {
    return  0x0000FFFF & v;
}

static inline fix16_t fix16_hi(fix16_t v) {
    return 0xFFFF0000 & v;
}

static inline fix16_t fix16_clamp(fix16_t v, fix16_t max) {
    return v > max ? max : v;
}

void distort_init(distort_t *d) {
    // TODO: move this to be constant static values
    static int initialized = 0;
    if (!initialized) {
        fix16_half = fix16_from_float(0.5);
        fix16_neg_one = fix16_from_int(-1);

        shape_sf = fix16_from_int(32);
        shape_rmax = fix16_mul(shape_sf, shape_sf);

        initialized = 1;
    }

    distort_set_ratio(d, fix16_one);
    distort_set_shift(d, 0);
    distort_set_bend(d, 0);
    distort_set_compress(d, fix16_one);
}

void distort_set_ratio(distort_t *d, fix16_t v) {
    if (v >= fix16_half && v <= fix16_two) {
        d->param.ratio = v;
        d->state.factor = fix16_div(fix16_one, d->param.ratio);
    }
}

void distort_set_shift(distort_t *d, fix16_t v) {
    if (v >= fix16_neg_one && v <= fix16_one) {
        d->param.shift = v;
        d->state.offset = v < 0 ? fix16_add(fix16_one, v) : v;
    }
}

void distort_set_bend(distort_t *d, fix16_t v) {
    if (v >= fix16_neg_one && v <= fix16_one) {
        d->param.bend = v;
    }
}

void distort_set_compress(distort_t *d, fix16_t v) {
    if (v >= fix16_one) {
        d->param.compress = v;
    }
}

fix16_t distort_phase(distort_t *d, fix16_t phase) {
    // scale and shift; wrapping to [0, 1)
    return fix16_lo(fix16_add(fix16_mul(phase, d->state.factor), d->state.offset));
}

fix16_t distort_shape(distort_t *d, fix16_t in) {
    fix16_t s, v, e, b;

    if (d->param.bend < 0) {
        // inverse exp...
        b = fix16_mul(d->param.bend, fix16_neg_one);
        e = fix16_mul(fix16_sub(fix16_one, in), shape_sf);
        e = fix16_div(fix16_mul(e, e), shape_rmax);
        e = fix16_sub(fix16_one, e);
        // printf(" inverse b, e: 0x%.8x, 0x%.8x\n", b, e);
    } else {
        // straight exp...
        b = d->param.bend;
        e = fix16_mul(in, shape_sf);
        e = fix16_div(fix16_mul(e, e), shape_rmax);
        // printf("straight b, e: 0x%.8x, 0x%.8x\n", b, e);
    }

    if (b == 0) {
        // printf("       use in: 0x%.8x, 0x%.8x\n", in, e);
        v = in;
    } else if (b == fix16_one) {
        // printf("        use e: 0x%.8x, 0x%.8x\n", in, e);
        v = e;
    } else {
        // printf("         lerp: 0x%.8x, 0x%.8x\n", in, e);
        v = fix16_lerp16(in, e, b);
    }
    // printf("            v: 0x%.8x\n", v);

    // comp is multiply the output value and clamp to 1
    return fix16_clamp(fix16_mul(v, d->param.compress), fix16_one);
}

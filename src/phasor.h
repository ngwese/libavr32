#ifndef _PHASOR_H_
#define _PHASOR_H_

#include "types.h"

// called for each division of a cycle, now is current division, reset
// will be true if the phasor was externally reset
typedef void (phasor_callback_t)(u8 now, bool reset);

// initialize hardware
void init_phasor(void);

// configure behavior
int phasor_setup(u16 hz, u8 divisions);

int phasor_start(void);
int phasor_stop(void);
void phasor_reset(void);

int phasor_set_frequency(u16 hz);
void phasor_set_callback(phasor_callback_t *cb);

#endif

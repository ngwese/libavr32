// asf
#include "compiler.h"
#include "print_funcs.h"
#include "tc.h"

// libavr32
#include "conf_tc_irq.h"
#include "phasor.h"
#include "util.h"

// TEMP
#include "conf_board.h"
#include "gpio.h"


static phasor_callback_t *_phasor_cb;

volatile static u16 _now;
volatile static bool _reset;

static u8 _ticks;

// -------------------------------------------------------------------------------
// functions (internal)
// -------------------------------------------------------------------------------


// initialize second tc channel
static void init_phasor_tc(void) {
  volatile avr32_tc_t *tc = APP_TC;

  // waveform options
  static const tc_waveform_opt_t waveform_opt = {
    .channel  = PHASOR_TC_CHANNEL, // channel
    .bswtrg   = TC_EVT_EFFECT_NOOP, // software trigger action on TIOB
    .beevt    = TC_EVT_EFFECT_NOOP, // external event action
    .bcpc     = TC_EVT_EFFECT_NOOP, // rc compare action
    .bcpb     = TC_EVT_EFFECT_NOOP, // rb compare
    .aswtrg   = TC_EVT_EFFECT_NOOP, // soft trig on TIOA
    .aeevt    = TC_EVT_EFFECT_NOOP, // etc
    .acpc     = TC_EVT_EFFECT_NOOP,
    .acpa     = TC_EVT_EFFECT_NOOP,
    // Waveform selection: Up mode with automatic trigger(reset) on RC compare.
    .wavsel   = TC_WAVEFORM_SEL_UP_MODE_RC_TRIGGER,
    .enetrg   = false,             // external event trig
    .eevt     = 0,                 // extern event select
    .eevtedg  = TC_SEL_NO_EDGE,    // extern event edge
    .cpcdis   = false,             // counter disable when rc compare
    .cpcstop  = false,             // counter stopped when rc compare
    .burst    = false,
    .clki     = false,
    // Internal source clock 5, connected to fPBA / 128.
    .tcclks   = TC_CLOCK_SOURCE_TC5
  };

  // Options for enabling TC interrupts
  static const tc_interrupt_t tc_interrupt = {
    .etrgs = 0,
    .ldrbs = 0,
    .ldras = 0,
    .cpcs  = 1, // enable interrupt on RC compare alone
    .cpbs  = 0,
    .cpas  = 0,
    .lovrs = 0,
    .covfs = 0
  };
  // Initialize the timer/counter.
  tc_init_waveform(tc, &waveform_opt);

	// TODO: configure timer to fire at hz * ticks; will need to put
	// some upper bound on this for safety..

	// set timer compare trigger.
  // we want it to overflow and generate an interrupt every .2 ms
  // so (1 / fPBA / 128) * RC = 0.001
  // so RC = fPBA / 128 / 200
  //tc_write_rc(tc, PHASOR_TC_CHANNEL, (FPBA_HZ / 25600));
	//tc_write_rc(tc, PHASOR_TC_CHANNEL, (FPBA_HZ / 1280000));
	tc_write_rc(tc, PHASOR_TC_CHANNEL, (FPBA_HZ / 25600));

  // configure the timer interrupt
  tc_configure_interrupts(tc, PHASOR_TC_CHANNEL, &tc_interrupt);
}


__attribute__((__interrupt__))
static void irq_phasor(void) {
	// test
	if (_now == 0) {
		gpio_set_gpio_pin(B10);
	}
	else if (_now == _ticks >> 1) {
		gpio_clr_gpio_pin(B10);
	}

	//(*_phasor_cb)(now, reset);

	if (_now == 0 && _reset) {
		_reset = false;
	}
	
	_now++;
	if (_now >= _ticks) {
		_now = 0;
	}
	
  // clear interrupt flag by reading timer SR
  tc_read_sr(APP_TC, PHASOR_TC_CHANNEL);
}


static void null_cb(u8 now, bool reset) {
}


// -----------------------------------------------------------------------------
// functions (external)
// -----------------------------------------------------------------------------

void init_phasor(void) {
	phasor_set_callback(NULL);

	init_phasor_tc();

	INTC_register_interrupt(&irq_phasor, PHASOR_TC_IRQ, UI_IRQ_PRIORITY);
}

u16 phasor_setup(u16 hz, u8 ticks) {
	_ticks = ticks < 1 ? 1 : ticks;
	return phasor_set_frequency(hz);
}

int phasor_start(void) {
	_now = 0;
	_reset = false;
	print_dbg("\r\n>> pre tc_start");
	int i = tc_start(APP_TC, PHASOR_TC_CHANNEL);
	print_dbg("\r\n>> post tc_start: ");
	print_dbg_ulong(i);
	return i;
}

int phasor_stop(void) {
	return tc_stop(APP_TC, PHASOR_TC_CHANNEL);
}

void phasor_reset(void) {
	cpu_irq_disable_level(APP_TC_IRQ_PRIORITY);

	_now = 0;
	_reset = true;

	cpu_irq_enable_level(APP_TC_IRQ_PRIORITY);
}
	
u16 phasor_set_frequency(u16 hz) {
	u16 rate, rc;

	// limit range, 7Hz is about as slow as possible with a 16 bit
	// counter; 4kHz is just a safety, depending on what happens in the
	// phasor callback 4kHz might be too fast.
	rate = uclip(hz * _ticks, 7, 4000);

	rc = FPBA_HZ / (128 * rate);
	tc_write_rc(APP_TC, PHASOR_TC_CHANNEL, rc);

	return rate;
}

void phasor_set_callback(phasor_callback_t *cb) {
	_phasor_cb = cb == NULL ? &null_cb : cb;
}



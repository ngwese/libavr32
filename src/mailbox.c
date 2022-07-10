// asf
#include "compiler.h"

// libavr32
#include "conf_tc_irq.h"
#include "mailbox.h"

void mailbox_init(mailbox_t *mb) {
	mb->empty = true;
	mb->value.which = 0;
	mb->value.data = 0;
}

bool mailbox_post(mailbox_t *mb, message_t *p, post_mode_t mode) {
	bool posted = false;
	
	cpu_irq_disable_level(APP_TC_IRQ_PRIORITY);

	if (mode == kPostReplace || (mode == kPostRetain && mb->empty)) {
		mb->value.which = p->which;
		mb->value.data = p->data;
		mb->empty = false;
		posted = true;
	}

	cpu_irq_enable_level(APP_TC_IRQ_PRIORITY);

	return posted;
}

bool mailbox_get(mailbox_t *mb, message_t *p) {
	bool got = false;

	cpu_irq_disable_level(APP_TC_IRQ_PRIORITY);

	if (!mb->empty) {
		p->which = mb->value.which;
		p->data = mb->value.data;
		mb->empty = got = true;
	}

	cpu_irq_enable_level(APP_TC_IRQ_PRIORITY);

	return got;
}

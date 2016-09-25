#ifndef _MAILBOX_H_
#define _MAILBOX_H_

#include "types.h"

typedef struct {
  u8 which;
  s32 data;
} message_t;

typedef enum {
  kPostReplace,
  kPostRetain
} post_mode_t;

typedef struct {
  message_t value;
  bool empty;
} mailbox_t;

void mailbox_init(mailbox_t *mb);
bool mailbox_post(mailbox_t *mb, message_t *p, post_mode_t mode);
bool mailbox_get(mailbox_t *mb, message_t *p);

#endif // header guard

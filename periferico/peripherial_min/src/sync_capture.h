#ifndef SYNC_CAPTURE_H_
#define SYNC_CAPTURE_H_

#include <stdint.h>

int sync_capture_init(void);
int sync_capture_handle_command(uint8_t command);

#endif /* SYNC_CAPTURE_H_ */

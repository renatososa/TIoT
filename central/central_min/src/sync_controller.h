#ifndef SYNC_CONTROLLER_H_
#define SYNC_CONTROLLER_H_

#include <stdbool.h>
#include <stdint.h>

int sync_controller_init(void);
void sync_controller_session_ready(void);
void sync_controller_disconnected(void);
bool sync_controller_process_notification(const uint8_t *data, uint16_t len);

#endif /* SYNC_CONTROLLER_H_ */

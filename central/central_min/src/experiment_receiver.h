#ifndef EXPERIMENT_RECEIVER_H_
#define EXPERIMENT_RECEIVER_H_

#include <stdint.h>

void experiment_receiver_reset(void);
void experiment_receiver_process(const uint8_t *data, uint16_t len);
void experiment_receiver_dump(void);

#endif /* EXPERIMENT_RECEIVER_H_ */

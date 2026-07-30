#ifndef EXPERIMENT_H_
#define EXPERIMENT_H_

#include <stdbool.h>

#include "emg_protocol.h"

int experiment_start(void);
void experiment_start_data(void);
void experiment_stop_data(void);
bool experiment_is_running(void);
uint8_t experiment_stats_count(void);
void experiment_get_stats(uint8_t index, struct emg_experiment_stats *stats);

#endif /* EXPERIMENT_H_ */

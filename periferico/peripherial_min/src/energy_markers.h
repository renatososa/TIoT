#ifndef ENERGY_MARKERS_H_
#define ENERGY_MARKERS_H_

#include <stdbool.h>

int energy_markers_init(void);
void energy_markers_advertising(bool active);
void energy_markers_connected(bool active);
void energy_markers_profile(bool active);
void energy_markers_acquisition(bool active);
void energy_markers_transmitter(bool active);
void energy_markers_all_off(void);

#endif /* ENERGY_MARKERS_H_ */

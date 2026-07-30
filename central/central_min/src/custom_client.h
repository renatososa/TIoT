#ifndef CUSTOM_CLIENT_H_
#define CUSTOM_CLIENT_H_

#include <stdint.h>

int custom_client_start(void);
int custom_client_send_control(uint8_t command);
int custom_client_print_link_state(void);

#endif /* CUSTOM_CLIENT_H_ */

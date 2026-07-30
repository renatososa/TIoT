#ifndef CUSTOM_GATT_H_
#define CUSTOM_GATT_H_

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/bluetooth/conn.h>

#define CUSTOM_GATT_MAX_PAYLOAD_LEN 244

void custom_gatt_connected(struct bt_conn *conn);
void custom_gatt_disconnected(struct bt_conn *conn);
int custom_gatt_send(const uint8_t *data, uint16_t len);
bool custom_gatt_is_ready(void);

#endif /* CUSTOM_GATT_H_ */

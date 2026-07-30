#include <errno.h>
#include <string.h>

#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "custom_gatt.h"
#include "emg_protocol.h"
#include "sync_capture.h"

#define TX_CHAR_ATTR_INDEX 4

#define BT_UUID_MIN_SERVICE_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef0)
#define BT_UUID_MIN_RX_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef1)
#define BT_UUID_MIN_TX_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef2)

static struct bt_conn *current_conn;
static bool notify_enabled;
static uint8_t tx_payload[CUSTOM_GATT_MAX_PAYLOAD_LEN];
static uint16_t tx_payload_len;

static struct bt_uuid_128 min_service_uuid = BT_UUID_INIT_128(BT_UUID_MIN_SERVICE_VAL);
static struct bt_uuid_128 min_rx_uuid = BT_UUID_INIT_128(BT_UUID_MIN_RX_VAL);
static struct bt_uuid_128 min_tx_uuid = BT_UUID_INIT_128(BT_UUID_MIN_TX_VAL);

static K_MUTEX_DEFINE(gatt_mutex);
static K_MUTEX_DEFINE(send_mutex);
static K_SEM_DEFINE(notification_complete, 0, 1);

static void ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	ARG_UNUSED(attr);

	k_mutex_lock(&gatt_mutex, K_FOREVER);
	notify_enabled = (value == BT_GATT_CCC_NOTIFY);
	k_mutex_unlock(&gatt_mutex);

	printk("Notifications %s\n", notify_enabled ? "enabled" : "disabled");
}

static ssize_t tx_read_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			  void *buf, uint16_t len, uint16_t offset)
{
	ssize_t result;

	k_mutex_lock(&gatt_mutex, K_FOREVER);
	result = bt_gatt_attr_read(conn, attr, buf, len, offset,
				   tx_payload, tx_payload_len);
	k_mutex_unlock(&gatt_mutex);

	return result;
}

static ssize_t rx_write_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			   const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
	uint8_t command;
	int err;

	ARG_UNUSED(conn);
	ARG_UNUSED(attr);
	ARG_UNUSED(flags);

	if (offset != 0U) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	if (len > CUSTOM_GATT_MAX_PAYLOAD_LEN) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	if (emg_control_decode(&command, buf, len) != 0) {
		return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}

	err = sync_capture_handle_command(command);
	if (err) {
		return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
	}

	return len;
}

BT_GATT_SERVICE_DEFINE(min_service,
	BT_GATT_PRIMARY_SERVICE(&min_service_uuid),
	BT_GATT_CHARACTERISTIC(&min_rx_uuid.uuid,
			       BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
			       BT_GATT_PERM_WRITE,
			       NULL, rx_write_cb, NULL),
	BT_GATT_CHARACTERISTIC(&min_tx_uuid.uuid,
			       BT_GATT_CHRC_NOTIFY | BT_GATT_CHRC_READ,
			       BT_GATT_PERM_READ,
			       tx_read_cb, NULL, tx_payload),
	BT_GATT_CCC(ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

void custom_gatt_connected(struct bt_conn *conn)
{
	k_mutex_lock(&gatt_mutex, K_FOREVER);
	if (current_conn != NULL) {
		bt_conn_unref(current_conn);
	}
	current_conn = bt_conn_ref(conn);
	k_mutex_unlock(&gatt_mutex);
}

void custom_gatt_disconnected(struct bt_conn *conn)
{
	k_mutex_lock(&gatt_mutex, K_FOREVER);
	notify_enabled = false;
	if (current_conn == conn) {
		bt_conn_unref(current_conn);
		current_conn = NULL;
	}
	k_mutex_unlock(&gatt_mutex);
}

static void notification_sent(struct bt_conn *conn, void *user_data)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(user_data);
	k_sem_give(&notification_complete);
}

int custom_gatt_send(const uint8_t *data, uint16_t len)
{
	struct bt_conn *conn;
	struct bt_gatt_notify_params params = {0};
	int err;

	if (len > CUSTOM_GATT_MAX_PAYLOAD_LEN) {
		return -EMSGSIZE;
	}

	k_mutex_lock(&send_mutex, K_FOREVER);
	k_mutex_lock(&gatt_mutex, K_FOREVER);
	conn = current_conn ? bt_conn_ref(current_conn) : NULL;
	if ((conn == NULL) || !notify_enabled) {
		k_mutex_unlock(&gatt_mutex);
		if (conn != NULL) {
			bt_conn_unref(conn);
		}
		k_mutex_unlock(&send_mutex);
		return -ENOTCONN;
	}

	memcpy(tx_payload, data, len);
	tx_payload_len = len;
	k_mutex_unlock(&gatt_mutex);

	k_sem_reset(&notification_complete);
	params.attr = &min_service.attrs[TX_CHAR_ATTR_INDEX];
	params.data = tx_payload;
	params.len = tx_payload_len;
	params.func = notification_sent;

	err = bt_gatt_notify_cb(conn, &params);
	if (err == 0) {
		err = k_sem_take(&notification_complete, K_SECONDS(2));
	}
	bt_conn_unref(conn);
	k_mutex_unlock(&send_mutex);

	return err;
}

bool custom_gatt_is_ready(void)
{
	bool ready;

	k_mutex_lock(&gatt_mutex, K_FOREVER);
	ready = (current_conn != NULL) && notify_enabled;
	k_mutex_unlock(&gatt_mutex);

	return ready;
}

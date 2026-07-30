#include <errno.h>
#include <stdbool.h>
#include <string.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "custom_client.h"
#include "emg_protocol.h"
#include "experiment_receiver.h"
#include "sync_controller.h"

#define TARGET_NAME "Xiao_GATT_Min"

#define BT_UUID_MIN_SERVICE_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef0)
#define BT_UUID_MIN_RX_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef1)
#define BT_UUID_MIN_TX_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef2)

enum discovery_step {
	DISCOVERY_IDLE,
	DISCOVERY_SERVICE,
	DISCOVERY_RX,
	DISCOVERY_TX,
};

struct scan_match {
	bool matched;
};

static struct bt_conn *current_conn;
static struct bt_gatt_discover_params discover_params;
static struct bt_gatt_subscribe_params subscribe_params;
static K_MUTEX_DEFINE(control_write_mutex);
static K_SEM_DEFINE(control_write_complete, 0, 1);
static uint8_t control_write_att_error;

static enum discovery_step discovery_state;
static uint16_t service_start_handle;
static uint16_t service_end_handle;
static uint16_t rx_handle;
static uint16_t tx_handle;

static struct bt_uuid_128 service_uuid = BT_UUID_INIT_128(BT_UUID_MIN_SERVICE_VAL);
static struct bt_uuid_128 rx_uuid = BT_UUID_INIT_128(BT_UUID_MIN_RX_VAL);
static struct bt_uuid_128 tx_uuid = BT_UUID_INIT_128(BT_UUID_MIN_TX_VAL);

static void att_mtu_updated(struct bt_conn *conn, uint16_t tx, uint16_t rx)
{
	ARG_UNUSED(conn);
	printk("ATT MTU updated: tx=%u rx=%u\n", tx, rx);
}

static struct bt_gatt_cb gatt_callbacks = {
	.att_mtu_updated = att_mtu_updated,
};

static void start_scan(void);
static int discover_service(void);
static int discover_rx_characteristic(void);
static int discover_tx_characteristic(void);
static int subscribe_to_tx(void);

static bool data_cb(struct bt_data *data, void *user_data)
{
	struct scan_match *match = user_data;

	if (data->type != BT_DATA_NAME_COMPLETE) {
		return true;
	}

	if ((data->data_len == strlen(TARGET_NAME)) &&
	    (memcmp(data->data, TARGET_NAME, data->data_len) == 0)) {
		match->matched = true;
		return false;
	}

	return true;
}

static bool advertisement_matches(struct net_buf_simple *ad)
{
	struct scan_match match = {0};

	bt_data_parse(ad, data_cb, &match);
	return match.matched;
}

static void control_write_cb(struct bt_conn *conn, uint8_t err,
			     struct bt_gatt_write_params *params)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(params);

	control_write_att_error = err;
	k_sem_give(&control_write_complete);
}

int custom_client_send_control(uint8_t command)
{
	uint8_t payload[EMG_CONTROL_SIZE];
	struct bt_gatt_write_params params = {0};
	struct bt_conn *conn;
	int err;

	k_mutex_lock(&control_write_mutex, K_FOREVER);
	if ((current_conn == NULL) || (rx_handle == 0U)) {
		k_mutex_unlock(&control_write_mutex);
		return -ENOTCONN;
	}

	conn = bt_conn_ref(current_conn);
	emg_control_encode(payload, command);
	params.func = control_write_cb;
	params.handle = rx_handle;
	params.offset = 0U;
	params.data = payload;
	params.length = sizeof(payload);
	control_write_att_error = 0U;
	k_sem_reset(&control_write_complete);

	err = bt_gatt_write(conn, &params);
	if (err == 0) {
		err = k_sem_take(&control_write_complete, K_SECONDS(2));
		if (err != 0) {
			bt_gatt_cancel(conn, &params);
		} else if (control_write_att_error != 0U) {
			err = -EIO;
		}
	}

	bt_conn_unref(conn);
	k_mutex_unlock(&control_write_mutex);
	return err;
}

static uint8_t notify_cb(struct bt_conn *conn, struct bt_gatt_subscribe_params *params,
			 const void *data, uint16_t length)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(params);

	if (data == NULL) {
		printk("Notification subscription removed\n");
		return BT_GATT_ITER_STOP;
	}

	if (!sync_controller_process_notification(data, length)) {
		experiment_receiver_process(data, length);
	}

	return BT_GATT_ITER_CONTINUE;
}

static void subscribe_cb(struct bt_conn *conn, uint8_t err,
			 struct bt_gatt_subscribe_params *params)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(params);

	if (err) {
		printk("Subscribe failed: 0x%02x\n", err);
		return;
	}

	printk("Subscribed to TX notifications\n");
	sync_controller_session_ready();
}

static uint8_t discover_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			   struct bt_gatt_discover_params *params)
{
	int err;

	if (attr == NULL) {
		printk("Discovery failed in step %d\n", discovery_state);
		memset(params, 0, sizeof(*params));
		return BT_GATT_ITER_STOP;
	}

	if (discovery_state == DISCOVERY_SERVICE) {
		const struct bt_gatt_service_val *service = attr->user_data;

		service_start_handle = attr->handle;
		service_end_handle = service->end_handle;
		printk("Service found: start=0x%04x end=0x%04x\n",
		       service_start_handle, service_end_handle);

		memset(params, 0, sizeof(*params));
		err = discover_rx_characteristic();
		if (err) {
			printk("RX characteristic discovery start failed: %d\n", err);
		}
		return BT_GATT_ITER_STOP;
	}

	if ((discovery_state == DISCOVERY_RX) || (discovery_state == DISCOVERY_TX)) {
		const struct bt_gatt_chrc *chrc = attr->user_data;

		if (discovery_state == DISCOVERY_RX) {
			rx_handle = chrc->value_handle;
			printk("RX characteristic found: handle=0x%04x\n", rx_handle);

			memset(params, 0, sizeof(*params));
			err = discover_tx_characteristic();
			if (err) {
				printk("TX characteristic discovery start failed: %d\n", err);
			}
			return BT_GATT_ITER_STOP;
		}

		tx_handle = chrc->value_handle;
		printk("TX characteristic found: handle=0x%04x\n", tx_handle);

		memset(params, 0, sizeof(*params));
		err = subscribe_to_tx();
		if (err) {
			printk("Subscribe request failed: %d\n", err);
		}
		return BT_GATT_ITER_STOP;
	}

	return BT_GATT_ITER_STOP;
}

static int discover_service(void)
{
	discovery_state = DISCOVERY_SERVICE;

	discover_params.func = discover_cb;
	discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
	discover_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
	discover_params.type = BT_GATT_DISCOVER_PRIMARY;
	discover_params.uuid = &service_uuid.uuid;

	return bt_gatt_discover(current_conn, &discover_params);
}

static int discover_rx_characteristic(void)
{
	discovery_state = DISCOVERY_RX;

	discover_params.func = discover_cb;
	discover_params.start_handle = service_start_handle + 1U;
	discover_params.end_handle = service_end_handle;
	discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;
	discover_params.uuid = &rx_uuid.uuid;

	return bt_gatt_discover(current_conn, &discover_params);
}

static int discover_tx_characteristic(void)
{
	discovery_state = DISCOVERY_TX;

	discover_params.func = discover_cb;
	discover_params.start_handle = service_start_handle + 1U;
	discover_params.end_handle = service_end_handle;
	discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;
	discover_params.uuid = &tx_uuid.uuid;

	return bt_gatt_discover(current_conn, &discover_params);
}

static int subscribe_to_tx(void)
{
	memset(&subscribe_params, 0, sizeof(subscribe_params));

	subscribe_params.notify = notify_cb;
	subscribe_params.subscribe = subscribe_cb;
	subscribe_params.value = BT_GATT_CCC_NOTIFY;
	subscribe_params.value_handle = tx_handle;
	subscribe_params.ccc_handle = BT_GATT_AUTO_DISCOVER_CCC_HANDLE;
	subscribe_params.end_handle = service_end_handle;
	subscribe_params.disc_params = &discover_params;

	return bt_gatt_subscribe(current_conn, &subscribe_params);
}

static void reset_client_state(void)
{
	discovery_state = DISCOVERY_IDLE;
	service_start_handle = 0U;
	service_end_handle = 0U;
	rx_handle = 0U;
	tx_handle = 0U;
	memset(&discover_params, 0, sizeof(discover_params));
	memset(&subscribe_params, 0, sizeof(subscribe_params));
}

static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];
	int discover_err;

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (err) {
		printk("Connection failed to %s: %u\n", addr, err);
		if (current_conn != NULL) {
			bt_conn_unref(current_conn);
			current_conn = NULL;
		}
		start_scan();
		return;
	}

	printk("Connected: %s\n", addr);
	reset_client_state();
	experiment_receiver_reset();

	discover_err = discover_service();
	if (discover_err) {
		printk("Service discovery start failed: %d\n", discover_err);
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	printk("Disconnected: %s reason=0x%02x\n", addr, reason);
	sync_controller_disconnected();

	if (current_conn == conn) {
		bt_conn_unref(current_conn);
		current_conn = NULL;
	}

	reset_client_state();
	start_scan();
}

static void le_param_updated(struct bt_conn *conn, uint16_t interval,
			     uint16_t latency, uint16_t timeout)
{
	ARG_UNUSED(conn);
	printk("BLE parameter update: interval=%u us latency=%u timeout=%u ms\n",
	       BT_CONN_INTERVAL_TO_US(interval), latency, timeout * 10U);
}

#if defined(CONFIG_BT_USER_PHY_UPDATE)
static void le_phy_updated(struct bt_conn *conn,
			   struct bt_conn_le_phy_info *info)
{
	ARG_UNUSED(conn);
	printk("BLE PHY update: tx=%u rx=%u\n",
	       info->tx_phy, info->rx_phy);
}
#endif

#if defined(CONFIG_BT_USER_DATA_LEN_UPDATE)
static void le_data_len_updated(struct bt_conn *conn,
				struct bt_conn_le_data_len_info *info)
{
	ARG_UNUSED(conn);
	printk("BLE data length update: tx=%uB/%uus rx=%uB/%uus\n",
	       info->tx_max_len, info->tx_max_time,
	       info->rx_max_len, info->rx_max_time);
}
#endif

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
	.le_param_updated = le_param_updated,
#if defined(CONFIG_BT_USER_PHY_UPDATE)
	.le_phy_updated = le_phy_updated,
#endif
#if defined(CONFIG_BT_USER_DATA_LEN_UPDATE)
	.le_data_len_updated = le_data_len_updated,
#endif
};

static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
			 struct net_buf_simple *ad)
{
	char addr_str[BT_ADDR_LE_STR_LEN];
	int err;

	if (current_conn != NULL) {
		return;
	}

	if ((type != BT_GAP_ADV_TYPE_ADV_IND) &&
	    (type != BT_GAP_ADV_TYPE_EXT_ADV) &&
	    (type != BT_GAP_ADV_TYPE_SCAN_RSP)) {
		return;
	}

	if (!advertisement_matches(ad)) {
		return;
	}

	bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
	printk("Found target %s RSSI=%d\n", addr_str, rssi);

	err = bt_le_scan_stop();
	if ((err != 0) && (err != -EALREADY)) {
		printk("Scan stop failed: %d\n", err);
	}

	err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN,
				BT_LE_CONN_PARAM_DEFAULT, &current_conn);
	if (err) {
		printk("Create connection failed: %d\n", err);
		current_conn = NULL;
		start_scan();
		return;
	}

	printk("Connecting...\n");
}

static void start_scan(void)
{
	int err;

	if (current_conn != NULL) {
		return;
	}

	err = bt_le_scan_start(BT_LE_SCAN_ACTIVE, device_found);
	if (err == -EALREADY) {
		return;
	}

	if (err) {
		printk("Scan start failed: %d\n", err);
		return;
	}

	printk("Scanning for %s\n", TARGET_NAME);
}

int custom_client_start(void)
{
	reset_client_state();
	bt_gatt_cb_register(&gatt_callbacks);
	start_scan();
	return 0;
}

int custom_client_print_link_state(void)
{
	struct bt_conn_info info;
	struct bt_conn *conn;
	int err;

	if (current_conn == NULL) {
		return -ENOTCONN;
	}

	conn = bt_conn_ref(current_conn);
	err = bt_conn_get_info(conn, &info);
	if (err) {
		bt_conn_unref(conn);
		return err;
	}

	printk("L,interval_us,%u\n", info.le.interval_us);
	printk("L,latency,%u\n", info.le.latency);
	printk("L,timeout_ms,%u\n", info.le.timeout * 10U);
#if defined(CONFIG_BT_USER_PHY_UPDATE)
	if (info.le.phy != NULL) {
		printk("L,phy_tx,%u\n", info.le.phy->tx_phy);
		printk("L,phy_rx,%u\n", info.le.phy->rx_phy);
	}
#endif
#if defined(CONFIG_BT_USER_DATA_LEN_UPDATE)
	if (info.le.data_len != NULL) {
		printk("L,data_len_tx_bytes,%u\n",
		       info.le.data_len->tx_max_len);
		printk("L,data_len_rx_bytes,%u\n",
		       info.le.data_len->rx_max_len);
		printk("L,data_time_tx_us,%u\n",
		       info.le.data_len->tx_max_time);
		printk("L,data_time_rx_us,%u\n",
		       info.le.data_len->rx_max_time);
	}
#endif

	bt_conn_unref(conn);
	return 0;
}

#include <errno.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "custom_gatt.h"
#if defined(CONFIG_EMG_POSTEXPERIMENT)
#include "energy_markers.h"
#endif
#include "experiment.h"
#include "sync_capture.h"

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

#define ADV_RESTART_DELAY_MS 100
#define ADV_RETRY_DELAY_MS 250

static void advertising_start(struct k_work *work);

static K_WORK_DELAYABLE_DEFINE(adv_restart_work, advertising_start);

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data sd[] = {
	BT_DATA_BYTES(BT_DATA_UUID128_ALL,
		      BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef0)),
};

static void advertising_start(struct k_work *work)
{
	int err;

	ARG_UNUSED(work);

#if defined(CONFIG_EMG_POSTEXPERIMENT)
	energy_markers_advertising(true);
#endif
	err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_2, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err == -EALREADY) {
		printk("Advertising already running\n");
		return;
	}

	if (err) {
#if defined(CONFIG_EMG_POSTEXPERIMENT)
		energy_markers_advertising(false);
#endif
		printk("Advertising start failed: %d\n", err);
		k_work_reschedule(&adv_restart_work, K_MSEC(ADV_RETRY_DELAY_MS));
		return;
	}

	printk("Advertising started\n");
}

static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	if (err) {
#if defined(CONFIG_EMG_POSTEXPERIMENT)
		energy_markers_advertising(false);
		energy_markers_connected(false);
#endif
		printk("Connection failed: %u\n", err);
		k_work_reschedule(&adv_restart_work, K_MSEC(ADV_RESTART_DELAY_MS));
		return;
	}

#if defined(CONFIG_EMG_POSTEXPERIMENT)
	energy_markers_advertising(false);
	energy_markers_connected(true);
#endif
	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	printk("Connected: %s\n", addr);

	k_work_cancel_delayable(&adv_restart_work);
	custom_gatt_connected(conn);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	printk("Disconnected: %s reason=0x%02x\n", addr, reason);

#if defined(CONFIG_EMG_POSTEXPERIMENT)
	energy_markers_all_off();
#endif
	custom_gatt_disconnected(conn);
	k_work_reschedule(&adv_restart_work, K_MSEC(ADV_RESTART_DELAY_MS));
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

static void wait_for_serial_terminal(void)
{
	const struct device *const console = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
	uint32_t dtr = 0;
	int err;
	int retries = 0;

	if (!device_is_ready(console)) {
		return;
	}

	while (retries < 50) {
		err = uart_line_ctrl_get(console, UART_LINE_CTRL_DTR, &dtr);
		if ((err == 0) && dtr) {
			break;
		}

		k_sleep(K_MSEC(100));
		retries++;
	}

	k_sleep(K_MSEC(100));
}

int main(void)
{
	int err;

	wait_for_serial_terminal();
	printk("Booting EMG experiment peripheral\n");

#if defined(CONFIG_EMG_POSTEXPERIMENT)
	err = energy_markers_init();
	if (err) {
		printk("Energy marker init failed: %d\n", err);
		return 0;
	}
#endif

	err = sync_capture_init();
	if (err) {
		printk("Synchronization input init failed: %d\n", err);
		return 0;
	}

	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed: %d\n", err);
		return 0;
	}

	printk("Bluetooth initialized\n");
	printk("Custom payload max length: %d bytes\n", CUSTOM_GATT_MAX_PAYLOAD_LEN);
	experiment_start();

	k_work_reschedule(&adv_restart_work, K_NO_WAIT);

	while (true) {
		k_sleep(K_SECONDS(1));
	}
}

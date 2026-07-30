#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/bluetooth/addr.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/bluetooth/hci_types.h>
#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net_buf.h>
#include <zephyr/sys/printk.h>

#define BLE_SURVEY_DURATION_SECONDS 60U
#define WIFI_SURVEY_DURATION_SECONDS 60U
#define SURVEY_START_DELAY_SECONDS 5U
#define BLE_SURVEY_MAX_DEVICES 128U
#define WIFI_SURVEY_MAX_NETWORKS 64U
#define SURVEY_NAME_MAX 63U
#define WIFI_SCAN_TIMEOUT_SECONDS 20U
#define WIFI_SCAN_GAP_MS 1000U

struct observed_device {
	bt_addr_le_t address;
	char name[SURVEY_NAME_MAX + 1U];
	uint32_t observations;
	int32_t rssi_sum;
	int8_t rssi_min;
	int8_t rssi_max;
	uint32_t first_seen_ms;
	uint32_t last_seen_ms;
	uint32_t advertising_type_mask;
	bool name_complete;
};

struct parsed_name {
	char value[SURVEY_NAME_MAX + 1U];
	bool present;
	bool complete;
};

struct observed_wifi {
	uint8_t bssid[WIFI_MAC_ADDR_LEN];
	char ssid[WIFI_SSID_MAX_LEN + 1U];
	uint32_t observations;
	int32_t rssi_sum;
	int8_t rssi_min;
	int8_t rssi_max;
	uint32_t first_seen_ms;
	uint32_t last_seen_ms;
	uint8_t band;
	uint8_t channel;
	enum wifi_security_type security;
};

static struct observed_device devices[BLE_SURVEY_MAX_DEVICES];
static size_t device_count;
static uint32_t untracked_reports;
static int64_t scan_start_ms;
static struct observed_wifi wifi_networks[WIFI_SURVEY_MAX_NETWORKS];
static size_t wifi_network_count;
static uint32_t wifi_untracked_results;
static uint32_t wifi_scan_count;
static int64_t wifi_scan_start_ms;
static struct net_mgmt_event_callback wifi_callback;
static K_SEM_DEFINE(wifi_scan_done, 0, 1);

static const struct bt_le_scan_param scan_parameters = {
	.type = BT_LE_SCAN_TYPE_ACTIVE,
	.options = BT_LE_SCAN_OPT_NONE,
	.interval = BT_GAP_SCAN_FAST_INTERVAL,
	.window = BT_GAP_SCAN_FAST_WINDOW,
	.timeout = 0U,
};

static const struct wifi_scan_params wifi_scan_parameters = {
	.scan_type = WIFI_SCAN_TYPE_ACTIVE,
	.bands = BIT(WIFI_FREQ_BAND_2_4_GHZ),
};

static const char *address_type_name(uint8_t type)
{
	switch (type) {
	case BT_ADDR_LE_PUBLIC:
		return "public";
	case BT_ADDR_LE_RANDOM:
		return "random";
	case BT_ADDR_LE_PUBLIC_ID:
		return "public_identity";
	case BT_ADDR_LE_RANDOM_ID:
		return "random_identity";
	default:
		return "unknown";
	}
}

static bool parse_name_cb(struct bt_data *data, void *user_data)
{
	struct parsed_name *parsed = user_data;
	size_t length;

	if ((data->type != BT_DATA_NAME_COMPLETE) &&
	    (data->type != BT_DATA_NAME_SHORTENED)) {
		return true;
	}

	if (parsed->complete && (data->type == BT_DATA_NAME_SHORTENED)) {
		return true;
	}

	length = MIN((size_t)data->data_len, (size_t)SURVEY_NAME_MAX);
	for (size_t index = 0U; index < length; index++) {
		uint8_t character = data->data[index];

		/* Mantiene cada registro en una sola fila CSV sin depender de UTF-8. */
		if ((character < 0x20U) || (character > 0x7eU)) {
			parsed->value[index] = '?';
		} else if (character == ',') {
			parsed->value[index] = ';';
		} else {
			parsed->value[index] = (char)character;
		}
	}
	parsed->value[length] = '\0';
	parsed->present = true;
	parsed->complete = data->type == BT_DATA_NAME_COMPLETE;
	return !parsed->complete;
}

static void sanitize_bytes(char *destination, size_t destination_size,
			   const uint8_t *source, size_t source_size)
{
	size_t length = MIN(source_size, destination_size - 1U);

	for (size_t index = 0U; index < length; index++) {
		uint8_t character = source[index];

		if ((character < 0x20U) || (character > 0x7eU)) {
			destination[index] = '?';
		} else if (character == ',') {
			destination[index] = ';';
		} else {
			destination[index] = (char)character;
		}
	}
	destination[length] = '\0';
}

static struct observed_device *find_device(const bt_addr_le_t *address)
{
	for (size_t index = 0U; index < device_count; index++) {
		if (bt_addr_le_cmp(&devices[index].address, address) == 0) {
			return &devices[index];
		}
	}

	return NULL;
}

static struct observed_device *create_device(const bt_addr_le_t *address,
					      int8_t rssi,
					      uint32_t elapsed_ms)
{
	struct observed_device *device;

	if (device_count >= ARRAY_SIZE(devices)) {
		untracked_reports++;
		return NULL;
	}

	device = &devices[device_count++];
	memset(device, 0, sizeof(*device));
	bt_addr_le_copy(&device->address, address);
	device->rssi_min = rssi;
	device->rssi_max = rssi;
	device->first_seen_ms = elapsed_ms;
	device->last_seen_ms = elapsed_ms;
	return device;
}

static void device_found(const bt_addr_le_t *address, int8_t rssi, uint8_t type,
			 struct net_buf_simple *advertising_data)
{
	struct parsed_name parsed = {0};
	struct observed_device *device;
	uint32_t elapsed_ms;

	if (rssi == BT_HCI_LE_RSSI_NOT_AVAILABLE) {
		return;
	}

	elapsed_ms = (uint32_t)(k_uptime_get() - scan_start_ms);
	device = find_device(address);
	if (device == NULL) {
		device = create_device(address, rssi, elapsed_ms);
		if (device == NULL) {
			return;
		}
	}

	bt_data_parse(advertising_data, parse_name_cb, &parsed);
	if (parsed.present && (!device->name_complete || parsed.complete)) {
		memcpy(device->name, parsed.value, sizeof(device->name));
		device->name_complete = parsed.complete;
	}

	device->observations++;
	device->rssi_sum += rssi;
	device->rssi_min = MIN(device->rssi_min, rssi);
	device->rssi_max = MAX(device->rssi_max, rssi);
	device->last_seen_ms = elapsed_ms;
	if (type < 32U) {
		device->advertising_type_mask |= BIT(type);
	}
}

static void print_mean_rssi(const struct observed_device *device)
{
	int32_t mean_x100 =
		(device->rssi_sum * 100) / (int32_t)device->observations;
	uint32_t magnitude =
		mean_x100 < 0 ? (uint32_t)(-mean_x100) : (uint32_t)mean_x100;

	if (mean_x100 < 0) {
		printk("-%u.%02u", magnitude / 100U, magnitude % 100U);
	} else {
		printk("%u.%02u", magnitude / 100U, magnitude % 100U);
	}
}

static size_t strongest_unprinted(const bool *printed)
{
	size_t strongest = ARRAY_SIZE(devices);

	for (size_t index = 0U; index < device_count; index++) {
		if (printed[index]) {
			continue;
		}
		if ((strongest == ARRAY_SIZE(devices)) ||
		    (devices[index].rssi_max > devices[strongest].rssi_max)) {
			strongest = index;
		}
	}

	return strongest;
}

static void print_results(void)
{
	bool printed[BLE_SURVEY_MAX_DEVICES] = {0};

	printk("CSV_BLE,event,address,address_type,name,observations,"
	       "rssi_min_dbm,rssi_mean_dbm,rssi_max_dbm,"
	       "first_seen_ms,last_seen_ms,advertising_type_mask\n");

	for (size_t row = 0U; row < device_count; row++) {
		size_t index = strongest_unprinted(printed);
		struct observed_device *device;
		char address[BT_ADDR_STR_LEN];

		if (index == ARRAY_SIZE(devices)) {
			break;
		}

		printed[index] = true;
		device = &devices[index];
		bt_addr_to_str(&device->address.a, address, sizeof(address));

		printk("BLE,%s,%s,%s,%u,%d,",
		       address,
		       address_type_name(device->address.type),
		       device->name[0] != '\0' ? device->name : "<unnamed>",
		       device->observations,
		       device->rssi_min);
		print_mean_rssi(device);
		printk(",%d,%u,%u,0x%08x\n",
		       device->rssi_max,
		       device->first_seen_ms,
		       device->last_seen_ms,
		       device->advertising_type_mask);
	}

	printk("SUMMARY_BLE,unique_devices,%u\n", (uint32_t)device_count);
	printk("SUMMARY_BLE,untracked_reports,%u\n", untracked_reports);
}

static struct observed_wifi *find_wifi_network(const uint8_t *bssid)
{
	for (size_t index = 0U; index < wifi_network_count; index++) {
		if (memcmp(wifi_networks[index].bssid, bssid,
			   WIFI_MAC_ADDR_LEN) == 0) {
			return &wifi_networks[index];
		}
	}

	return NULL;
}

static void process_wifi_result(const struct wifi_scan_result *result)
{
	struct observed_wifi *network;
	uint32_t elapsed_ms =
		(uint32_t)(k_uptime_get() - wifi_scan_start_ms);

	network = find_wifi_network(result->mac);
	if (network == NULL) {
		if (wifi_network_count >= ARRAY_SIZE(wifi_networks)) {
			wifi_untracked_results++;
			return;
		}

		network = &wifi_networks[wifi_network_count++];
		memset(network, 0, sizeof(*network));
		memcpy(network->bssid, result->mac, WIFI_MAC_ADDR_LEN);
		network->rssi_min = result->rssi;
		network->rssi_max = result->rssi;
		network->first_seen_ms = elapsed_ms;
	}

	sanitize_bytes(network->ssid, sizeof(network->ssid),
		       result->ssid, result->ssid_length);
	network->observations++;
	network->rssi_sum += result->rssi;
	network->rssi_min = MIN(network->rssi_min, result->rssi);
	network->rssi_max = MAX(network->rssi_max, result->rssi);
	network->last_seen_ms = elapsed_ms;
	network->band = result->band;
	network->channel = result->channel;
	network->security = result->security;
}

static void wifi_event_handler(struct net_mgmt_event_callback *callback,
			       uint64_t event, struct net_if *interface)
{
	ARG_UNUSED(interface);

	if (event == NET_EVENT_WIFI_SCAN_RESULT) {
		process_wifi_result(
			(const struct wifi_scan_result *)callback->info);
	} else if (event == NET_EVENT_WIFI_SCAN_DONE) {
		k_sem_give(&wifi_scan_done);
	}
}

static int run_wifi_survey(void)
{
	struct net_if *interface = net_if_get_default();
	int64_t deadline;

	if (interface == NULL) {
		return -ENODEV;
	}

	net_mgmt_init_event_callback(
		&wifi_callback, wifi_event_handler,
		NET_EVENT_WIFI_SCAN_RESULT | NET_EVENT_WIFI_SCAN_DONE);
	net_mgmt_add_event_callback(&wifi_callback);

	wifi_scan_start_ms = k_uptime_get();
	deadline = wifi_scan_start_ms +
		   ((int64_t)WIFI_SURVEY_DURATION_SECONDS * MSEC_PER_SEC);

	while (k_uptime_get() < deadline) {
		int err;

		k_sem_reset(&wifi_scan_done);
		err = net_mgmt(NET_REQUEST_WIFI_SCAN, interface,
			       (void *)&wifi_scan_parameters,
			       sizeof(wifi_scan_parameters));
		if (err != 0) {
			net_mgmt_del_event_callback(&wifi_callback);
			return err;
		}

		err = k_sem_take(&wifi_scan_done,
				 K_SECONDS(WIFI_SCAN_TIMEOUT_SECONDS));
		if (err != 0) {
			net_mgmt_del_event_callback(&wifi_callback);
			return err;
		}

		wifi_scan_count++;
		if (k_uptime_get() < deadline) {
			k_sleep(K_MSEC(WIFI_SCAN_GAP_MS));
		}
	}

	net_mgmt_del_event_callback(&wifi_callback);
	return 0;
}

static size_t strongest_wifi_unprinted(const bool *printed)
{
	size_t strongest = ARRAY_SIZE(wifi_networks);

	for (size_t index = 0U; index < wifi_network_count; index++) {
		if (printed[index]) {
			continue;
		}
		if ((strongest == ARRAY_SIZE(wifi_networks)) ||
		    (wifi_networks[index].rssi_max >
		     wifi_networks[strongest].rssi_max)) {
			strongest = index;
		}
	}

	return strongest;
}

static void print_wifi_mean_rssi(const struct observed_wifi *network)
{
	int32_t mean_x100 =
		(network->rssi_sum * 100) / (int32_t)network->observations;
	uint32_t magnitude =
		mean_x100 < 0 ? (uint32_t)(-mean_x100) : (uint32_t)mean_x100;

	printk("%s%u.%02u",
	       mean_x100 < 0 ? "-" : "",
	       magnitude / 100U,
	       magnitude % 100U);
}

static void print_wifi_results(void)
{
	bool printed[WIFI_SURVEY_MAX_NETWORKS] = {0};

	printk("CSV_WIFI,event,bssid,ssid,band,channel,security,observations,"
	       "rssi_min_dbm,rssi_mean_dbm,rssi_max_dbm,"
	       "first_seen_ms,last_seen_ms\n");

	for (size_t row = 0U; row < wifi_network_count; row++) {
		size_t index = strongest_wifi_unprinted(printed);
		struct observed_wifi *network;

		if (index == ARRAY_SIZE(wifi_networks)) {
			break;
		}

		printed[index] = true;
		network = &wifi_networks[index];
		printk("WIFI,%02x:%02x:%02x:%02x:%02x:%02x,%s,%s,%u,%s,%u,%d,",
		       network->bssid[0], network->bssid[1],
		       network->bssid[2], network->bssid[3],
		       network->bssid[4], network->bssid[5],
		       network->ssid[0] != '\0' ? network->ssid : "<hidden>",
		       wifi_band_txt(network->band),
		       network->channel,
		       wifi_security_txt(network->security),
		       network->observations,
		       network->rssi_min);
		print_wifi_mean_rssi(network);
		printk(",%d,%u,%u\n",
		       network->rssi_max,
		       network->first_seen_ms,
		       network->last_seen_ms);
	}

	printk("SUMMARY_WIFI,unique_networks,%u\n",
	       (uint32_t)wifi_network_count);
	printk("SUMMARY_WIFI,completed_scans,%u\n", wifi_scan_count);
	printk("SUMMARY_WIFI,untracked_results,%u\n",
	       wifi_untracked_results);
}

int main(void)
{
	int err;

	printk("Booting central BLE and Wi-Fi environment survey\n");
	printk("PROFILE,radio_environment_scan,1\n");
	printk("SETTING,ble_duration_s,%u\n", BLE_SURVEY_DURATION_SECONDS);
	printk("SETTING,wifi_duration_s,%u\n", WIFI_SURVEY_DURATION_SECONDS);
	printk("SETTING,start_delay_s,%u\n", SURVEY_START_DELAY_SECONDS);
	printk("SETTING,scan_type,active\n");
	printk("SETTING,scan_interval_units,%u\n", scan_parameters.interval);
	printk("SETTING,scan_window_units,%u\n", scan_parameters.window);
	printk("SETTING,ble_scan_interval_ms,%u\n",
	       (scan_parameters.interval * 625U) / 1000U);
	printk("SETTING,ble_scan_window_ms,%u\n",
	       (scan_parameters.window * 625U) / 1000U);
	printk("SETTING,duplicate_filter,disabled\n");
	printk("SETTING,wifi_scan_type,active\n");
	printk("SETTING,wifi_band,2.4GHz\n");
	printk("SETTING,ble_max_unique_devices,%u\n",
	       BLE_SURVEY_MAX_DEVICES);
	printk("SETTING,wifi_max_unique_networks,%u\n",
	       WIFI_SURVEY_MAX_NETWORKS);

	err = bt_enable(NULL);
	if (err != 0) {
		printk("ERROR,bluetooth_init,%d\n", err);
		printk("Session complete\n");
		return 0;
	}

	printk("Bluetooth initialized\n");
	printk("BLE survey starts in %u seconds\n",
	       SURVEY_START_DELAY_SECONDS);
	k_sleep(K_SECONDS(SURVEY_START_DELAY_SECONDS));

	scan_start_ms = k_uptime_get();
	err = bt_le_scan_start(&scan_parameters, device_found);
	if (err != 0) {
		printk("ERROR,scan_start,%d\n", err);
		printk("Session complete\n");
		return 0;
	}

	printk("BLE survey started\n");
	k_sleep(K_SECONDS(BLE_SURVEY_DURATION_SECONDS));

	err = bt_le_scan_stop();
	if (err != 0) {
		printk("ERROR,scan_stop,%d\n", err);
	}

	printk("BLE survey stopped\n");
	err = bt_disable();
	if (err != 0) {
		printk("WARNING,bluetooth_disable,%d\n", err);
	}

	printk("Wi-Fi survey started\n");
	err = run_wifi_survey();
	if (err != 0) {
		printk("ERROR,wifi_scan,%d\n", err);
	} else {
		printk("Wi-Fi survey stopped\n");
	}

	print_results();
	print_wifi_results();
	printk("Session complete\n");
	return 0;
}

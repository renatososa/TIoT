#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "custom_gatt.h"
#include "emg_protocol.h"
#include "experiment.h"
#include "sync_capture.h"

#define SYNC_NODE DT_PATH(zephyr_user)

BUILD_ASSERT(DT_NODE_HAS_STATUS(SYNC_NODE, okay),
	     "The sync-signal devicetree alias is required");

static const struct gpio_dt_spec sync_gpio =
	GPIO_DT_SPEC_GET(SYNC_NODE, sync_gpios);
static struct gpio_callback sync_gpio_cb;
static struct k_spinlock sync_lock;
static uint64_t captured_time_us[EMG_SYNC_RECORD_COUNT];
static uint8_t captured_per_group[EMG_SYNC_GROUP_COUNT];
static bool capture_armed;
static uint8_t armed_group;

static K_SEM_DEFINE(report_sync_sem, 0, 1);

static uint64_t local_time_us(void)
{
	return k_ticks_to_us_floor64(k_uptime_ticks());
}

static void sync_edge_cb(const struct device *port,
			 struct gpio_callback *cb, gpio_port_pins_t pins)
{
	k_spinlock_key_t key;
	uint8_t group_index;
	uint8_t pulse_id;

	ARG_UNUSED(port);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);

	key = k_spin_lock(&sync_lock);
	if (!capture_armed ||
	    (captured_per_group[armed_group] >= EMG_SYNC_PULSES_PER_GROUP)) {
		k_spin_unlock(&sync_lock, key);
		return;
	}

	group_index = captured_per_group[armed_group]++;
	pulse_id = (armed_group * EMG_SYNC_PULSES_PER_GROUP) + group_index;
	captured_time_us[pulse_id] = local_time_us();
	k_spin_unlock(&sync_lock, key);
}

static void arm_group(uint8_t group)
{
	k_spinlock_key_t key = k_spin_lock(&sync_lock);

	armed_group = group;
	captured_per_group[group] = 0U;
	capture_armed = true;
	k_spin_unlock(&sync_lock, key);
}

static void disarm_capture(void)
{
	k_spinlock_key_t key = k_spin_lock(&sync_lock);

	capture_armed = false;
	k_spin_unlock(&sync_lock, key);
}

static void report_sync_thread(void *p1, void *p2, void *p3)
{
	uint8_t encoded[EMG_STATS_RECORD_SIZE];
	uint8_t count[EMG_SYNC_GROUP_COUNT];

	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (true) {
		k_sem_take(&report_sync_sem, K_FOREVER);

		for (uint8_t group = 0U; group < EMG_SYNC_GROUP_COUNT; group++) {
			k_spinlock_key_t key = k_spin_lock(&sync_lock);

			count[group] = captured_per_group[group];
			k_spin_unlock(&sync_lock, key);
		}

		printk("Sync report initial=%u final=%u\n", count[0], count[1]);

		for (uint8_t index = 0U; index < experiment_stats_count(); index++) {
			struct emg_experiment_stats stats;
			int err;

			experiment_get_stats(index, &stats);
			emg_stats_record_encode(encoded, &stats);
			err = custom_gatt_send(encoded, EMG_STATS_RECORD_SIZE);
			if (err) {
				printk("Experiment stats send failed index=%u err=%d\n",
				       index, err);
				continue;
			}
		}

		for (uint8_t group = 0U; group < EMG_SYNC_GROUP_COUNT; group++) {
			for (uint8_t index = 0U;
			     index < EMG_SYNC_PULSES_PER_GROUP; index++) {
				const uint16_t pulse_id =
					(group * EMG_SYNC_PULSES_PER_GROUP) + index;
				const struct emg_sync_record record = {
					.pulse_id = pulse_id,
					.group = group,
					.local_time_us =
						captured_time_us[pulse_id],
				};
				int err;

				emg_sync_record_encode(encoded, &record);
				err = custom_gatt_send(encoded,
						       EMG_SYNC_RECORD_SIZE);
				if (err) {
					printk("Sync report send failed id=%u err=%d\n",
					       pulse_id, err);
					break;
				}
			}
		}
	}
}

K_THREAD_DEFINE(sync_report_thread_id, 2048, report_sync_thread,
		NULL, NULL, NULL, 6, 0, 0);

int sync_capture_handle_command(uint8_t command)
{
	switch (command) {
	case EMG_CONTROL_PREPARE:
		experiment_stop_data();
		disarm_capture();
		memset(captured_time_us, 0, sizeof(captured_time_us));
		memset(captured_per_group, 0, sizeof(captured_per_group));
		arm_group(EMG_SYNC_GROUP_INITIAL);
		printk("Initial synchronization armed\n");
		return 0;

	case EMG_CONTROL_START_DATA:
		disarm_capture();
		if (captured_per_group[EMG_SYNC_GROUP_INITIAL] !=
		    EMG_SYNC_PULSES_PER_GROUP) {
			printk("Initial synchronization incomplete: %u\n",
			       captured_per_group[EMG_SYNC_GROUP_INITIAL]);
		}
		experiment_start_data();
		return 0;

	case EMG_CONTROL_STOP_DATA:
		experiment_stop_data();
		arm_group(EMG_SYNC_GROUP_FINAL);
		return 0;

	case EMG_CONTROL_REPORT_SYNC:
		disarm_capture();
		k_sem_give(&report_sync_sem);
		return 0;

	default:
		return -EINVAL;
	}
}

int sync_capture_init(void)
{
	int err;

	if (!gpio_is_ready_dt(&sync_gpio)) {
		return -ENODEV;
	}

	err = gpio_pin_configure_dt(&sync_gpio, GPIO_INPUT | GPIO_PULL_DOWN);
	if (err) {
		return err;
	}

	gpio_init_callback(&sync_gpio_cb, sync_edge_cb, BIT(sync_gpio.pin));
	err = gpio_add_callback(sync_gpio.port, &sync_gpio_cb);
	if (err) {
		return err;
	}

	err = gpio_pin_interrupt_configure_dt(&sync_gpio, GPIO_INT_EDGE_RISING);
	if (err) {
		return err;
	}

	printk("Sync input ready: XIAO D2 / P0.28\n");
	return 0;
}

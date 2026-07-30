#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/printk.h>

#include "custom_client.h"
#include "emg_protocol.h"
#include "experiment_receiver.h"
#include "sync_controller.h"

#define SYNC_NODE DT_PATH(zephyr_user)
#if defined(CONFIG_EMG_PREEXPERIMENT)
#define EXPERIMENT_DURATION_MS (EMG_PREEXPERIMENT_TOTAL_SECONDS * 1000U)
#define LINK_SETTLE_SECONDS 35U
#elif defined(CONFIG_EMG_POSTEXPERIMENT)
#define EXPERIMENT_DURATION_MS \
	(EMG_POSTEXPERIMENT_DATA_MS + EMG_POSTEXPERIMENT_GUARD_MS)
#define LINK_SETTLE_SECONDS 5U
#else
#define EXPERIMENT_DURATION_MS 60000U
#endif
#define SYNC_COMMAND_SETTLE_MS 250U
#define SYNC_REPORT_TIMEOUT_MS 5000U
#define SYNC_PULSE_PERIOD_MS 100U
#define SYNC_PULSE_WIDTH_MS 10U

BUILD_ASSERT(DT_NODE_HAS_STATUS(SYNC_NODE, okay),
	     "The sync-signal devicetree alias is required");

static const struct gpio_dt_spec sync_gpio =
	GPIO_DT_SPEC_GET(SYNC_NODE, sync_gpios);
static uint64_t central_pulse_time_us[EMG_SYNC_RECORD_COUNT];
static uint64_t peripheral_pulse_time_us[EMG_SYNC_RECORD_COUNT];
static struct emg_experiment_stats
	experiment_stats[EMG_EXPERIMENT_STATS_MAX];
static uint8_t experiment_stats_count;
static uint32_t received_sync_mask;
static atomic_t results_scheduled;
static atomic_t session_generation;
static K_SEM_DEFINE(session_ready_sem, 0, 1);
static K_SEM_DEFINE(results_ready_sem, 0, 1);

static uint64_t local_time_us(void)
{
	return k_ticks_to_us_floor64(k_uptime_ticks());
}

static bool session_is_current(atomic_val_t generation)
{
	return atomic_get(&session_generation) == generation;
}

static void schedule_results_dump(void)
{
	if (atomic_cas(&results_scheduled, 0, 1)) {
		k_sem_give(&results_ready_sem);
	}
}

static bool sleep_checked(uint32_t duration_ms, atomic_val_t generation)
{
	uint32_t elapsed_ms = 0U;

	while (elapsed_ms < duration_ms) {
		const uint32_t interval_ms = MIN(100U, duration_ms - elapsed_ms);

		k_sleep(K_MSEC(interval_ms));
		elapsed_ms += interval_ms;
		if (!session_is_current(generation)) {
			return false;
		}
	}

	return true;
}

static int send_control_checked(uint8_t command, atomic_val_t generation)
{
	int err;

	if (!session_is_current(generation)) {
		return -ENOTCONN;
	}

	err = custom_client_send_control(command);
	if (err) {
		printk("Control command %u failed: %d\n", command, err);
	}
	return err;
}

static bool generate_pulse_group(uint8_t group, atomic_val_t generation)
{
	const uint16_t first_id = group * EMG_SYNC_PULSES_PER_GROUP;

	for (uint8_t index = 0U; index < EMG_SYNC_PULSES_PER_GROUP; index++) {
		const uint16_t pulse_id = first_id + index;

		if (!session_is_current(generation)) {
			gpio_pin_set_dt(&sync_gpio, 0);
			return false;
		}

		gpio_pin_set_dt(&sync_gpio, 1);
		central_pulse_time_us[pulse_id] = local_time_us();
		k_sleep(K_MSEC(SYNC_PULSE_WIDTH_MS));
		gpio_pin_set_dt(&sync_gpio, 0);

		if (!sleep_checked(SYNC_PULSE_PERIOD_MS - SYNC_PULSE_WIDTH_MS,
				   generation)) {
			return false;
		}
	}

	printk("Sync pulse group %u generated\n", group);
	return true;
}

static uint64_t group_average(const uint64_t *values, uint8_t group)
{
	uint64_t sum = 0U;
	const uint16_t first_id = group * EMG_SYNC_PULSES_PER_GROUP;

	for (uint8_t index = 0U; index < EMG_SYNC_PULSES_PER_GROUP; index++) {
		sum += values[first_id + index];
	}

	return sum / EMG_SYNC_PULSES_PER_GROUP;
}

static void report_calibration(void)
{
	const uint32_t expected_mask = BIT(EMG_SYNC_RECORD_COUNT) - 1U;
	uint64_t peripheral_start;
	uint64_t central_start;
	uint64_t peripheral_end;
	uint64_t central_end;
	uint64_t scale_q32;

	if (experiment_stats_count == 0U) {
		printk("E,experiment_stats_missing\n");
	} else {
		for (uint8_t index = 0U; index < experiment_stats_count;
		     index++) {
			const struct emg_experiment_stats *stats =
				&experiment_stats[index];

			printk("P,0x%02x,%u,%u,%u,%u,%u,%u,%u\n",
			       stats->config_id,
			       stats->generated_packets,
			       stats->enqueued_packets,
			       stats->transmitted_packets,
			       stats->queue_dropped_packets,
			       stats->stop_dropped_packets,
			       stats->send_error_packets,
			       stats->queue_high_watermark);
		}
	}

	if (received_sync_mask != expected_mask) {
		printk("E,sync_incomplete,0x%08x,0x%08x\n",
		       received_sync_mask, expected_mask);
		goto dump_data;
	}

	for (uint16_t pulse_id = 0U; pulse_id < EMG_SYNC_RECORD_COUNT;
	     pulse_id++) {
		printk("S,%u,%u,%llu,%llu\n", pulse_id,
		       pulse_id / EMG_SYNC_PULSES_PER_GROUP,
		       central_pulse_time_us[pulse_id],
		       peripheral_pulse_time_us[pulse_id]);

		if ((central_pulse_time_us[pulse_id] == 0U) ||
		    (peripheral_pulse_time_us[pulse_id] == 0U)) {
			printk("E,sync_missing_pulse,%u\n", pulse_id);
			goto dump_data;
		}
	}

	peripheral_start =
		group_average(peripheral_pulse_time_us, EMG_SYNC_GROUP_INITIAL);
	central_start =
		group_average(central_pulse_time_us, EMG_SYNC_GROUP_INITIAL);
	peripheral_end =
		group_average(peripheral_pulse_time_us, EMG_SYNC_GROUP_FINAL);
	central_end =
		group_average(central_pulse_time_us, EMG_SYNC_GROUP_FINAL);

	if (peripheral_end <= peripheral_start) {
		printk("E,sync_invalid_interval\n");
		goto dump_data;
	}

	scale_q32 = ((central_end - central_start) << 32) /
		    (peripheral_end - peripheral_start);

	printk("C,%llu,%llu,%llu,%llu,%llu\n",
	       peripheral_start, central_start, peripheral_end, central_end,
	       scale_q32);

dump_data:
	experiment_receiver_dump();
	printk("Session complete\n");
}

static void results_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (true) {
		k_sem_take(&results_ready_sem, K_FOREVER);
		report_calibration();
	}
}

static void session_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (true) {
		atomic_val_t generation;

		k_sem_take(&session_ready_sem, K_FOREVER);
		generation = atomic_get(&session_generation);
		memset(central_pulse_time_us, 0, sizeof(central_pulse_time_us));
		memset(peripheral_pulse_time_us, 0, sizeof(peripheral_pulse_time_us));
		memset(&experiment_stats, 0, sizeof(experiment_stats));
		experiment_stats_count = 0U;
		received_sync_mask = 0U;
		atomic_clear(&results_scheduled);

#if defined(CONFIG_EMG_PREEXPERIMENT) || defined(CONFIG_EMG_POSTEXPERIMENT)
		printk("Waiting %u s for BLE PHY, Data Length and connection parameters\n",
		       LINK_SETTLE_SECONDS);
		if (!sleep_checked(LINK_SETTLE_SECONDS * 1000U, generation)) {
			continue;
		}
		if (custom_client_print_link_state() != 0) {
			printk("E,link_state_unavailable\n");
			continue;
		}
#if defined(CONFIG_EMG_PREEXPERIMENT)
		printk("Session preparing: 10 initial pulses, saturation preexperiment, 10 final pulses\n");
#else
		printk("Session preparing: 10 initial pulses, 11 profiles x 20 s, 10 final pulses\n");
#endif
#else
		printk("Session preparing: 10 initial pulses, 60 s data, 10 final pulses\n");
#endif
		if (send_control_checked(EMG_CONTROL_PREPARE, generation) != 0 ||
		    !sleep_checked(SYNC_COMMAND_SETTLE_MS, generation) ||
		    !generate_pulse_group(EMG_SYNC_GROUP_INITIAL, generation)) {
			continue;
		}

		printk("Experiment started: duration=%u ms\n",
		       EXPERIMENT_DURATION_MS);
		if (send_control_checked(EMG_CONTROL_START_DATA, generation) != 0) {
			continue;
		}

		if (!sleep_checked(EXPERIMENT_DURATION_MS, generation) ||
		    send_control_checked(EMG_CONTROL_STOP_DATA, generation) != 0 ||
		    !sleep_checked(SYNC_COMMAND_SETTLE_MS, generation) ||
		    !generate_pulse_group(EMG_SYNC_GROUP_FINAL, generation) ||
		    send_control_checked(EMG_CONTROL_REPORT_SYNC, generation) != 0) {
			continue;
		}

		printk("Experiment stopped; waiting for synchronization records\n");
		if (sleep_checked(SYNC_REPORT_TIMEOUT_MS, generation) &&
		    !atomic_get(&results_scheduled)) {
			printk("E,sync_report_timeout\n");
			schedule_results_dump();
		}
	}
}

K_THREAD_DEFINE(sync_session_thread_id, 4096, session_thread,
		NULL, NULL, NULL, 6, 0, 0);
K_THREAD_DEFINE(results_dump_thread_id, 3072, results_thread,
		NULL, NULL, NULL, 7, 0, 0);

int sync_controller_init(void)
{
	int err;

	if (!gpio_is_ready_dt(&sync_gpio)) {
		return -ENODEV;
	}

	err = gpio_pin_configure_dt(&sync_gpio, GPIO_OUTPUT_INACTIVE);
	if (err) {
		return err;
	}

	printk("Sync output ready: ESP32-S3 GPIO4\n");
	return 0;
}

void sync_controller_session_ready(void)
{
	atomic_inc(&session_generation);
	k_sem_give(&session_ready_sem);
}

void sync_controller_disconnected(void)
{
	atomic_inc(&session_generation);
	gpio_pin_set_dt(&sync_gpio, 0);
}

bool sync_controller_process_notification(const uint8_t *data, uint16_t len)
{
	struct emg_experiment_stats stats;
	struct emg_sync_record record;

	if (emg_stats_record_decode(&stats, data, len) == 0) {
		if (experiment_stats_count < ARRAY_SIZE(experiment_stats)) {
			experiment_stats[experiment_stats_count++] = stats;
		} else {
			printk("E,experiment_stats_overflow,0x%02x\n",
			       stats.config_id);
		}
		return true;
	}

	if (emg_sync_record_decode(&record, data, len) != 0) {
		return false;
	}

	peripheral_pulse_time_us[record.pulse_id] = record.local_time_us;
	received_sync_mask |= BIT(record.pulse_id);

	if (received_sync_mask == (BIT(EMG_SYNC_RECORD_COUNT) - 1U)) {
		schedule_results_dump();
	}

	return true;
}

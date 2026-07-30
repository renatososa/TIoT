#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/printk.h>

#include "custom_gatt.h"
#include "emg_protocol.h"
#include "energy_markers.h"
#include "experiment.h"

#define EMG_PACKET_QUEUE_DEPTH 8U
#define TRANSMITTER_IDLE_TIMEOUT_MS 2000U

struct post_profile {
	const char *name;
	uint8_t config_id;
	uint16_t payload_len;
	uint16_t period_ms;
};

struct queued_packet {
	uint64_t acquisition_time_us;
	uint32_t packet_id;
	uint8_t profile_index;
	uint16_t payload_len;
	uint8_t payload[EMG_PACKET_DATA_MAX];
};

struct profile_counters {
	atomic_t generated_packets;
	atomic_t enqueued_packets;
	atomic_t transmitted_packets;
	atomic_t queue_dropped_packets;
	atomic_t stop_dropped_packets;
	atomic_t send_error_packets;
	atomic_t queue_high_watermark;
};

static const struct post_profile profiles[] = {
	{ "S1-E20", EMG_CONFIG_S1_E20, 60U, 20U },
	{ "S2-E5", EMG_CONFIG_S2_E5, 60U, 5U },
	{ "S2-E10", EMG_CONFIG_S2_E10, 120U, 10U },
	{ "S2-E20", EMG_CONFIG_S2_E20, 240U, 20U },
	{ "S3-E5", EMG_CONFIG_S3_E5, 240U, 5U },
	{ "S3-E10", EMG_CONFIG_S3_E10, 480U, 10U },
	{ "S3-E20", EMG_CONFIG_S3_E20, 960U, 20U },
	{ "P960-T10", EMG_CONFIG_STRESS_P960_T10, 960U, 10U },
	{ "P960-T8", EMG_CONFIG_STRESS_P960_T8, 960U, 8U },
	{ "P960-T7", EMG_CONFIG_STRESS_P960_T7, 960U, 7U },
	{ "P960-T6", EMG_CONFIG_STRESS_P960_T6, 960U, 6U },
};

BUILD_ASSERT(ARRAY_SIZE(profiles) == EMG_POSTEXPERIMENT_PROFILE_COUNT);

K_MSGQ_DEFINE(packet_queue, sizeof(struct queued_packet),
	      EMG_PACKET_QUEUE_DEPTH, 4);
static K_SEM_DEFINE(acquisition_tick_sem, 0, K_SEM_MAX_LIMIT);
static K_SEM_DEFINE(sequence_start_sem, 0, 1);

static struct profile_counters counters[EMG_POSTEXPERIMENT_PROFILE_COUNT];
static uint8_t notification[EMG_GATT_VALUE_MAX];
static uint64_t acquisition_epoch_us;
static uint32_t sample_word_index;
static atomic_t sequence_running;
static atomic_t profile_running;
static atomic_t transmitter_active;
static atomic_t current_profile_index;

static uint64_t local_time_us(void)
{
	return k_ticks_to_us_floor64(k_uptime_ticks());
}

static void acquisition_timer_handler(struct k_timer *timer)
{
	ARG_UNUSED(timer);

	if (atomic_get(&profile_running)) {
		k_sem_give(&acquisition_tick_sem);
	}
}

K_TIMER_DEFINE(acquisition_timer, acquisition_timer_handler, NULL);

static void reset_counter(struct profile_counters *counter)
{
	atomic_clear(&counter->generated_packets);
	atomic_clear(&counter->enqueued_packets);
	atomic_clear(&counter->transmitted_packets);
	atomic_clear(&counter->queue_dropped_packets);
	atomic_clear(&counter->stop_dropped_packets);
	atomic_clear(&counter->send_error_packets);
	atomic_clear(&counter->queue_high_watermark);
}

static void update_queue_high_watermark(struct profile_counters *counter,
					uint32_t used)
{
	atomic_val_t previous = atomic_get(&counter->queue_high_watermark);

	while ((used > (uint32_t)previous) &&
	       !atomic_cas(&counter->queue_high_watermark, previous, used)) {
		previous = atomic_get(&counter->queue_high_watermark);
	}
}

static void generate_payload(uint8_t *payload, uint16_t len, uint8_t config_id)
{
	for (uint16_t offset = 0U; offset < len; offset += 3U) {
		const uint32_t value =
			((sample_word_index++ & 0x00ffffU) << 8) |
			config_id;

		payload[offset] = (uint8_t)value;
		payload[offset + 1U] = (uint8_t)(value >> 8);
		payload[offset + 2U] = (uint8_t)(value >> 16);
	}
}

static int send_packet(const struct queued_packet *packet)
{
	const struct post_profile *profile = &profiles[packet->profile_index];
	const uint8_t fragment_count =
		(packet->payload_len + EMG_FRAGMENT_DATA_MAX - 1U) /
		EMG_FRAGMENT_DATA_MAX;
	const uint64_t tx_time_us = local_time_us();
	uint16_t offset = 0U;

	for (uint8_t fragment_index = 0U; fragment_index < fragment_count;
	     fragment_index++) {
		const uint16_t remaining = packet->payload_len - offset;
		const uint16_t fragment_len =
			MIN(remaining, EMG_FRAGMENT_DATA_MAX);
		const struct emg_fragment_header header = {
			.config_id = profile->config_id,
			.packet_id = packet->packet_id,
			.acquisition_time_us = packet->acquisition_time_us,
			.tx_time_us = tx_time_us,
			.packet_payload_len = packet->payload_len,
			.fragment_offset = offset,
			.fragment_index = fragment_index,
			.fragment_count = fragment_count,
			.fragment_payload_len = fragment_len,
		};
		int err;

		emg_header_encode(notification, &header);
		memcpy(&notification[EMG_HEADER_SIZE], &packet->payload[offset],
		       fragment_len);

		err = custom_gatt_send(notification,
				       EMG_HEADER_SIZE + fragment_len);
		if (err) {
			return err;
		}
		offset += fragment_len;
	}

	return 0;
}

static void acquisition_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (true) {
		struct queued_packet packet;
		struct profile_counters *counter;
		const struct post_profile *profile;
		uint8_t profile_index;
		uint32_t queue_used;

		k_sem_take(&acquisition_tick_sem, K_FOREVER);
		if (!atomic_get(&profile_running)) {
			continue;
		}

		profile_index = (uint8_t)atomic_get(&current_profile_index);
		if (profile_index >= ARRAY_SIZE(profiles)) {
			continue;
		}

		profile = &profiles[profile_index];
		counter = &counters[profile_index];
		packet.profile_index = profile_index;
		packet.payload_len = profile->payload_len;
		packet.packet_id =
			(uint32_t)atomic_inc(&counter->generated_packets);
		packet.acquisition_time_us =
			acquisition_epoch_us +
			((uint64_t)packet.packet_id * profile->period_ms * 1000U);

		energy_markers_acquisition(true);
		generate_payload(packet.payload, packet.payload_len,
				 profile->config_id);
		energy_markers_acquisition(false);

		if (!atomic_get(&profile_running) ||
		    ((uint8_t)atomic_get(&current_profile_index) != profile_index)) {
			atomic_inc(&counter->queue_dropped_packets);
			continue;
		}

		if (k_msgq_put(&packet_queue, &packet, K_NO_WAIT) != 0) {
			atomic_inc(&counter->queue_dropped_packets);
			continue;
		}

		atomic_inc(&counter->enqueued_packets);
		queue_used = k_msgq_num_used_get(&packet_queue);
		update_queue_high_watermark(counter, queue_used);
	}
}

static void transmitter_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (true) {
		struct queued_packet packet;
		struct profile_counters *counter;

		if (k_msgq_get(&packet_queue, &packet, K_FOREVER) != 0) {
			continue;
		}

		counter = &counters[packet.profile_index];
		if (!atomic_get(&profile_running) ||
		    ((uint8_t)atomic_get(&current_profile_index) !=
		     packet.profile_index)) {
			atomic_inc(&counter->stop_dropped_packets);
			continue;
		}

		atomic_set(&transmitter_active, 1);
		energy_markers_transmitter(true);
		if (send_packet(&packet) == 0) {
			atomic_inc(&counter->transmitted_packets);
		} else {
			atomic_inc(&counter->send_error_packets);
		}
		energy_markers_transmitter(false);
		atomic_clear(&transmitter_active);
	}
}

static bool sleep_sequence(uint32_t duration_ms)
{
	uint32_t elapsed_ms = 0U;

	while (elapsed_ms < duration_ms) {
		const uint32_t interval = MIN(50U, duration_ms - elapsed_ms);

		k_sleep(K_MSEC(interval));
		elapsed_ms += interval;
		if (!atomic_get(&sequence_running)) {
			return false;
		}
	}

	return true;
}

static void stop_current_profile(uint8_t profile_index)
{
	struct profile_counters *counter = &counters[profile_index];
	uint32_t pending;
	uint32_t waited_ms = 0U;

	atomic_clear(&profile_running);
	k_timer_stop(&acquisition_timer);
	k_sem_reset(&acquisition_tick_sem);
	energy_markers_acquisition(false);

	pending = k_msgq_num_used_get(&packet_queue);
	if (pending != 0U) {
		atomic_add(&counter->stop_dropped_packets, pending);
		k_msgq_purge(&packet_queue);
	}

	while (atomic_get(&transmitter_active) &&
	       (waited_ms < TRANSMITTER_IDLE_TIMEOUT_MS)) {
		k_sleep(K_MSEC(1));
		waited_ms++;
	}

	energy_markers_transmitter(false);
	energy_markers_profile(false);
}

static void sequence_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (true) {
		k_sem_take(&sequence_start_sem, K_FOREVER);

		for (uint8_t index = 0U;
		     index < ARRAY_SIZE(profiles) &&
		     atomic_get(&sequence_running);
		     index++) {
			const struct post_profile *profile = &profiles[index];

			k_msgq_purge(&packet_queue);
			k_sem_reset(&acquisition_tick_sem);
			atomic_set(&current_profile_index, index);
			acquisition_epoch_us = local_time_us();
			sample_word_index = 0U;

			printk("POST stage=%u/%u name=%s config=0x%02x payload=%u period=%ums duration=%us\n",
			       index + 1U, ARRAY_SIZE(profiles), profile->name,
			       profile->config_id, profile->payload_len,
			       profile->period_ms,
			       EMG_POSTEXPERIMENT_PROFILE_SECONDS);

			atomic_set(&profile_running, 1);
			energy_markers_profile(true);
			k_timer_start(&acquisition_timer,
				      K_MSEC(profile->period_ms),
				      K_MSEC(profile->period_ms));

			sleep_sequence(EMG_POSTEXPERIMENT_PROFILE_SECONDS * 1000U);
			stop_current_profile(index);

			printk("POST stage complete config=0x%02x generated=%u transmitted=%u queue_drop=%u stop_drop=%u\n",
			       profile->config_id,
			       (uint32_t)atomic_get(
				       &counters[index].generated_packets),
			       (uint32_t)atomic_get(
				       &counters[index].transmitted_packets),
			       (uint32_t)atomic_get(
				       &counters[index].queue_dropped_packets),
			       (uint32_t)atomic_get(
				       &counters[index].stop_dropped_packets));

			if ((index + 1U < ARRAY_SIZE(profiles)) &&
			    !sleep_sequence(EMG_POSTEXPERIMENT_GAP_MS)) {
				break;
			}
		}

		atomic_clear(&sequence_running);
		energy_markers_profile(false);
		energy_markers_acquisition(false);
		energy_markers_transmitter(false);
		printk("POST unified profile sequence complete\n");
	}
}

K_THREAD_DEFINE(acquisition_thread_id, 3072, acquisition_thread,
		NULL, NULL, NULL, 4, 0, 0);
K_THREAD_DEFINE(transmitter_thread_id, 4096, transmitter_thread,
		NULL, NULL, NULL, 5, 0, 0);
K_THREAD_DEFINE(sequence_thread_id, 3072, sequence_thread,
		NULL, NULL, NULL, 6, 0, 0);

int experiment_start(void)
{
	printk("Unified post-preexperiment: %u profiles, %u s each, %u ms gaps\n",
	       ARRAY_SIZE(profiles), EMG_POSTEXPERIMENT_PROFILE_SECONDS,
	       EMG_POSTEXPERIMENT_GAP_MS);
	return 0;
}

void experiment_start_data(void)
{
	experiment_stop_data();
	k_sem_reset(&sequence_start_sem);
	k_msgq_purge(&packet_queue);
	k_sem_reset(&acquisition_tick_sem);

	for (uint8_t index = 0U; index < ARRAY_SIZE(counters); index++) {
		reset_counter(&counters[index]);
	}

	atomic_clear(&transmitter_active);
	atomic_set(&sequence_running, 1);
	k_sem_give(&sequence_start_sem);
}

void experiment_stop_data(void)
{
	uint8_t profile_index =
		(uint8_t)atomic_get(&current_profile_index);

	atomic_clear(&sequence_running);
	if (atomic_get(&profile_running) &&
	    (profile_index < ARRAY_SIZE(profiles))) {
		stop_current_profile(profile_index);
	}
}

bool experiment_is_running(void)
{
	return atomic_get(&sequence_running) != 0;
}

uint8_t experiment_stats_count(void)
{
	return ARRAY_SIZE(profiles);
}

void experiment_get_stats(uint8_t index, struct emg_experiment_stats *stats)
{
	const struct profile_counters *counter;

	if (index >= ARRAY_SIZE(profiles)) {
		memset(stats, 0, sizeof(*stats));
		return;
	}

	counter = &counters[index];
	stats->config_id = profiles[index].config_id;
	stats->generated_packets = atomic_get(&counter->generated_packets);
	stats->enqueued_packets = atomic_get(&counter->enqueued_packets);
	stats->transmitted_packets = atomic_get(&counter->transmitted_packets);
	stats->queue_dropped_packets =
		atomic_get(&counter->queue_dropped_packets);
	stats->stop_dropped_packets =
		atomic_get(&counter->stop_dropped_packets);
	stats->send_error_packets =
		atomic_get(&counter->send_error_packets);
	stats->queue_high_watermark =
		atomic_get(&counter->queue_high_watermark);
}

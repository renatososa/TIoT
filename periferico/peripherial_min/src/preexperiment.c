#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/printk.h>

#include "custom_gatt.h"
#include "emg_protocol.h"
#include "experiment.h"

struct preexperiment_profile {
	uint8_t config_id;
	uint16_t payload_len;
};

struct preexperiment_packet {
	uint64_t acquisition_time_us;
	uint32_t packet_id;
	uint8_t config_id;
	uint16_t payload_len;
	uint8_t payload[EMG_PACKET_DATA_MAX];
};

static const struct preexperiment_profile profiles[] = {
	{ EMG_CONFIG_PRE_P15, 15U },
	{ EMG_CONFIG_PRE_P30, 30U },
	{ EMG_CONFIG_PRE_P60, 60U },
	{ EMG_CONFIG_PRE_P120, 120U },
	{ EMG_CONFIG_PRE_P240, 240U },
	{ EMG_CONFIG_PRE_P480, 480U },
	{ EMG_CONFIG_PRE_P960, 960U },
};

BUILD_ASSERT(ARRAY_SIZE(profiles) == EMG_PREEXPERIMENT_STAGE_COUNT);

static uint8_t notification[EMG_GATT_VALUE_MAX];
static atomic_t experiment_running;
static atomic_t generated_packets;
static atomic_t transmitted_packets;
static atomic_t send_error_packets;
static K_SEM_DEFINE(start_sem, 0, 1);

static uint64_t local_time_us(void)
{
	return k_ticks_to_us_floor64(k_uptime_ticks());
}

static void generate_payload(uint8_t *payload, uint16_t len,
			     uint32_t packet_id, uint8_t config_id)
{
	for (uint16_t index = 0U; index < len; index++) {
		payload[index] =
			(uint8_t)(packet_id + index + ((uint32_t)config_id << 1));
	}
}

static int send_packet(const struct preexperiment_packet *packet)
{
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
			.config_id = packet->config_id,
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

static bool wait_gap(void)
{
	uint32_t elapsed_ms = 0U;

	while (elapsed_ms < EMG_PREEXPERIMENT_GAP_MS) {
		const uint32_t interval =
			MIN(50U, EMG_PREEXPERIMENT_GAP_MS - elapsed_ms);

		k_sleep(K_MSEC(interval));
		elapsed_ms += interval;
		if (!atomic_get(&experiment_running)) {
			return false;
		}
	}

	return true;
}

static void preexperiment_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (true) {
		k_sem_take(&start_sem, K_FOREVER);

		for (uint8_t stage = 0U;
		     stage < ARRAY_SIZE(profiles) &&
		     atomic_get(&experiment_running);
		     stage++) {
			const struct preexperiment_profile *profile =
				&profiles[stage];
			const uint64_t stage_start_us = local_time_us();
			const uint64_t stage_duration_us =
				(uint64_t)EMG_PREEXPERIMENT_STAGE_SECONDS *
				1000000U;
			uint32_t packet_id = 0U;

			printk("PRE stage=%u/%u config=0x%02x payload=%u duration=%us\n",
			       stage + 1U, ARRAY_SIZE(profiles),
			       profile->config_id, profile->payload_len,
			       EMG_PREEXPERIMENT_STAGE_SECONDS);

			while (atomic_get(&experiment_running) &&
			       ((local_time_us() - stage_start_us) <
				stage_duration_us)) {
				struct preexperiment_packet packet = {
					.acquisition_time_us = local_time_us(),
					.packet_id = packet_id++,
					.config_id = profile->config_id,
					.payload_len = profile->payload_len,
				};

				generate_payload(packet.payload, packet.payload_len,
						 packet.packet_id,
						 packet.config_id);
				atomic_inc(&generated_packets);

				if (send_packet(&packet) == 0) {
					atomic_inc(&transmitted_packets);
				} else {
					atomic_inc(&send_error_packets);
					if (atomic_get(&experiment_running)) {
						k_sleep(K_MSEC(1));
					}
				}
			}

			printk("PRE stage complete config=0x%02x packets=%u\n",
			       profile->config_id, packet_id);

			if ((stage + 1U < ARRAY_SIZE(profiles)) &&
			    !wait_gap()) {
				break;
			}
		}

		atomic_clear(&experiment_running);
		printk("PRE saturation sequence complete\n");
	}
}

K_THREAD_DEFINE(preexperiment_thread_id, 4608, preexperiment_thread,
		NULL, NULL, NULL, 5, 0, 0);

int experiment_start(void)
{
	printk("BLE saturation preexperiment: 7 payloads, 10 s each, one notification in flight\n");
	return 0;
}

void experiment_start_data(void)
{
	atomic_clear(&experiment_running);
	k_sem_reset(&start_sem);
	atomic_clear(&generated_packets);
	atomic_clear(&transmitted_packets);
	atomic_clear(&send_error_packets);

	atomic_set(&experiment_running, 1);
	k_sem_give(&start_sem);
}

void experiment_stop_data(void)
{
	atomic_clear(&experiment_running);
}

bool experiment_is_running(void)
{
	return atomic_get(&experiment_running) != 0;
}

uint8_t experiment_stats_count(void)
{
	return 1U;
}

void experiment_get_stats(uint8_t index, struct emg_experiment_stats *stats)
{
	ARG_UNUSED(index);

	stats->config_id = EMG_CONFIG_PREEXPERIMENT;
	stats->generated_packets = atomic_get(&generated_packets);
	stats->enqueued_packets = stats->generated_packets;
	stats->transmitted_packets = atomic_get(&transmitted_packets);
	stats->queue_dropped_packets = 0U;
	stats->stop_dropped_packets = 0U;
	stats->send_error_packets = atomic_get(&send_error_packets);
	stats->queue_high_watermark = 0U;
}

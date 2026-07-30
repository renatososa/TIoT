#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "emg_protocol.h"
#include "experiment_receiver.h"

#define EXPERIMENT_MAX_RECORDS 50000U

struct experiment_record {
	uint64_t acquisition_time_us;
	uint64_t tx_time_us;
	uint64_t rx_time_us;
	uint32_t packet_id;
	uint16_t payload_bytes;
	uint8_t config_id;
	uint8_t fragments;
};

BUILD_ASSERT(sizeof(struct experiment_record) == 32U);

/*
 * The saturation preexperiment can exceed 20000 packets. Keep 50000 compact
 * records in external PSRAM so the radio measurement remains free of UART
 * output and all data can be dumped after the final synchronization.
 */
static struct experiment_record records[EXPERIMENT_MAX_RECORDS]
	__attribute__((section(".ext_ram.bss.experiment_records")));
static struct emg_fragment_header current_header;
static uint32_t last_completed_packet_id;
static uint8_t last_completed_config_id;
static uint32_t record_count;
static uint32_t incomplete_count;
static uint32_t lost_packet_count;
static uint32_t invalid_fragment_count;
static uint32_t inconsistent_header_count;
static uint32_t duplicate_fragment_count;
static uint32_t overflow_count;
static uint16_t received_bytes;
static uint8_t received_fragments;
static bool packet_active;
static bool have_last_completed;

static uint8_t expected_fragment_mask(uint8_t fragment_count)
{
	return (uint8_t)((1U << fragment_count) - 1U);
}

static void report_incomplete(void)
{
	if (packet_active) {
		incomplete_count++;
	}
}

static void begin_packet(const struct emg_fragment_header *header)
{
	if (have_last_completed &&
	    (header->config_id == last_completed_config_id) &&
	    (header->packet_id > (last_completed_packet_id + 1U))) {
		lost_packet_count +=
			header->packet_id - (last_completed_packet_id + 1U);
	}

	current_header = *header;
	received_bytes = 0U;
	received_fragments = 0U;
	packet_active = true;
}

void experiment_receiver_reset(void)
{
	memset(&current_header, 0, sizeof(current_header));
	received_bytes = 0U;
	received_fragments = 0U;
	packet_active = false;
	have_last_completed = false;
	last_completed_config_id = 0U;
	record_count = 0U;
	incomplete_count = 0U;
	lost_packet_count = 0U;
	invalid_fragment_count = 0U;
	inconsistent_header_count = 0U;
	duplicate_fragment_count = 0U;
	overflow_count = 0U;
}

void experiment_receiver_process(const uint8_t *data, uint16_t len)
{
	struct emg_fragment_header header;
	uint8_t fragment_bit;

	if (emg_header_decode(&header, data, len) != 0) {
		invalid_fragment_count++;
		return;
	}

	if (!packet_active || (header.packet_id != current_header.packet_id)) {
		report_incomplete();
		begin_packet(&header);
	}

	if ((header.config_id != current_header.config_id) ||
	    (header.acquisition_time_us != current_header.acquisition_time_us) ||
	    (header.tx_time_us != current_header.tx_time_us) ||
	    (header.packet_payload_len != current_header.packet_payload_len) ||
	    (header.fragment_count != current_header.fragment_count)) {
		inconsistent_header_count++;
		packet_active = false;
		return;
	}

	fragment_bit = (uint8_t)(1U << header.fragment_index);
	if ((received_fragments & fragment_bit) != 0U) {
		duplicate_fragment_count++;
		return;
	}

	received_bytes += header.fragment_payload_len;
	received_fragments |= fragment_bit;

	if ((received_bytes == current_header.packet_payload_len) &&
	    (received_fragments == expected_fragment_mask(current_header.fragment_count))) {
		uint64_t rx_time_us = k_ticks_to_us_floor64(k_uptime_ticks());

		if (record_count < ARRAY_SIZE(records)) {
			struct experiment_record *record = &records[record_count++];

			record->acquisition_time_us =
				current_header.acquisition_time_us;
			record->tx_time_us = current_header.tx_time_us;
			record->rx_time_us = rx_time_us;
			record->packet_id = current_header.packet_id;
			record->payload_bytes = current_header.packet_payload_len;
			record->config_id = current_header.config_id;
			record->fragments = current_header.fragment_count;
		} else {
			overflow_count++;
		}

		last_completed_packet_id = current_header.packet_id;
		last_completed_config_id = current_header.config_id;
		have_last_completed = true;
		packet_active = false;
	}
}

void experiment_receiver_dump(void)
{
	printk("CSV,event,config_id,packet_id,acquisition_time_us,tx_time_us,rx_time_us,payload_bytes,fragments\n");

	for (uint32_t index = 0U; index < record_count; index++) {
		const struct experiment_record *record = &records[index];

		printk("D,0x%02x,%u,%llu,%llu,%llu,%u,%u\n",
		       record->config_id, record->packet_id,
		       record->acquisition_time_us,
		       record->tx_time_us, record->rx_time_us,
		       record->payload_bytes, record->fragments);
	}

	printk("R,records,%u\n", record_count);
	printk("R,incomplete_packets,%u\n", incomplete_count);
	printk("R,lost_packets,%u\n", lost_packet_count);
	printk("R,invalid_fragments,%u\n", invalid_fragment_count);
	printk("R,inconsistent_headers,%u\n", inconsistent_header_count);
	printk("R,duplicate_fragments,%u\n", duplicate_fragment_count);
	printk("R,buffer_overflow,%u\n", overflow_count);
}

#ifndef EMG_PROTOCOL_H_
#define EMG_PROTOCOL_H_

#include <stdint.h>

#include <zephyr/sys/byteorder.h>

#define EMG_PROTOCOL_MAGIC 0x454dU
#define EMG_CONTROL_MAGIC 0x5443U
#define EMG_SYNC_MAGIC 0x5953U
#define EMG_STATS_MAGIC 0x5453U
#define EMG_PROTOCOL_VERSION 2U

#define EMG_ATT_MTU 247U
#define EMG_GATT_VALUE_MAX (EMG_ATT_MTU - 3U)
#define EMG_HEADER_SIZE 32U
#define EMG_FRAGMENT_DATA_MAX (EMG_GATT_VALUE_MAX - EMG_HEADER_SIZE)
#define EMG_PACKET_DATA_MAX 960U
#define EMG_CONTROL_SIZE 4U
#define EMG_SYNC_RECORD_SIZE 16U
#define EMG_STATS_RECORD_SIZE 32U
#define EMG_SYNC_PULSES_PER_GROUP 10U
#define EMG_SYNC_GROUP_COUNT 2U
#define EMG_SYNC_RECORD_COUNT \
	(EMG_SYNC_PULSES_PER_GROUP * EMG_SYNC_GROUP_COUNT)
#define EMG_PREEXPERIMENT_STAGE_COUNT 7U
#define EMG_PREEXPERIMENT_STAGE_SECONDS 10U
#define EMG_PREEXPERIMENT_GAP_MS 500U
#define EMG_PREEXPERIMENT_TOTAL_SECONDS 75U
#define EMG_POSTEXPERIMENT_PROFILE_COUNT 11U
#define EMG_POSTEXPERIMENT_PROFILE_SECONDS 20U
#define EMG_POSTEXPERIMENT_GAP_MS 1000U
#define EMG_POSTEXPERIMENT_DATA_MS \
	((EMG_POSTEXPERIMENT_PROFILE_COUNT * \
	  EMG_POSTEXPERIMENT_PROFILE_SECONDS * 1000U) + \
	 ((EMG_POSTEXPERIMENT_PROFILE_COUNT - 1U) * \
	  EMG_POSTEXPERIMENT_GAP_MS))
#define EMG_POSTEXPERIMENT_GUARD_MS 1000U
#define EMG_EXPERIMENT_STATS_MAX EMG_POSTEXPERIMENT_PROFILE_COUNT
#define EMG_FRAGMENT_COUNT_MAX \
	((EMG_PACKET_DATA_MAX + EMG_FRAGMENT_DATA_MAX - 1U) / EMG_FRAGMENT_DATA_MAX)

enum emg_config_id {
	EMG_CONFIG_S1_E5 = 0x11,
	EMG_CONFIG_S1_E10 = 0x12,
	EMG_CONFIG_S1_E20 = 0x13,
	EMG_CONFIG_S2_E5 = 0x21,
	EMG_CONFIG_S2_E10 = 0x22,
	EMG_CONFIG_S2_E20 = 0x23,
	EMG_CONFIG_S3_E5 = 0x31,
	EMG_CONFIG_S3_E10 = 0x32,
	EMG_CONFIG_S3_E20 = 0x33,
	EMG_CONFIG_PREEXPERIMENT = 0xa0,
	EMG_CONFIG_PRE_P15 = 0xa1,
	EMG_CONFIG_PRE_P30 = 0xa2,
	EMG_CONFIG_PRE_P60 = 0xa3,
	EMG_CONFIG_PRE_P120 = 0xa4,
	EMG_CONFIG_PRE_P240 = 0xa5,
	EMG_CONFIG_PRE_P480 = 0xa6,
	EMG_CONFIG_PRE_P960 = 0xa7,
	EMG_CONFIG_STRESS_P960_T10 = 0xb1,
	EMG_CONFIG_STRESS_P960_T8 = 0xb2,
	EMG_CONFIG_STRESS_P960_T7 = 0xb3,
	EMG_CONFIG_STRESS_P960_T6 = 0xb4,
};

enum emg_control_command {
	EMG_CONTROL_PREPARE = 1,
	EMG_CONTROL_START_DATA = 2,
	EMG_CONTROL_STOP_DATA = 3,
	EMG_CONTROL_REPORT_SYNC = 4,
};

enum emg_sync_group {
	EMG_SYNC_GROUP_INITIAL = 0,
	EMG_SYNC_GROUP_FINAL = 1,
};

struct emg_fragment_header {
	uint8_t config_id;
	uint32_t packet_id;
	uint64_t acquisition_time_us;
	uint64_t tx_time_us;
	uint16_t packet_payload_len;
	uint16_t fragment_offset;
	uint8_t fragment_index;
	uint8_t fragment_count;
	uint16_t fragment_payload_len;
};

struct emg_experiment_stats {
	uint8_t config_id;
	uint32_t generated_packets;
	uint32_t enqueued_packets;
	uint32_t transmitted_packets;
	uint32_t queue_dropped_packets;
	uint32_t stop_dropped_packets;
	uint32_t send_error_packets;
	uint32_t queue_high_watermark;
};

struct emg_sync_record {
	uint16_t pulse_id;
	uint8_t group;
	uint64_t local_time_us;
};

static inline void emg_control_encode(uint8_t *dst, uint8_t command)
{
	sys_put_le16(EMG_CONTROL_MAGIC, &dst[0]);
	dst[2] = EMG_PROTOCOL_VERSION;
	dst[3] = command;
}

static inline int emg_control_decode(uint8_t *command, const uint8_t *src,
				     uint16_t len)
{
	if ((len != EMG_CONTROL_SIZE) ||
	    (sys_get_le16(&src[0]) != EMG_CONTROL_MAGIC) ||
	    (src[2] != EMG_PROTOCOL_VERSION) ||
	    (src[3] < EMG_CONTROL_PREPARE) ||
	    (src[3] > EMG_CONTROL_REPORT_SYNC)) {
		return -1;
	}

	*command = src[3];
	return 0;
}

static inline void emg_sync_record_encode(uint8_t *dst,
					   const struct emg_sync_record *record)
{
	sys_put_le16(EMG_SYNC_MAGIC, &dst[0]);
	dst[2] = EMG_PROTOCOL_VERSION;
	dst[3] = record->group;
	sys_put_le16(record->pulse_id, &dst[4]);
	dst[6] = 0U;
	dst[7] = 0U;
	sys_put_le64(record->local_time_us, &dst[8]);
}

static inline int emg_sync_record_decode(struct emg_sync_record *record,
					  const uint8_t *src, uint16_t len)
{
	if ((len != EMG_SYNC_RECORD_SIZE) ||
	    (sys_get_le16(&src[0]) != EMG_SYNC_MAGIC) ||
	    (src[2] != EMG_PROTOCOL_VERSION) ||
	    (src[3] >= EMG_SYNC_GROUP_COUNT)) {
		return -1;
	}

	record->group = src[3];
	record->pulse_id = sys_get_le16(&src[4]);
	record->local_time_us = sys_get_le64(&src[8]);

	if ((record->pulse_id >= EMG_SYNC_RECORD_COUNT) ||
	    (record->pulse_id / EMG_SYNC_PULSES_PER_GROUP != record->group)) {
		return -1;
	}

	return 0;
}

static inline void emg_stats_record_encode(
	uint8_t *dst, const struct emg_experiment_stats *stats)
{
	sys_put_le16(EMG_STATS_MAGIC, &dst[0]);
	dst[2] = EMG_PROTOCOL_VERSION;
	dst[3] = stats->config_id;
	sys_put_le32(stats->generated_packets, &dst[4]);
	sys_put_le32(stats->enqueued_packets, &dst[8]);
	sys_put_le32(stats->transmitted_packets, &dst[12]);
	sys_put_le32(stats->queue_dropped_packets, &dst[16]);
	sys_put_le32(stats->stop_dropped_packets, &dst[20]);
	sys_put_le32(stats->send_error_packets, &dst[24]);
	sys_put_le32(stats->queue_high_watermark, &dst[28]);
}

static inline int emg_stats_record_decode(
	struct emg_experiment_stats *stats, const uint8_t *src, uint16_t len)
{
	if ((len != EMG_STATS_RECORD_SIZE) ||
	    (sys_get_le16(&src[0]) != EMG_STATS_MAGIC) ||
	    (src[2] != EMG_PROTOCOL_VERSION)) {
		return -1;
	}

	stats->config_id = src[3];
	stats->generated_packets = sys_get_le32(&src[4]);
	stats->enqueued_packets = sys_get_le32(&src[8]);
	stats->transmitted_packets = sys_get_le32(&src[12]);
	stats->queue_dropped_packets = sys_get_le32(&src[16]);
	stats->stop_dropped_packets = sys_get_le32(&src[20]);
	stats->send_error_packets = sys_get_le32(&src[24]);
	stats->queue_high_watermark = sys_get_le32(&src[28]);
	return 0;
}

static inline void emg_header_encode(uint8_t *dst,
				     const struct emg_fragment_header *header)
{
	sys_put_le16(EMG_PROTOCOL_MAGIC, &dst[0]);
	dst[2] = EMG_PROTOCOL_VERSION;
	dst[3] = header->config_id;
	sys_put_le32(header->packet_id, &dst[4]);
	sys_put_le64(header->acquisition_time_us, &dst[8]);
	sys_put_le64(header->tx_time_us, &dst[16]);
	sys_put_le16(header->packet_payload_len, &dst[24]);
	sys_put_le16(header->fragment_offset, &dst[26]);
	dst[28] = header->fragment_index;
	dst[29] = header->fragment_count;
	sys_put_le16(header->fragment_payload_len, &dst[30]);
}

static inline int emg_header_decode(struct emg_fragment_header *header,
				    const uint8_t *src, uint16_t len)
{
	if ((len < EMG_HEADER_SIZE) ||
	    (sys_get_le16(&src[0]) != EMG_PROTOCOL_MAGIC) ||
	    (src[2] != EMG_PROTOCOL_VERSION)) {
		return -1;
	}

	header->config_id = src[3];
	header->packet_id = sys_get_le32(&src[4]);
	header->acquisition_time_us = sys_get_le64(&src[8]);
	header->tx_time_us = sys_get_le64(&src[16]);
	header->packet_payload_len = sys_get_le16(&src[24]);
	header->fragment_offset = sys_get_le16(&src[26]);
	header->fragment_index = src[28];
	header->fragment_count = src[29];
	header->fragment_payload_len = sys_get_le16(&src[30]);

	if ((header->packet_payload_len > EMG_PACKET_DATA_MAX) ||
	    (header->fragment_count == 0U) ||
	    (header->fragment_count > EMG_FRAGMENT_COUNT_MAX) ||
	    (header->fragment_index >= header->fragment_count) ||
	    (header->fragment_payload_len > EMG_FRAGMENT_DATA_MAX) ||
	    ((uint32_t)header->fragment_offset + header->fragment_payload_len >
	     header->packet_payload_len) ||
	    ((uint32_t)EMG_HEADER_SIZE + header->fragment_payload_len != len)) {
		return -1;
	}

	return 0;
}

#endif /* EMG_PROTOCOL_H_ */

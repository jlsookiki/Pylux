// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <munit.h>

#include <chiaki/audioreceiver.h>
#include <chiaki/session.h>
#include <chiaki/takion.h>

#include "test_log.h"

#include <stdlib.h>
#include <string.h>

// The two PSCLOUD audio layouts, with values taken from real streams.
static MunitResult test_layout_detection(const MunitParameter params[], void *user)
{
	ChiakiTakionAVPacket packet;
	memset(&packet, 0, sizeof(packet));

	// Black Myth: Wukong on a Steam Deck, 2026-10-30: packed Remote Play layout.
	// 0x7821 = unit size 0x78 (120 bytes), 2 fec units, 1 source unit, 3 total.
	packet.units_in_frame_total = 3;
	packet.units_in_frame_fec = 0x7821;
	munit_assert(!chiaki_takion_av_packet_audio_is_unitized(&packet));
	munit_assert_uint8(chiaki_takion_av_packet_audio_unit_size(&packet), ==, 120);
	munit_assert_uint8(chiaki_takion_av_packet_audio_source_units_count(&packet), ==, 1);
	munit_assert_uint8(chiaki_takion_av_packet_audio_fec_units_count(&packet), ==, 2);

	// Unitized layout: literal FEC count, one unit per packet.
	packet.units_in_frame_total = 3;
	packet.units_in_frame_fec = 1;
	munit_assert(chiaki_takion_av_packet_audio_is_unitized(&packet));
	packet.units_in_frame_fec = 0;
	munit_assert(chiaki_takion_av_packet_audio_is_unitized(&packet));
	packet.units_in_frame_fec = 3;
	munit_assert(chiaki_takion_av_packet_audio_is_unitized(&packet));

	// Any nonzero unit size byte makes the packed value exceed a sane total.
	packet.units_in_frame_total = 16;
	packet.units_in_frame_fec = 0x0100;
	munit_assert(!chiaki_takion_av_packet_audio_is_unitized(&packet));

	return MUNIT_OK;
}

static int sink_frames;
static size_t sink_last_size;

static void sink_frame_cb(uint8_t *buf, size_t buf_size, void *user)
{
	(void)buf; (void)user;
	sink_frames++;
	sink_last_size = buf_size;
}

// The outcome the layout fix exists for: a packed-layout PSCLOUD packet
// reaches the audio sink instead of being rejected by the unitized
// reassembler, and a unitized packet still goes through the reassembler.
static MunitResult test_pscloud_packets_reach_sink(const MunitParameter params[], void *user)
{
	(void)params; (void)user;
	ChiakiSession *session = calloc(1, sizeof(ChiakiSession));
	munit_assert_not_null(session);
	session->log = get_test_log();
	session->service_type = CHIAKI_SERVICE_TYPE_PSCLOUD;
	session->audio_sink.frame_cb = sink_frame_cb;

	ChiakiAudioReceiver receiver;
	munit_assert_int(chiaki_audio_receiver_init(&receiver, session, NULL), ==, CHIAKI_ERR_SUCCESS);
	munit_assert_not_null(receiver.pscloud_audio_reassembler);
	sink_frames = 0;
	sink_last_size = 0;

	// Packed Remote Play layout as seen in the Wukong stream: one AV packet
	// holding the whole generation of 3 units x 120 bytes.
	uint8_t packed_data[360];
	memset(packed_data, 0xab, sizeof(packed_data));
	ChiakiTakionAVPacket packed;
	memset(&packed, 0, sizeof(packed));
	packed.codec = 5;
	packed.frame_index = 1000;
	packed.units_in_frame_total = 3;
	packed.units_in_frame_fec = 0x7821;
	packed.data = packed_data;
	packed.data_size = sizeof(packed_data);
	chiaki_audio_receiver_av_packet(&receiver, &packed);
	// One source unit; the two fec units repeat earlier frames and are dropped.
	munit_assert_int(sink_frames, ==, 1);
	munit_assert_size(sink_last_size, ==, 120);

	// Unitized layout: one unit per packet, literal fec count. The first
	// unit of a generation is a source unit and is emitted as it arrives.
	uint8_t unit_data[120];
	memset(unit_data, 0xcd, sizeof(unit_data));
	ChiakiTakionAVPacket unit;
	memset(&unit, 0, sizeof(unit));
	unit.codec = 5;
	unit.frame_index = 2000;
	unit.unit_index = 0;
	unit.units_in_frame_total = 3;
	unit.units_in_frame_fec = 2;
	unit.data = unit_data;
	unit.data_size = sizeof(unit_data);
	chiaki_audio_receiver_av_packet(&receiver, &unit);
	munit_assert_int(sink_frames, ==, 2);
	munit_assert_size(sink_last_size, ==, 120);

	chiaki_audio_receiver_fini(&receiver);
	free(session);
	return MUNIT_OK;
}

MunitTest tests_pscloud_audio[] = {
	{
		"/packets_reach_sink",
		test_pscloud_packets_reach_sink,
		NULL,
		NULL,
		MUNIT_TEST_OPTION_NONE,
		NULL
	},
	{
		"/layout_detection",
		test_layout_detection,
		NULL,
		NULL,
		MUNIT_TEST_OPTION_NONE,
		NULL
	},
	{ NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};

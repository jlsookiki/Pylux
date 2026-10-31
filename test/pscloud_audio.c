// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <munit.h>

#include <chiaki/takion.h>

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

MunitTest tests_pscloud_audio[] = {
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

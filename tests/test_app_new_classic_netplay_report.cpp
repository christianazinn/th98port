#include "th03/app/new_classic_netplay_report.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

namespace {

std::size_t checks = 0U;
std::size_t failures = 0U;

void expect(bool condition, const char *message)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << message << '\n';
	}
}

void put_u16(std::vector<std::byte> &bytes, std::size_t at, std::uint16_t value)
{
	bytes[at] = static_cast<std::byte>(value & 0xFFU);
	bytes[at + 1U] = static_cast<std::byte>(value >> 8U);
}

std::vector<std::byte> font_fixture()
{
	std::vector<std::byte> bytes(3'168U, std::byte{0U});
	constexpr std::array magic{'T', '3', 'P', 'F', 'N', 'T', '1'};
	for(std::size_t index = 0U; index < magic.size(); ++index) {
		bytes[index] = static_cast<std::byte>(magic[index]);
	}
	put_u16(bytes, 8U, 3'168U);
	put_u16(bytes, 10U, 32U);
	put_u16(bytes, 12U, 32U);
	put_u16(bytes, 14U, 128U);
	put_u16(bytes, 16U, 95U);
	bytes[18U] = std::byte{0x20U};
	bytes[19U] = std::byte{0x7EU};
	bytes[20U] = std::byte{16U};
	bytes[21U] = std::byte{16U};
	bytes[22U] = std::byte{0x3FU};
	for(std::size_t glyph = 0U; glyph < 95U; ++glyph) {
		bytes[32U + glyph] = std::byte{8U};
		bytes[128U + (glyph * 32U)] = std::byte{0x80U};
	}
	std::uint16_t checksum = 0U;
	for(std::size_t index = 32U; index < bytes.size(); ++index) {
		checksum = static_cast<std::uint16_t>(
			checksum + std::to_integer<std::uint8_t>(bytes[index])
		);
	}
	put_u16(bytes, 24U, checksum);
	return bytes;
}

std::shared_ptr<const th03::app::NewClassicReplayPatchAssets> assets_fixture()
{
	auto decoded = th03::app::detail::decode_new_classic_menu_font(font_fixture());
	if(!decoded) {
		return {};
	}
	auto assets = std::make_shared<th03::app::NewClassicReplayPatchAssets>();
	assets->menu_font = std::move(*decoded);
	auto &background = assets->replay_list_background;
	background.palette.emplace();
	for(std::size_t index = 0U; index < 16U; ++index) {
		background.palette->colors[index] = {
			static_cast<std::uint8_t>(index),
			static_cast<std::uint8_t>(index),
			static_cast<std::uint8_t>(index),
		};
	}
	background.image.width = 640U;
	background.image.height = 400U;
	background.image.indices.assign(256'000U, 2U);
	return assets;
}

th03::app::NewClassicNetplayReportMetrics metrics()
{
	return {
		.confirmed_frames = 1'234U,
		.session = {
			.resolved_frames = 1'234U,
			.predicted_frames = 89U,
			.rollback_requests = 12U,
			.rewinds = 7U,
			.resimulated_frames = 34U,
			.prediction_stall_events = 5U,
			.remote_wait_events = 6U,
			.maximum_prediction_depth = 8U,
		},
		.missed_deadlines = 3U,
		.round_trip_time_ms = 47U,
	};
}

void test_kinds_and_source_surface()
{
	const auto assets = assets_fixture();
	for(const auto kind : {
		th03::app::NewClassicNetplayReportKind::match_complete,
		th03::app::NewClassicNetplayReportKind::forfeit_loss,
		th03::app::NewClassicNetplayReportKind::forfeit_win,
		th03::app::NewClassicNetplayReportKind::forfeit_mutual,
		th03::app::NewClassicNetplayReportKind::peer_left,
		th03::app::NewClassicNetplayReportKind::peer_timeout,
		th03::app::NewClassicNetplayReportKind::desync,
	}) {
		th03::app::NewClassicNetplayReport report(kind, metrics(), assets);
		expect(report.valid(), "every report kind composes");
		expect(report.kind() == kind, "report retains its typed outcome");
		expect(report.metrics().confirmed_frames == 1'234U,
			"report retains local-only counters");
		expect(report.metrics().round_trip_time_ms == 47U,
			"report retains advisory transport round-trip time");
		expect(report.surface().pixels.size() == 256'000U &&
			!report.surface().host_overlay && report.surface().shown_page ==
				th03::present::Page::one,
			"report is an authenticated source-domain hidden-page publication");
	}
}

void test_release_and_application_quit()
{
	const auto assets = assets_fixture();
	th03::app::NewClassicNetplayReport report(
		th03::app::NewClassicNetplayReportKind::peer_timeout, metrics(), assets
	);
	auto held = report.consume({.confirm = true});
	expect(held.status == th03::app::NewClassicNetplayReportStatus::running,
		"held terminal input cannot dismiss the report");
	static_cast<void>(report.consume({}));
	auto fresh = report.consume({.cancel = true});
	expect(fresh.status == th03::app::NewClassicNetplayReportStatus::complete,
		"fresh interface input dismisses after release");

	th03::app::NewClassicNetplayReport quitting(
		th03::app::NewClassicNetplayReportKind::desync, metrics(), assets
	);
	auto quit = quitting.consume({.app_quit_requested = true});
	expect(quit.status ==
		th03::app::NewClassicNetplayReportStatus::application_quit,
		"application quit remains unconditional");
}

void test_missing_round_trip_is_explicitly_unavailable()
{
	const auto assets = assets_fixture();
	auto unavailable = metrics();
	unavailable.round_trip_time_ms.reset();
	th03::app::NewClassicNetplayReport report(
		th03::app::NewClassicNetplayReportKind::match_complete,
		unavailable,
		assets
	);
	expect(report.valid() && !report.metrics().round_trip_time_ms,
		"transport without RTT composes an explicitly unavailable report value");
}

void test_invalid_assets_fail_closed()
{
	th03::app::NewClassicNetplayReport report(
		th03::app::NewClassicNetplayReportKind::match_complete, metrics(), {}
	);
	expect(!report.valid() && report.status() ==
		th03::app::NewClassicNetplayReportStatus::complete,
		"missing authenticated assets fail closed without a host fallback");
}

} // namespace

int main()
{
	test_kinds_and_source_surface();
	test_release_and_application_quit();
	test_missing_round_trip_is_explicitly_unavailable();
	test_invalid_assets_fail_closed();
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}

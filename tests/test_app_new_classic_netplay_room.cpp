#include "th03/app/new_classic_netplay_room.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string_view>
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

void release(th03::app::NewClassicNetplayRoom &room)
{
	static_cast<void>(room.consume({}));
}

void test_host_intent_is_confirmation_gated()
{
	using namespace th03::app;
	NewClassicNetplayRoom room(assets_fixture());
	expect(room.valid() && room.surface().pixels.size() == 256'000U &&
		!room.surface().host_overlay,
		"room menu is an authenticated source-domain surface");
	auto held = room.consume({.ui = {.confirm = true}});
	expect(held.status == NewClassicNetplayRoomStatus::running &&
		!room.take_launch(),
		"held VS confirmation cannot create a network launch intent");
	release(room);
	auto entered = room.consume({.ui = {.confirm = true}});
	expect(entered.surface_changed && !room.take_launch(),
		"Create Room first enters an offline confirmation owner");
	release(room);
	auto launched = room.consume({.ui = {.confirm = true}});
	auto intent = room.take_launch();
	expect(launched.status == NewClassicNetplayRoomStatus::launch_requested &&
		intent && intent->mode == NewClassicNetplayRoomLaunchMode::host &&
		intent->host_port == 31'415U && intent->join_endpoint.empty(),
		"fresh confirmation publishes the typed default-port host intent");
	expect(!room.take_launch(), "launch intent is single-consumer");
}

void test_join_entry_and_validation()
{
	using namespace th03::app;
	NewClassicNetplayRoom room(assets_fixture());
	release(room);
	static_cast<void>(room.consume({.ui = {.down = true}}));
	release(room);
	static_cast<void>(room.consume({.ui = {.confirm = true}}));
	release(room);
	const auto invalid = room.consume({.ui = {.confirm = true}});
	expect(invalid.status == NewClassicNetplayRoomStatus::running &&
		invalid.surface_changed && !room.take_launch(),
		"empty Join endpoint is visibly rejected without networking");
	release(room);
	constexpr std::string_view endpoint = "100.64.0.2:31415";
	for(const char character : endpoint) {
		const auto typed = room.consume({.text_character = character});
		expect(typed.surface_changed,
			"each accepted endpoint character redraws the local editor");
	}
	static_cast<void>(room.consume({.erase = true}));
	static_cast<void>(room.consume({.text_character = '5'}));
	const auto connected = room.consume({.ui = {.confirm = true}});
	auto intent = room.take_launch();
	expect(connected.status == NewClassicNetplayRoomStatus::launch_requested &&
		intent && intent->mode == NewClassicNetplayRoomLaunchMode::join &&
		intent->join_endpoint == endpoint,
		"validated Tailscale endpoint becomes one typed join intent");
}

void test_settings_back_and_application_quit()
{
	using namespace th03::app;
	NewClassicNetplayRoom room(assets_fixture());
	release(room);
	for(std::uint8_t step = 0U; step < 2U; ++step) {
		static_cast<void>(room.consume({.ui = {.down = true}}));
		release(room);
	}
	static_cast<void>(room.consume({.ui = {.confirm = true}}));
	release(room);
	static_cast<void>(room.consume({.ui = {.down = true}}));
	release(room);
	const auto adjusted = room.consume({.ui = {.right = true}});
	expect(adjusted.surface_changed,
		"Network Settings adjusts the direct host port locally");
	release(room);
	static_cast<void>(room.consume({.ui = {.cancel = true}}));
	release(room);
	const auto back = room.consume({.ui = {.cancel = true}});
	expect(back.status == NewClassicNetplayRoomStatus::back,
		"room menu Cancel returns to New Classic without a socket");

	NewClassicNetplayRoom quitting(assets_fixture());
	const auto quit = quitting.consume({
		.ui = {.app_quit_requested = true},
	});
	expect(quit.status == NewClassicNetplayRoomStatus::application_quit,
		"application quit remains unconditional in every room phase");
}

void test_missing_assets_fail_closed()
{
	th03::app::NewClassicNetplayRoom room({});
	expect(!room.valid() && room.status() ==
		th03::app::NewClassicNetplayRoomStatus::back,
		"room UI has no host-font fallback when authenticated art is absent");
}

} // namespace

int main()
{
	test_host_intent_is_confirmation_gated();
	test_join_entry_and_validation();
	test_settings_back_and_application_quit();
	test_missing_assets_fail_closed();
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}

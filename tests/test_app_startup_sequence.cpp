#include "th03/app/runtime/startup_sequence.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <iostream>
#include <utility>

namespace th03::app::detail {

struct StartupSequenceTestAccess final {
	static std::expected<StartupSequence, StartupSequenceError> create(
		assets::BfntSheet logo
	)
	{
		return StartupSequence::create_from_decoded_logo(std::move(logo));
	}
};

} // namespace th03::app::detail

namespace {

using th03::app::StartupSequence;
using th03::app::StartupSequenceStatus;
using th03::app::StartupSlot;

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

th03::assets::BfntSheet synthetic_logo()
{
	th03::assets::BfntSheet result{};
	result.consumer = th03::assets::BfntConsumerPolicy::superimpose;
	result.header.color_flags = 0x83U;
	result.header.version = 0x16U;
	result.header.pixel_width = 64U;
	result.header.pixel_height = 16U;
	result.header.first_code = 0U;
	result.header.last_code = 15U;
	result.transparent_color = 0U;
	result.palette.emplace();
	for(std::size_t index = 0U; index < result.palette->colors.size(); ++index) {
		result.palette->colors[index] = {
			static_cast<std::uint8_t>(index),
			static_cast<std::uint8_t>((index * 3U) & 0x0FU),
			static_cast<std::uint8_t>((index * 5U) & 0x0FU),
		};
	}
	constexpr std::size_t plane_bytes = (64U / 8U) * 16U;
	for(std::uint16_t code = 0U; code < 16U; ++code) {
		th03::assets::BfntGlyph glyph{};
		glyph.code = code;
		th03::assets::PlanarBitmap4 bitmap{};
		for(auto &plane : bitmap.color_planes) {
			plane.resize(plane_bytes, 0U);
		}
		// One deterministic visible vertical stripe per glyph is enough to
		// exercise ordinary and wave placement without embedding game data.
		const auto stripe = static_cast<std::uint8_t>(0x80U >> (code & 7U));
		for(std::size_t row = 0U; row < 16U; ++row) {
			const auto offset = (row * 8U) + ((code >> 3U) & 1U);
			bitmap.color_planes[code & 3U][offset] = stripe;
		}
		glyph.pixels = std::move(bitmap);
		result.glyphs.push_back(std::move(glyph));
	}
	return result;
}

std::uint64_t surface_hash(const th03::present::SourceSurface &surface)
{
	std::uint64_t hash = 14695981039346656037ULL;
	for(const auto pixel : surface.pixels) {
		hash ^= pixel.value;
		hash *= 1099511628211ULL;
	}
	return hash;
}

StartupSequence make_sequence()
{
	auto created = th03::app::detail::StartupSequenceTestAccess::create(
		synthetic_logo()
	);
	expect(created.has_value(), "the source-shaped synthetic BFNT is accepted");
	return std::move(*created);
}

void test_profile_and_first_publication()
{
	auto invalid = synthetic_logo();
	invalid.header.pixel_width = 32U;
	auto rejected = th03::app::detail::StartupSequenceTestAccess::create(
		std::move(invalid)
	);
	expect(
		!rejected && rejected.error().code ==
			th03::app::StartupSequenceErrorCode::asset_profile_mismatch,
		"the startup owner rejects a non-TOUHOU BFNT geometry"
	);

	auto sequence = make_sequence();
	expect(
		sequence.slot() == StartupSlot::zun_soft &&
		sequence.status() == StartupSequenceStatus::running &&
		sequence.source_frame() == 0U,
		"the first ordered startup slot is the ZUN Soft scene"
	);
	const auto black = surface_hash(sequence.surface());
	const auto first_wait = sequence.consume({});
	expect(
		!first_wait.surface_changed && sequence.source_frame() == 0U,
		"the first rendered source frame remains hidden through wait one"
	);
	const auto first_flip = sequence.consume({});
	expect(
		first_flip.surface_changed && sequence.source_frame() == 1U &&
		sequence.surface().shown_page == th03::present::Page::zero &&
		surface_hash(sequence.surface()) != black,
		"wait two publishes source frame zero on page zero"
	);
	(void)sequence.consume({});
	const auto second_flip = sequence.consume({});
	expect(
		second_flip.surface_changed && sequence.source_frame() == 2U &&
		sequence.surface().shown_page == th03::present::Page::one,
		"the following source publication alternates to page one"
	);
}

void test_skip_and_reserved_slot()
{
	auto sequence = make_sequence();
	(void)sequence.consume({.any_input = true});
	expect(
		sequence.status() == StartupSequenceStatus::running,
		"input is sampled after the source's two waits rather than before them"
	);
	const auto skipped = sequence.consume({.any_input = true});
	expect(
		skipped.surface_changed &&
		skipped.status == StartupSequenceStatus::complete &&
		skipped.slot == StartupSlot::complete,
		"held input skips through the logo and the empty project-credit slot"
	);
	expect(
		std::ranges::all_of(
			sequence.surface().pixels,
			[](const auto &pixel) { return pixel.value == 0U; }
		),
		"startup completion publishes the source black clear"
	);
}

void test_natural_cadence_and_application_quit()
{
	auto sequence = make_sequence();
	std::size_t publications = 0U;
	std::size_t ticks = 0U;
	while(sequence.status() == StartupSequenceStatus::running && ticks < 500U) {
		const auto result = sequence.consume({});
		publications += result.surface_changed ? 1U : 0U;
		++ticks;
	}
	expect(
		ticks == 463U && publications == 232U &&
		sequence.source_frame() == 231U &&
		sequence.status() == StartupSequenceStatus::complete,
		"231 double-wait frames are followed by one immediate black exit publication"
	);

	auto quit = make_sequence();
	const auto closed = quit.consume({.app_quit_requested = true});
	expect(
		closed.status == StartupSequenceStatus::quit_requested &&
		!closed.surface_changed,
		"host close exits immediately without inventing a source publication"
	);
}

void test_authentic_profile(const std::filesystem::path &path)
{
	auto sequence = StartupSequence::load(path);
	expect(
		sequence.has_value(),
		"the authenticated 8,272-byte TH01-TH03 TOUHOU.DAT profile loads"
	);
	if(!sequence) {
		return;
	}
	(void)sequence->consume({});
	const auto first = sequence->consume({});
	expect(
		first.surface_changed && sequence->source_frame() == 1U &&
		sequence->surface().palette_tone == 2,
		"the authentic logo publishes its first page at source tone 2"
	);
}

} // namespace

int main(int argc, char **argv)
{
	test_profile_and_first_publication();
	test_skip_and_reserved_slot();
	test_natural_cadence_and_application_quit();
	if(argc >= 2) {
		test_authentic_profile(argv[1]);
	}
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}

#include "../src/app/classic_mainl_continue_internal.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "th03/app/authentic_text.hpp"

namespace {

using th03::ContinueChoice;
using th03::app::ClassicAudioCommand;
using th03::app::ClassicAudioCommandType;
using th03::app::ClassicAudioStatus;
using th03::app::ClassicMainlContinueBoundary;
using th03::app::ClassicMainlContinueCompletion;
using th03::app::ClassicMainlContinueErrorCode;
using th03::app::ClassicMainlContinueInput;
using th03::app::ClassicMainlContinuePhase;
using th03::app::ClassicMainlContinueScene;
using th03::app::ClassicMainlContinueTestAccess;
using th03::app::ClassicMainlGameOverBoundary;
using th03::app::ClassicMainlGameOverPhase;
using th03::app::ClassicMainlGameOverScene;
using th03::app::ClassicMusicCue;
using th03::app::LoadedMainResource;
using th03::app::MainResourceKey;
using th03::app::MainResourceRole;
using th03::app::ResourceBytes;
using th03::present::AnkGlyph;
using th03::present::AnkGlyphProvider;
using th03::present::JisGlyph;
using th03::present::Page;
using th03::present::Point;

std::size_t checks = 0U;
std::size_t failures = 0U;

void expect(bool condition, std::string_view message)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << message << '\n';
	}
}

void append_u8(std::vector<std::byte> &bytes, std::uint8_t value)
{
	bytes.push_back(static_cast<std::byte>(value));
}

void append_be16(std::vector<std::byte> &bytes, std::uint16_t value)
{
	append_u8(bytes, static_cast<std::uint8_t>(value >> 8U));
	append_u8(bytes, static_cast<std::uint8_t>(value));
}

void append_le16(std::vector<std::byte> &bytes, std::uint16_t value)
{
	append_u8(bytes, static_cast<std::uint8_t>(value));
	append_u8(bytes, static_cast<std::uint8_t>(value >> 8U));
}

class BitWriter final {
public:
	void bit(std::uint8_t value)
	{
		if((count_ % 8U) == 0U) {
			bytes_.push_back(std::byte{0U});
		}
		const auto shift = static_cast<unsigned int>(7U - (count_ % 8U));
		auto byte = std::to_integer<std::uint8_t>(bytes_.back());
		byte = static_cast<std::uint8_t>(
			byte | static_cast<std::uint8_t>((value & 1U) << shift)
		);
		bytes_.back() = static_cast<std::byte>(byte);
		++count_;
	}

	void bits(std::uint32_t value, unsigned int count)
	{
		for(auto remaining = count; remaining > 0U; --remaining) {
			bit(static_cast<std::uint8_t>(
				(value >> (remaining - 1U)) & 1U
			));
		}
	}

	[[nodiscard]] const std::vector<std::byte> &bytes() const noexcept
	{
		return bytes_;
	}

private:
	std::vector<std::byte> bytes_{};
	std::size_t count_ = 0U;
};

class ColorEncoder final {
public:
	ColorEncoder()
	{
		for(std::size_t previous = 0U; previous < table_.size(); ++previous) {
			for(std::size_t index = 0U; index < table_[previous].size(); ++index) {
				table_[previous][index] = static_cast<std::uint8_t>(
					(previous + index + 1U) & 0x0FU
				);
			}
		}
	}

	void color(BitWriter &bits, std::uint8_t previous, std::uint8_t color)
	{
		auto &table = table_[previous & 0x0FU];
		const auto found = std::find(table.begin(), table.end(), color);
		const auto index = static_cast<std::uint8_t>(
			std::distance(table.begin(), found)
		);
		write_code(bits, static_cast<std::uint8_t>(index ^ 0x0FU));
		std::move(found + 1, table.end(), found);
		table.back() = color;
	}

private:
	static void write_code(BitWriter &bits, std::uint8_t code)
	{
		if(code <= 1U) {
			bits.bit(1U);
			bits.bits(code, 1U);
		} else if(code <= 3U) {
			bits.bits(0U, 2U);
			bits.bits(static_cast<std::uint32_t>(code - 2U), 1U);
		} else if(code <= 7U) {
			bits.bits(2U, 3U);
			bits.bits(static_cast<std::uint32_t>(code - 4U), 2U);
		} else {
			bits.bits(3U, 3U);
			bits.bits(static_cast<std::uint32_t>(code - 8U), 3U);
		}
	}

	std::array<std::array<std::uint8_t, 16U>, 16U> table_{};
};

void write_position(BitWriter &bits, std::uint8_t position)
{
	if(position < 3U) {
		bits.bits(position, 2U);
	} else {
		bits.bits(3U, 2U);
		bits.bit(static_cast<std::uint8_t>(position - 3U));
	}
}

void write_length(BitWriter &bits, std::size_t length)
{
	unsigned int suffix = 0U;
	for(auto remaining = length; remaining > 1U; remaining >>= 1U) {
		++suffix;
	}
	for(unsigned int index = 0U; index < suffix; ++index) {
		bits.bit(1U);
	}
	bits.bit(0U);
	const auto base = std::size_t{1U} << suffix;
	bits.bits(static_cast<std::uint32_t>(length - base), suffix);
}

std::vector<std::byte> make_pi(
	std::size_t exact_size,
	std::uint8_t fill,
	std::uint8_t palette_seed,
	std::uint16_t width = 640U
)
{
	constexpr std::uint16_t height = 400U;
	std::vector<std::byte> bytes;
	for(const auto value : {'P', 'i', 'T', '3'}) {
		append_u8(bytes, static_cast<std::uint8_t>(value));
	}
	append_u8(bytes, 0x1AU);
	append_u8(bytes, 0U);
	append_u8(bytes, 0U);
	append_u8(bytes, 0U);
	append_u8(bytes, 0U);
	append_u8(bytes, 4U);
	for(const auto value : {1U, 2U, 3U, 4U}) {
		append_u8(bytes, static_cast<std::uint8_t>(value));
	}
	append_be16(bytes, 0U);
	append_be16(bytes, width);
	append_be16(bytes, height);
	for(std::uint8_t color = 0U; color < 16U; ++color) {
		append_u8(bytes, static_cast<std::uint8_t>(
			((color + palette_seed) & 0x0FU) << 4U
		));
		append_u8(bytes, static_cast<std::uint8_t>(
			((15U - color + palette_seed) & 0x0FU) << 4U
		));
		append_u8(bytes, static_cast<std::uint8_t>(
			((color ^ palette_seed) & 0x0FU) << 4U
		));
	}
	BitWriter bits;
	ColorEncoder colors;
	colors.color(bits, 0U, fill);
	colors.color(bits, fill, fill);
	write_position(bits, 1U);
	write_length(bits, (static_cast<std::size_t>(width) * height / 2U) - 1U);
	bytes.insert(bytes.end(), bits.bytes().begin(), bits.bytes().end());
	expect(bytes.size() <= exact_size, "synthetic PI fits canonical byte count");
	bytes.resize(exact_size, std::byte{0U});
	return bytes;
}

std::vector<std::byte> make_choice_cdg(std::uint16_t width = 96U)
{
	constexpr std::uint16_t height = 41U;
	const std::uint8_t cel_count = (width == 96U) ? 4U : 6U;
	const auto stride = static_cast<std::size_t>(width) / 8U;
	const auto plane_size = stride * height;
	std::vector<std::byte> bytes;
	append_le16(bytes, static_cast<std::uint16_t>(plane_size));
	append_le16(bytes, width);
	append_le16(bytes, height);
	append_le16(bytes, static_cast<std::uint16_t>(80U * (height - 1U)));
	append_le16(bytes, static_cast<std::uint16_t>(width / 32U));
	append_u8(bytes, cel_count);
	append_u8(bytes, 0U);
	append_le16(bytes, 0U);
	append_le16(bytes, 0U);
	for(std::uint8_t cel = 0U; cel < cel_count; ++cel) {
		bytes.insert(bytes.end(), plane_size, std::byte{0xFFU});
		const auto color = static_cast<std::uint8_t>(cel + 1U);
		for(std::size_t plane = 0U; plane < 4U; ++plane) {
			const auto value = ((color & (1U << plane)) != 0U) ?
				std::byte{0xFFU} : std::byte{0U};
			bytes.insert(bytes.end(), plane_size, value);
		}
	}
	bytes.resize(9856U, std::byte{0U});
	return bytes;
}

class SyntheticFont final : public AnkGlyphProvider {
public:
	[[nodiscard]] std::optional<AnkGlyph> glyph(
		std::uint8_t
	) const override
	{
		return std::nullopt;
	}

	[[nodiscard]] std::optional<JisGlyph> jis_glyph(
		std::uint16_t canonical_jis
	) const override
	{
		JisGlyph glyph{};
		glyph.row_halves[0U] = static_cast<std::uint8_t>(
			0x80U >> (canonical_jis & 0x07U)
		);
		return glyph;
	}
};

class MissingFont final : public AnkGlyphProvider {
public:
	[[nodiscard]] std::optional<AnkGlyph> glyph(
		std::uint8_t
	) const override
	{
		return std::nullopt;
	}
};

std::vector<std::byte> bytes_for(const th03::app::MainResourceSpec &spec)
{
	switch(spec.key.role) {
	case MainResourceRole::continue_background:
		return make_pi(spec.expected_bytes, 5U, 1U);
	case MainResourceRole::continue_choice_cels:
		return make_choice_cdg();
	case MainResourceRole::gameover_background:
		return make_pi(spec.expected_bytes, 9U, 7U);
	default:
		break;
	}
	return {};
}

std::vector<LoadedMainResource> resources_for(std::uint8_t credits)
{
	const auto keys = th03::app::mainl_continue_resource_keys(credits);
	expect(keys.has_value(), "synthetic Continue resource keys resolve");
	std::vector<LoadedMainResource> result;
	if(!keys) {
		return result;
	}
	result.reserve(keys->size());
	for(const auto key : *keys) {
		const auto spec = *th03::app::main_resource_spec(key);
		auto bytes = std::make_shared<const std::vector<std::byte>>(
			bytes_for(spec)
		);
		expect(bytes->size() == spec.expected_bytes,
			"synthetic Continue resource has canonical byte count");
		result.push_back({.spec = spec, .bytes = std::move(bytes)});
	}
	return result;
}

std::unique_ptr<ClassicMainlContinueScene> create_scene(
	std::uint8_t credits
)
{
	auto resources = resources_for(credits);
	SyntheticFont font;
	auto scene = ClassicMainlContinueTestAccess::create_with_synthetic_resources(
		credits, resources, font
	);
	expect(scene.has_value(), "synthetic Continue scene constructs");
	return scene ? std::move(*scene) : nullptr;
}

void advance_continue_fade(
	ClassicMainlContinueScene &scene,
	ClassicMainlContinuePhase expected_after
)
{
	bool preload_requested = false;
	for(std::size_t boundary = 1U; boundary <= 18U; ++boundary) {
		const auto tick = scene.advance({});
		expect(tick && tick->boundary == ClassicMainlContinueBoundary::vsync,
			"Continue palette fade consumes one exact VSync boundary");
		preload_requested = tick && tick->gameover_preload_requested;
	}
	expect(scene.phase() == expected_after,
		"Continue palette fade reaches the exact next phase");
	if(expected_after == ClassicMainlContinuePhase::await_gameover_preload) {
		expect(
			preload_requested,
			"the final black boundary requests the source-late OVER preload"
		);
	}
}

void drain_continue_audio(
	ClassicMainlContinueScene &scene,
	ClassicAudioCommand expected
)
{
	expect(scene.take_audio_command() == expected,
		"Continue publishes the expected source audio command");
	expect(!scene.take_audio_command(), "Continue audio queue drains exactly");
}

void satisfy_gameover_preload(ClassicMainlContinueScene &scene)
{
	auto resources = resources_for(0U);
	const auto preloaded =
		ClassicMainlContinueTestAccess::preload_gameover_with_synthetic_resources(
			scene, resources
		);
	expect(
		preloaded && preloaded->phase == ClassicMainlContinuePhase::ready &&
			preloaded->boundary == ClassicMainlContinueBoundary::immediate &&
			preloaded->surface_changed,
		"OVER is loaded synchronously only after Continue reaches black"
	);
}

ClassicMainlContinueCompletion finish_declined_continue(
	std::uint8_t credits
)
{
	auto scene = create_scene(credits);
	if(credits == 0U) {
		auto completion = scene->take_completion();
		expect(completion.has_value(),
			"zero-credit Continue publishes an immediate decline capability");
		return std::move(*completion);
	}
	advance_continue_fade(*scene, ClassicMainlContinuePhase::waiting_for_input);
	const auto cancel = scene->advance({.cancel = true});
	expect(cancel && cancel->boundary == ClassicMainlContinueBoundary::immediate &&
		cancel->choice_committed,
		"Cancel declines on the same source input sample");
	drain_continue_audio(*scene, {
		.type = ClassicAudioCommandType::fade_song,
		.cue = ClassicMusicCue::high_score,
		.parameter = 3U,
	});
	advance_continue_fade(
		*scene, ClassicMainlContinuePhase::await_gameover_preload
	);
	satisfy_gameover_preload(*scene);
	expect(!scene->take_completion(),
		"Continue completion waits for the queued score-song stop");
	drain_continue_audio(*scene, {
		.type = ClassicAudioCommandType::stop_song,
		.cue = ClassicMusicCue::high_score,
	});
	auto completion = scene->take_completion();
	expect(completion && completion->choice() == ContinueChoice::decline,
		"declined Continue publishes the typed choice once");
	return std::move(*completion);
}

void test_continue_input_and_acceptance()
{
	auto scene = create_scene(3U);
	if(!scene) {
		return;
	}
	expect(scene->phase() == ClassicMainlContinuePhase::black_in &&
		scene->selected_choice() == ContinueChoice::accept &&
		scene->raster().palette_tone() == 0,
		"credited Continue begins black with Yes selected");
	expect(scene->raster().pixel(Page::zero, {192, 272}) == 1U &&
		scene->raster().pixel(Page::zero, {352, 272}) == 4U,
		"initial Continue draws source cels 0 and 3");
	advance_continue_fade(*scene, ClassicMainlContinuePhase::waiting_for_input);

	const auto toggled = scene->advance({.left = true});
	expect(toggled && toggled->boundary == ClassicMainlContinueBoundary::vsync &&
		toggled->input_sampled && toggled->choice_changed &&
		scene->selected_choice() == ContinueChoice::decline,
		"first held direction toggles once before the frame wait");
	expect(scene->raster().pixel(Page::zero, {192, 272}) == 3U &&
		scene->raster().pixel(Page::zero, {352, 272}) == 2U,
		"declined selection draws source cels 2 and 1");
	const auto held = scene->advance({.right = true});
	expect(held && !held->choice_changed &&
		scene->selected_choice() == ContinueChoice::decline,
		"either held direction remains locked until full release");
	expect(scene->advance({}).has_value(), "direction release consumes one frame");

	const auto before_accept = scene->raster().display_state_hash();
	const auto accept = scene->advance({.left = true, .ok = true});
	expect(accept && accept->boundary == ClassicMainlContinueBoundary::immediate &&
		accept->input_sampled && accept->choice_changed &&
		accept->choice_committed && accept->surface_changed &&
		scene->selected_choice() == ContinueChoice::accept &&
		scene->raster().display_state_hash() != before_accept,
		"direction toggles before same-sample OK and accepted credit redraw");
	expect(!scene->advance({}) && scene->phase() ==
		ClassicMainlContinuePhase::black_out,
		"undrained fade command blocks the first black-out boundary");
	drain_continue_audio(*scene, {
		.type = ClassicAudioCommandType::fade_song,
		.cue = ClassicMusicCue::high_score,
		.parameter = 3U,
	});
	advance_continue_fade(
		*scene, ClassicMainlContinuePhase::await_gameover_preload
	);
	satisfy_gameover_preload(*scene);
	expect(scene->raster().pixel(Page::zero, {10, 10}) == 9U &&
		scene->raster().palette_tone() == 0,
		"final Continue boundary installs OVER at tone zero");
	drain_continue_audio(*scene, {
		.type = ClassicAudioCommandType::stop_song,
		.cue = ClassicMusicCue::high_score,
	});
	auto completion = scene->take_completion();
	expect(completion && completion->choice() == ContinueChoice::accept &&
		completion->raster().palette_tone() == 0 &&
		!scene->take_completion(),
		"accepted Continue publishes only its choice and black raster once");
	if(completion) {
		auto invalid_gameover = ClassicMainlGameOverScene::create(
			std::move(*completion)
		);
		expect(!invalid_gameover && invalid_gameover.error().code ==
			ClassicMainlContinueErrorCode::invalid_context,
			"accepted Continue capability cannot start Game Over");
	}

	auto priority = create_scene(2U);
	if(priority) {
		advance_continue_fade(
			*priority, ClassicMainlContinuePhase::waiting_for_input
		);
		const auto both = priority->advance({.ok = true, .cancel = true});
		expect(both && both->choice_committed &&
			priority->selected_choice() == ContinueChoice::accept,
			"OK wins over simultaneous Cancel in donor branch order");
	}
}

void drain_gameover_initial_audio(ClassicMainlGameOverScene &scene)
{
	expect(scene.take_audio_command() == ClassicAudioCommand{
		.type = ClassicAudioCommandType::stop_song,
	}, "Game Over first stops any previous song");
	expect(scene.take_audio_command() == ClassicAudioCommand{
		.type = ClassicAudioCommandType::load_song,
		.cue = ClassicMusicCue::game_over,
	}, "Game Over loads OVER.M after stopping");
	expect(scene.take_audio_command() == ClassicAudioCommand{
		.type = ClassicAudioCommandType::play_song,
		.cue = ClassicMusicCue::game_over,
	}, "Game Over starts OVER.M after loading");
	expect(!scene.take_audio_command(), "Game Over initial audio queue drains exactly");
}

void advance_gameover_fade(
	ClassicMainlGameOverScene &scene,
	ClassicAudioStatus audio,
	ClassicMainlGameOverPhase expected_after
)
{
	for(std::size_t boundary = 1U; boundary <= 18U; ++boundary) {
		const auto tick = scene.advance(audio);
		expect(tick && tick->boundary == ClassicMainlGameOverBoundary::vsync,
			"Game Over palette fade consumes one exact VSync boundary");
	}
	expect(scene.phase() == expected_after,
		"Game Over palette fade reaches the exact next phase");
}

void test_gameover_active_and_silent_waits()
{
	auto declined = finish_declined_continue(1U);
	auto gameover = ClassicMainlGameOverScene::create(std::move(declined));
	expect(gameover.has_value(), "declined Continue capability starts Game Over");
	if(!gameover) {
		return;
	}
	expect(!(*gameover)->advance({}) && (*gameover)->phase() ==
		ClassicMainlGameOverPhase::black_in,
		"Game Over cannot advance before stop/load/play drain");
	drain_gameover_initial_audio(**gameover);
	advance_gameover_fade(
		**gameover,
		{.bgm_active = true, .measure = 0U},
		ClassicMainlGameOverPhase::wait_measure_3
	);
	const auto busy = (*gameover)->advance({
		.bgm_active = false,
		.measure = 2U,
	});
	expect(busy && busy->boundary == ClassicMainlGameOverBoundary::audio_measure &&
		(*gameover)->phase() == ClassicMainlGameOverPhase::wait_measure_3,
		"active-on-entry measure wait remains active despite later status changes");
	const auto reached = (*gameover)->advance({.measure = 3U});
	expect(reached && reached->boundary == ClassicMainlGameOverBoundary::immediate &&
		(*gameover)->phase() == ClassicMainlGameOverPhase::black_out,
		"measure three ends the active PMD wait without a VSync");
	advance_gameover_fade(
		**gameover, {}, ClassicMainlGameOverPhase::ready
	);
	expect(!(*gameover)->take_completion(),
		"Game Over completion waits for its final stop command");
	expect((*gameover)->take_audio_command() == ClassicAudioCommand{
		.type = ClassicAudioCommandType::stop_song,
		.cue = ClassicMusicCue::game_over,
	}, "Game Over queues one final OVER.M stop");
	auto completion = (*gameover)->take_completion();
	expect(completion && completion->raster().palette_tone() == 0,
		"Game Over publishes the final black raster");

	auto zero = finish_declined_continue(0U);
	expect(zero.choice() == ContinueChoice::decline &&
		zero.raster().palette_tone() == 0,
		"zero-credit early return publishes only decline and black OVER");
	auto silent = ClassicMainlGameOverScene::create(std::move(zero));
	expect(silent.has_value(), "zero-credit capability also starts Game Over");
	if(!silent) {
		return;
	}
	drain_gameover_initial_audio(**silent);
	advance_gameover_fade(
		**silent, {}, ClassicMainlGameOverPhase::wait_measure_3
	);
	for(std::size_t frame = 1U; frame <= 63U; ++frame) {
		const auto wait = (*silent)->advance({
			.bgm_active = true,
			.measure = 99U,
		});
		expect(wait && wait->boundary == ClassicMainlGameOverBoundary::vsync &&
			(*silent)->phase() == ClassicMainlGameOverPhase::wait_measure_3,
			"inactive-on-entry Game Over uses the exact silent fallback");
	}
	const auto fallback_done = (*silent)->advance({
		.bgm_active = true,
		.measure = 99U,
	});
	expect(fallback_done && fallback_done->boundary ==
		ClassicMainlGameOverBoundary::vsync && (*silent)->phase() ==
		ClassicMainlGameOverPhase::black_out,
		"silent fallback ends on exactly frame 64 and ignores later PMD activity");
}

void test_transaction_rejections_and_lifetime()
{
	SyntheticFont font;
	auto resources = resources_for(2U);
	auto reordered = resources;
	std::swap(reordered[0U], reordered[1U]);
	auto rejected = ClassicMainlContinueTestAccess::create_with_synthetic_resources(
		2U, reordered, font
	);
	expect(!rejected && rejected.error().code ==
		ClassicMainlContinueErrorCode::resource_set_mismatch,
		"Continue transaction rejects reordered resources");
	auto missing = resources;
	missing.back().bytes.reset();
	rejected = ClassicMainlContinueTestAccess::create_with_synthetic_resources(
		2U, missing, font
	);
	expect(!rejected && rejected.error().code ==
		ClassicMainlContinueErrorCode::resource_bytes_missing,
		"Continue transaction rejects a late missing OVER resource");
	auto malformed_pi = resources;
	auto wrong_pi = std::make_shared<const std::vector<std::byte>>(
		make_pi(5646U, 5U, 1U, 320U)
	);
	malformed_pi[0U].bytes = wrong_pi;
	rejected = ClassicMainlContinueTestAccess::create_with_synthetic_resources(
		2U, malformed_pi, font
	);
	expect(!rejected && rejected.error().code ==
		ClassicMainlContinueErrorCode::invalid_asset_geometry,
		"old/wrong Continue PI geometry is rejected after decode");
	auto malformed_cdg = resources;
	auto wrong_cdg = std::make_shared<const std::vector<std::byte>>(
		make_choice_cdg(64U)
	);
	malformed_cdg[1U].bytes = wrong_cdg;
	rejected = ClassicMainlContinueTestAccess::create_with_synthetic_resources(
		2U, malformed_cdg, font
	);
	expect(!rejected && rejected.error().code ==
		ClassicMainlContinueErrorCode::invalid_asset_geometry,
		"wrong Continue CDG geometry is rejected after all four-cel decode");
	MissingFont missing_font;
	rejected = ClassicMainlContinueTestAccess::create_with_synthetic_resources(
		2U, resources, missing_font
	);
	expect(!rejected && rejected.error().code ==
		ClassicMainlContinueErrorCode::font_prepare_failed,
		"missing credit glyph fails before a scene is published");
	auto public_rejected = ClassicMainlContinueScene::create(
		2U, resources, font
	);
	expect(!public_rejected && public_rejected.error().code ==
		ClassicMainlContinueErrorCode::resource_integrity_failed,
		"production Continue scene reauthenticates supplied bytes");
	rejected = ClassicMainlContinueTestAccess::create_with_synthetic_resources(
		4U, resources, font
	);
	expect(!rejected && rejected.error().code ==
		ClassicMainlContinueErrorCode::invalid_context,
		"credit value four is rejected before resource decode");
	auto lifetime_resources = resources_for(3U);
	auto scene = ClassicMainlContinueTestAccess::create_with_synthetic_resources(
		3U, lifetime_resources, font
	);
	lifetime_resources.clear();
	expect(scene && (*scene)->advance({}).has_value(),
		"prepared Continue scene outlives caller resource and font owners");
}

void local_original_gate(const std::filesystem::path &content_root)
{
	SyntheticFont font;
	th03::app::MainResourceLoader loader(content_root);
	for(const auto credits : {std::uint8_t{0U}, std::uint8_t{1U},
		std::uint8_t{2U}, std::uint8_t{3U}}) {
		const auto resources = loader.load_mainl_continue(credits);
		expect(resources.has_value(),
			"local original Continue closure authenticates");
		if(!resources) {
			continue;
		}
		auto scene = ClassicMainlContinueScene::create(
			credits, *resources, font
		);
		expect(scene.has_value(),
			"local original Continue assets decode and prepare");
	}
}

} // namespace

int main(int argc, char **argv)
{
	test_continue_input_and_acceptance();
	test_gameover_active_and_silent_waits();
	test_transaction_rejections_and_lifetime();
	if(argc == 2) {
		local_original_gate(argv[1]);
	} else if(argc != 1) {
		std::cerr << "usage: test_app_classic_mainl_continue [content-root]\n";
		return 2;
	}

	if(failures != 0U) {
		std::cerr << failures << " of " << checks << " checks failed\n";
		return 1;
	}
	std::cout << checks << " checks passed\n";
	return 0;
}

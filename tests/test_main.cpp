#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <iostream>
#include <limits>
#include <span>
#include <string>
#include <variant>
#include <vector>

#include "th03/core/numeric.hpp"
#include "th03/formats/t3case.hpp"
#include "th03/formats/enedat.hpp"
#include "th03/formats/t3split.hpp"
#include "th03/formats/v11.hpp"
#include "game_internal.hpp"
#include "trig.hpp"

namespace {

int failures = 0;
int checks = 0;

void check(bool condition, const std::string &message)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << message << '\n';
	}
}

void put_u8(std::vector<std::byte> &bytes, std::size_t offset, std::uint8_t value)
{
	bytes[offset] = static_cast<std::byte>(value);
}

void put_u16(std::vector<std::byte> &bytes, std::size_t offset, std::uint16_t value)
{
	put_u8(bytes, offset, static_cast<std::uint8_t>(value));
	put_u8(bytes, offset + 1U, static_cast<std::uint8_t>(value >> 8U));
}

void put_u32(std::vector<std::byte> &bytes, std::size_t offset, std::uint32_t value)
{
	put_u8(bytes, offset, static_cast<std::uint8_t>(value));
	put_u8(bytes, offset + 1U, static_cast<std::uint8_t>(value >> 8U));
	put_u8(bytes, offset + 2U, static_cast<std::uint8_t>(value >> 16U));
	put_u8(bytes, offset + 3U, static_cast<std::uint8_t>(value >> 24U));
}

std::uint32_t get_u32(const std::vector<std::byte> &bytes, std::size_t offset)
{
	return (
		std::to_integer<std::uint32_t>(bytes[offset]) |
		(std::to_integer<std::uint32_t>(bytes[offset + 1U]) << 8U) |
		(std::to_integer<std::uint32_t>(bytes[offset + 2U]) << 16U) |
		(std::to_integer<std::uint32_t>(bytes[offset + 3U]) << 24U)
	);
}

void put_magic(
	std::vector<std::byte> &bytes,
	std::size_t offset,
	std::string_view magic
)
{
	for(std::size_t index = 0; index < magic.size(); ++index) {
		put_u8(bytes, offset + index, static_cast<std::uint8_t>(magic[index]));
	}
}

void refresh_t3case(std::vector<std::byte> &bytes)
{
	const auto payload_offset = get_u32(bytes, 16U);
	const auto payload_size = get_u32(bytes, 20U);
	put_u32(
		bytes,
		52U,
		th03::formats::fnv1a32(
			std::span<const std::byte>(bytes).subspan(payload_offset, payload_size)
		)
	);
	put_u32(bytes, 56U, 0U);
	put_u32(
		bytes,
		56U,
		th03::formats::fnv1a32(
			std::span<const std::byte>(bytes).first(payload_offset)
		)
	);
}

std::vector<std::byte> make_t3case(bool normalized)
{
	constexpr std::size_t record_count = 2;
	const auto payload_offset = static_cast<std::uint32_t>(
		128U + (normalized ? th03::formats::t3case_v11_snapshot_size : 0U)
	);
	const auto payload_size = static_cast<std::uint32_t>(
		record_count * th03::formats::t3case_record_size
	);
	std::vector<std::byte> bytes(payload_offset + payload_size, std::byte{0});
	put_magic(bytes, 0U, std::string_view("T3CASE1\0", 8));
	put_u16(bytes, 8U, 1U);
	put_u16(bytes, 10U, 64U);
	put_u16(bytes, 12U, 64U);
	put_u16(bytes, 14U, 20U);
	put_u32(bytes, 16U, payload_offset);
	put_u32(bytes, 20U, payload_size);
	put_u32(bytes, 24U, 1U);
	put_u32(bytes, 28U, static_cast<std::uint32_t>(record_count));
	put_u8(bytes, 32U, normalized ? 2U : 1U);
	put_u8(bytes, 33U, 1U);
	put_u8(bytes, 34U, 1U);
	put_u8(bytes, 35U, 1U);
	put_u8(bytes, 36U, 1U);
	put_u8(bytes, 37U, 3U);
	put_u16(bytes, 38U, normalized ? 0x0008U : 0U);
	put_u32(bytes, 40U, 42U);
	put_u32(bytes, 60U, static_cast<std::uint32_t>(bytes.size()));

	put_u32(bytes, 64U, 29U);
	put_u32(bytes, 68U, 0x89ABCDEFU);
	put_u8(bytes, 72U, 1U);
	put_u8(bytes, 73U, 2U);
	put_u8(bytes, 80U, 1U);
	put_u8(bytes, 81U, 1U);
	put_u8(bytes, 82U, 1U);
	put_u8(bytes, 83U, 4U);
	for(std::size_t stage = 0; stage < 9U; ++stage) {
		put_u8(bytes, 84U + stage, 0xFFU);
	}
	put_u8(bytes, 93U, 0xFFU);

	if(normalized) {
		constexpr std::size_t snapshot = 128U;
		put_u32(bytes, snapshot, 0x89ABCDEFU);
		for(std::size_t index = 0; index < 256U; ++index) {
			put_u8(bytes, snapshot + 4U + index, static_cast<std::uint8_t>(index));
			put_u8(bytes, snapshot + 261U + index, static_cast<std::uint8_t>(index + 1U));
			put_u8(bytes, snapshot + 517U + index, static_cast<std::uint8_t>(index + 2U));
		}
		put_u8(bytes, snapshot + 260U, 7U);
		put_u16(bytes, snapshot + 905U, 0x1234U);
		put_u16(bytes, snapshot + 907U, 0x5678U);
	}

	const auto input = static_cast<std::size_t>(payload_offset);
	put_u8(bytes, input, 1U);
	put_u8(bytes, input + 1U, 0U);
	put_u32(bytes, input + 4U, 0U);
	put_u16(bytes, input + 12U, 0x0020U);
	const auto control = input + th03::formats::t3case_record_size;
	put_u8(bytes, control, 2U);
	put_u8(bytes, control + 1U, 2U);
	put_u32(bytes, control + 4U, 1U);
	put_u16(bytes, control + 18U, 1U);
	refresh_t3case(bytes);
	return bytes;
}

std::vector<std::byte> make_v11()
{
	constexpr std::size_t input_offset = 400U + (9U * 994U);
	constexpr std::size_t input_size = 11U;
	std::vector<std::byte> bytes(input_offset + input_size, std::byte{0});
	put_magic(bytes, 0U, "T3RPLY11");
	put_u16(bytes, 8U, 11U);
	put_u16(bytes, 10U, 400U);
	put_u16(bytes, 12U, 0U);
	put_u16(bytes, 14U, 0x0003U);
	put_u8(bytes, 18U, 1U);
	put_u8(bytes, 19U, 2U);
	put_u8(bytes, 20U, 0U);
	put_u8(bytes, 21U, 1U);
	put_u8(bytes, 22U, 4U);
	put_u8(bytes, 23U, 0U);
	put_u8(bytes, 24U, 1U);
	put_u8(bytes, 25U, 1U);
	put_u32(bytes, 26U, 2U);
	put_u32(bytes, 34U, 29U);
	put_u32(bytes, 38U, 0x89ABCDEFU);
	put_u32(bytes, 42U, static_cast<std::uint32_t>(input_offset));
	put_u32(bytes, 46U, static_cast<std::uint32_t>(input_size));
	put_u32(bytes, 50U, 400U);
	put_u32(bytes, 54U, 994U);
	put_u16(bytes, 58U, 1U);
	put_u8(bytes, 67U, 1U);
	put_u8(bytes, 117U, 0U);

	constexpr std::size_t snapshot = 412U;
	put_u32(bytes, snapshot + 0U, 29U);
	put_u32(bytes, snapshot + 4U, 0x89ABCDEFU);
	put_u8(bytes, snapshot + 8U, 2U);
	put_u8(bytes, snapshot + 9U, 0U);
	put_u8(bytes, snapshot + 10U, 1U);
	put_u8(bytes, snapshot + 11U, 0U);
	put_u8(bytes, snapshot + 19U, 1U);
	put_u8(bytes, snapshot + 20U, 1U);
	put_u8(bytes, snapshot + 21U, 1U);
	put_u8(bytes, snapshot + 22U, 4U);
	for(std::size_t stage = 0; stage < 9U; ++stage) {
		put_u8(bytes, snapshot + 23U + stage, 0xFFU);
	}
	put_u8(bytes, snapshot + 48U, 7U);
	put_u8(bytes, snapshot + 53U, 0U);
	for(std::size_t index = 0; index < 256U; ++index) {
		put_u8(bytes, snapshot + 55U + index, static_cast<std::uint8_t>(index));
		put_u8(bytes, snapshot + 311U + index, static_cast<std::uint8_t>(index + 1U));
		put_u8(bytes, snapshot + 567U + index, static_cast<std::uint8_t>(index + 2U));
	}
	put_u16(bytes, snapshot + 951U, 0x1234U);
	put_u16(bytes, snapshot + 953U, 0x5678U);

	put_u8(bytes, input_offset + 0U, 0x01U); // gameplay, run 2
	put_u8(bytes, input_offset + 1U, 0x0FU); // full keyframe
	put_u16(bytes, input_offset + 2U, 0x0020U);
	put_u16(bytes, input_offset + 4U, 0x0040U);
	put_u16(bytes, input_offset + 6U, 0x1000U);
	put_u8(bytes, input_offset + 8U, 0U);
	put_u8(bytes, input_offset + 9U, 0x80U);
	put_u8(bytes, input_offset + 10U, 0xA5U);
	return bytes;
}

th03::formats::SplitRow split_row(
	th03::formats::SplitEvent event,
	std::uint32_t global_frame,
	std::uint32_t state_hash
)
{
	th03::formats::SplitRow row{};
	row.event = event;
	row.global_frame = global_frame;
	row.round_frame = global_frame;
	row.round_or_result_frame = static_cast<std::uint16_t>(global_frame);
	row.game_mode = 1U;
	row.resident_rand = 29;
	row.round_speed = 96U;
	row.state_hash = state_hash;
	return row;
}

void test_numeric()
{
	using namespace th03::exact;
	check(wrap_add<std::int16_t>(32767, 1) == std::numeric_limits<std::int16_t>::min(), "signed 16-bit addition wraps explicitly");
	check(wrap_subtract<std::uint8_t>(0U, 1U) == 255U, "unsigned 8-bit subtraction wraps");
	check(wrap_negate<std::int8_t>(std::numeric_limits<std::int8_t>::min()) == std::numeric_limits<std::int8_t>::min(), "signed 8-bit negation wraps");
	check(arithmetic_shift_right<std::int16_t>(-17, 4U) == -2, "signed right shift is arithmetic");
	check(Subpixel::from_integer_wrapping(12).raw() == 192, "Q12.4 integer conversion");
	check(Subpixel::from_raw(-17).to_integer_floor() == -2, "Q12.4 pixel conversion floors through arithmetic shift");
	check(Subpixel::from_raw(-17).to_integer_trunc() == -1, "Q12.4 slow conversion truncates toward zero");
	check(SubpixelDelta8::from_integer_wrapping(8).raw_bits() == 0x80U, "signed Q4.4 construction preserves raw wrap");
	check(DecimalSubpixel::from_raw(-19).to_integer_trunc() == -1, "decimal fixed point truncates toward zero");
}

void test_th3r03_round_start()
{
	th03::GameInit init{};
	init.resident_rand = 2638;
	init.random_seed = 0xE9B35FBAU;
	init.game_mode = 1U;
	init.rank = 3U;
	init.story_lives = 2U;
	init.remaining_credits = 3U;
	init.skill = 145U;
	init.is_cpu = {0U, 1U};
	init.paletted_character = {1, 11};
	init.autofire_mask = 1U;
	th03::RuntimeSnapshot snapshot{};
	snapshot.random_seed = 0xE9B35FBAU;
	for(auto &player : snapshot.players) {
		player.center_x = th03::exact::Subpixel::from_raw(2304);
		player.center_y = th03::exact::Subpixel::from_raw(5376);
		player.halfhearts = 10U;
		player.invincibility_time = 50U;
		player.gauge_charge_speed = 32U;
		player.gauge_avail = 1024U;
		player.bombs = 2U;
		player.shot_active = 1U;
	}
	snapshot.cpu_charge_ring[1][0] = 191U;
	init.runtime_snapshot = snapshot;

	auto game = th03::Game::create(init);
	check(game.has_value(), "TH3R03 first-round state initializes");
	const auto initialized = th03::detail::initialize_first_round(init);
	check(
		initialized.boss_initial[1].hp == 0x0064U,
		"TH3R03 P2 Kana boss HP excludes the adjacent palette sprite offset"
	);
	if(game) {
		const auto wrong_context = game->step(
			th03::CoreInput{}, th03::InputContext::interstitial
		);
		check(
			wrong_context.status == th03::StepStatus::invalid_input_context,
			"TH3R03 rejects interstitial input outside a warning wait"
		);
		const auto state = game->canonical_state();
		check(state.round_speed == 96U, "TH3R03 Lunatic round speed starts at raw Q4.4 96");
		check(state.state_hash == 0x5BDCE01AU, "TH3R03 native start state matches Anniversary schema-3 hash");
		th03::StepResult boundary{};
		for(std::uint32_t frame = 0; frame < 128U; ++frame) {
			th03::CoreInput input{0x0020U, 0U, 0U, 0U};
			if(frame <= 34U) {
				input.charge_mask = 1U;
			}
			if((frame >= 10U && frame <= 14U) ||
				(frame >= 37U && frame <= 85U)) {
				input.p1 = 0x0021U;
			} else if(frame >= 86U && frame <= 126U) {
				input.p1 = 0x0022U;
			}
			boundary = game->step(input);
		}
		check(boundary.status == th03::StepStatus::running,
			"TH3R03 native prefix advances through sample 128");
		check(boundary.input_boundary_state.state_hash == 0xC35124AFU,
			"TH3R03 sample-128 boundary matches Anniversary schema-3 hash");
	}
}

void test_legacy_player_hittest_alias()
{
	th03::detail::DeterministicState state{};
	state.players[1].center_x = th03::exact::Subpixel::from_raw(0x0420);
	state.players[1].center_y = th03::exact::Subpixel::from_raw(0x1120);
	std::int16_t collision_x = -1;
	std::int16_t collision_y = -1;
	check(
		th03::detail::player_hittest_legacy(
			state, 1U, 80U, collision_x, collision_y
		),
		"TH03 broad CPU probe preserves the wrapped DGROUP collision"
	);
	check(
		collision_x == 0 && collision_y == 0x0C20,
		"TH03 wrapped collision reports the first nonnegative map column"
	);
}

void test_hyper_callback_transition_delay()
{
	th03::detail::DeterministicState state{};
	auto &player = state.players[1];
	player.is_cpu = 1U;
	player.paletted_character = 11;
	player.center_x = th03::exact::Subpixel::from_integer_wrapping(200);
	player.center_y = th03::exact::Subpixel::from_integer_wrapping(300);
	player.speed_aligned_x = 56U;
	player.speed_aligned_y = 56U;
	player.speed_diagonal_x = 44U;
	player.speed_diagonal_y = 44U;
	player.hyper_active = 11U;
	player.gauge_avail = 128U << 4U;
	player.shot_active = 0U;

	(void)th03::detail::gameplay_frame_suffix_before_interstitial(state);
	check(
		player.center_x.raw() == ((200 << 4) - 56),
		"Hyper standby switches callbacks without applying the speed boost"
	);
	(void)th03::detail::gameplay_frame_suffix_before_interstitial(state);
	check(
		player.center_x.raw() == ((200 << 4) - 56 - 88),
		"Hyper character callback applies its speed boost on the following update"
	);
}

void test_story_result_route()
{
	th03::detail::DeterministicState state{};
	state.game_mode = 1U;
	state.winner = 0;
	state.defeat_flag = th03::detail::DefeatFlag::banner;
	state.background_phase = th03::detail::BackgroundPhase::type_a_steady;
	state.players[0].combo_hits_max = 5U;
	state.players[0].gauge_attacks_fired = 1U;
	state.players[0].boss_attacks_fired = 2U;
	state.players[0].boss_attacks_reversed = 3U;
	state.players[0].boss_panics_fired = 4U;

	th03::detail::gameplay_frame_suffix_after_interstitial(state);
	check(state.round_or_result_frame == 1U,
		"Story result frame zero advances without initializing the bonus");
	th03::detail::gameplay_frame_suffix_after_interstitial(state);
	check(
		state.result_bonus_remaining == (225000U - 1171U),
		"Story result frame one snapshots counters and adds one 1/192 bonus slice"
	);
	while(state.round_or_result_frame < 320U) {
		th03::detail::gameplay_frame_suffix_after_interstitial(state);
	}
	check(
		state.score[0] == std::array<std::uint8_t, 8>{0U, 0U, 0U, 5U, 2U, 2U, 0U, 0U},
		"Story winner bonus drains exactly into the decimal score"
	);
	for(std::size_t frame = 0U; frame < 16U; ++frame) {
		th03::detail::gameplay_frame_suffix_after_interstitial(state);
	}
	check(
		state.round_or_result_frame == 336U &&
		state.result_phase == th03::detail::ResultPhase::closing &&
		state.result_transition_complete,
		"Story result opening completes after exactly sixteen callbacks"
	);
	th03::detail::gameplay_frame_suffix_after_interstitial(state);
	check(
		state.route == 2U && state.round_or_result_frame == 336U,
		"Story P1 win returns route 2 before another gameplay-frame increment"
	);
}

void test_story_extends()
{
	th03::GameInit init{};
	init.game_mode = 1U;
	init.story_lives = 2U;
	init.last_score[0][6] = 1U;
	auto state = th03::detail::initialize_first_round(init);
	check(
		state.story_lives == 2U && state.extends_gained == 1U,
		"Story round derives its extend counter from the ten-million score digit"
	);
	state.background_phase = th03::detail::BackgroundPhase::type_a_steady;
	state.score[0][6] = 2U;
	th03::detail::gameplay_frame_suffix_after_interstitial(state);
	check(
		state.story_lives == 3U && state.extends_gained == 255U,
		"Story score rendering awards the second extend and disables further awards"
	);
	state.score[0][6] = 3U;
	th03::detail::gameplay_frame_suffix_after_interstitial(state);
	check(
		state.story_lives == 3U && state.extends_gained == 255U,
		"Disabled Story extends remain disabled above twenty million points"
	);
}

void test_random()
{
	std::uint32_t seed = 0xE9B35FBAU;
	check(th03::detail::irand(seed) == 0x6AB1U && seed == 0x6AB17D83U,
		"IRAND first known answer matches master.lib");
	check(th03::detail::irand(seed) == 0x4E0AU && seed == 0xCE0AE620U,
		"IRAND high bit is masked only from its return value");

	th03::detail::DeterministicState state{};
	state.random_seed = 0xE9B35FBAU;
	th03::detail::randring_fill(state);
	check(state.random_seed == 0x21D09EBAU,
		"descending 256-byte randring fill consumes the exact RNG sequence");
	check(
		state.randring[255] == 0xB1U && state.randring[254] == 0x0AU &&
		state.randring[1] == 0xC5U && state.randring[0] == 0xD0U,
		"randring fill stores low return bytes from index 255 down to zero"
	);

	state.randring[254] = 0x34U;
	state.randring[255] = 0x12U;
	state.randring_cursor = 254U;
	check(th03::detail::randring_next16(state) == 0x1234U &&
		state.randring_cursor == 255U,
		"randring 16-bit read is little-endian and advances once");
	state.randring[255] = 0xABU;
	check(th03::detail::randring_next16(state) == 0x00ABU &&
		state.randring_cursor == 0U,
		"randring index 255 reproduces the adjacent wrapped cursor byte");
}

void test_trig()
{
	check(th03::detail::cosine8(0U) == 256 &&
		th03::detail::sine8(0U) == 0,
		"master.lib integer trig begins at (256, 0)");
	check(th03::detail::cosine8(64U) == 0 &&
		th03::detail::sine8(64U) == 256,
		"master.lib integer trig preserves the 90-degree cardinal");
	check(th03::detail::cosine8(129U) == -256 &&
		th03::detail::sine8(193U) == -256,
		"master.lib integer trig preserves repeated negative extrema");
	const auto negative = th03::detail::vector2(128U, 1);
	check(negative.x == -1 && negative.y == 0,
		"vector2 arithmetic shift preserves negative rounding quirk");
	check(
		th03::detail::iatan2(0, 1) == 0U &&
		th03::detail::iatan2(1, 0) == 64U &&
		th03::detail::iatan2(0, -1) == 128U &&
		th03::detail::iatan2(-1, 0) == 192U,
		"master.lib iatan2 preserves cardinal angles"
	);
	check(
		th03::detail::iatan2(1, 1) == 32U &&
		th03::detail::iatan2(-1, -1) == 160U,
		"master.lib iatan2 preserves diagonal quadrants"
	);
}

void test_t3case()
{
	const std::array<std::byte, 1> a{std::byte{'a'}};
	check(th03::formats::fnv1a32({}) == 0x811C9DC5U, "FNV-1a empty known answer");
	check(th03::formats::fnv1a32(a) == 0xE40C292CU, "FNV-1a 'a' known answer");

	auto direct = make_t3case(false);
	const auto parsed = th03::formats::parse_t3case(direct);
	check(parsed.has_value(), "direct T3CASE parses");
	if(parsed) {
		check(parsed->header.sample_count == 1U && parsed->records.size() == 2U, "direct T3CASE counts");
		const auto session = th03::formats::make_replay_session(*parsed);
		check(session && session->records.size() == 2U, "direct T3CASE adapts to neutral replay session");
	}

	auto normalized = make_t3case(true);
	const auto normalized_parsed = th03::formats::parse_t3case(normalized);
	check(normalized_parsed.has_value(), "normalized V11 T3CASE parses");
	if(normalized_parsed && normalized_parsed->init.runtime_snapshot) {
		check(normalized_parsed->init.random_seed == 0x89ABCDEFU, "high-bit seed remains unsigned bits");
		check(normalized_parsed->init.runtime_snapshot->players[0].center_x.raw_bits() == 0x1234U, "snapshot Q12.4 position preserves raw bits");
	} else {
		check(false, "normalized T3CASE exposes runtime snapshot");
	}

	auto bad = direct;
	put_u8(bad, 0U, 'X');
	check(!th03::formats::parse_t3case(bad), "T3CASE rejects bad magic");
	bad = direct;
	put_u16(bad, 8U, 2U);
	refresh_t3case(bad);
	check(!th03::formats::parse_t3case(bad), "T3CASE rejects unsupported version");
	bad = direct;
	put_u16(bad, 38U, 0x0004U);
	refresh_t3case(bad);
	check(!th03::formats::parse_t3case(bad), "T3CASE rejects retired flag bit");
	bad = direct;
	put_u8(bad, 32U, 3U);
	refresh_t3case(bad);
	check(!th03::formats::parse_t3case(bad), "T3CASE rejects reserved V12 source kind");
	bad = direct;
	put_u8(bad, 64U + 52U, 1U);
	refresh_t3case(bad);
	check(!th03::formats::parse_t3case(bad), "T3CASE rejects nonzero startup reserved byte");
	bad = direct;
	put_u8(bad, 35U, 2U);
	refresh_t3case(bad);
	check(!th03::formats::parse_t3case(bad), "T3CASE rejects scenario/startup mismatch");
	bad = direct;
	put_u8(bad, 128U + 1U, 2U);
	refresh_t3case(bad);
	check(!th03::formats::parse_t3case(bad), "T3CASE rejects input with control phase");
	bad = direct;
	put_u32(bad, 128U + 4U, 1U);
	refresh_t3case(bad);
	check(!th03::formats::parse_t3case(bad), "T3CASE rejects non-dense frame index");
	bad = direct;
	put_u16(bad, 128U + 18U, 1U);
	refresh_t3case(bad);
	check(!th03::formats::parse_t3case(bad), "T3CASE rejects Charge without flag");
	bad = direct;
	put_u16(bad, 128U + 20U + 18U, 2U);
	refresh_t3case(bad);
	check(!th03::formats::parse_t3case(bad), "T3CASE rejects wrong first process control");
	bad = direct;
	bad.push_back(std::byte{0});
	check(!th03::formats::parse_t3case(bad), "T3CASE rejects trailing data");
	bad = direct;
	put_u8(bad, 128U + 12U, 0x21U);
	check(!th03::formats::parse_t3case(bad), "T3CASE rejects payload checksum mismatch");
	bad = direct;
	put_u8(bad, 64U, 30U);
	check(!th03::formats::parse_t3case(bad), "T3CASE rejects header checksum mismatch");
}

void test_enedat()
{
	std::vector<std::byte> bytes(10U, std::byte{0});
	put_u16(bytes, 0U, 6U);
	put_u16(bytes, 4U, 1U);
	put_u8(bytes, 6U, 1U);
	put_u8(bytes, 7U, 0U);
	put_u16(bytes, 8U, 0U);
	const auto parsed = th03::formats::parse_enedat(bytes);
	check(parsed && parsed->enemy_formations.size() == 1U,
		"ENEDAT parser exposes semantic formations");
	if(parsed) {
		check(parsed->enemy_formations[0].enemy_scripts[0][0] == 0U,
			"ENEDAT parser preserves script opcodes as owned bytes");
	}

	auto bad = bytes;
	put_u16(bad, 0U, 5U);
	check(!th03::formats::parse_enedat(bad),
		"ENEDAT rejects a header/payload size mismatch");
	bad = bytes;
	put_u16(bad, 4U, 17U);
	check(!th03::formats::parse_enedat(bad),
		"ENEDAT rejects the donor's unsafe over-16 formation count");
	bad = bytes;
	bad.push_back(std::byte{0});
	put_u16(bad, 0U, 7U);
	check(!th03::formats::parse_enedat(bad),
		"ENEDAT rejects trailing bytes after its terminator");
}

void test_t3split()
{
	th03::formats::SplitStream base{};
	base.state_hash_schema = 3U;
	base.rows = {
		split_row(th03::formats::SplitEvent::start, 0U, 0x11111111U),
		split_row(th03::formats::SplitEvent::checkpoint, 128U, 0x22222222U),
		split_row(th03::formats::SplitEvent::route, 200U, 0x33333333U),
		split_row(th03::formats::SplitEvent::finish, 200U, 0x33333333U),
	};
	const auto encoded_result = th03::formats::write_t3split(base);
	check(encoded_result.has_value(), "T3SPLIT writer accepts valid stream");
	if(!encoded_result) {
		return;
	}
	const auto &encoded = *encoded_result;
	const auto parsed = th03::formats::parse_t3split(encoded);
	check(parsed && parsed->rows.size() == 4U && parsed->state_hash_schema == 3U, "T3SPLIT round trip");

	auto candidate = base;
	candidate.rows.insert(candidate.rows.begin() + 1, split_row(th03::formats::SplitEvent::round_start, 0U, 0x11111111U));
	candidate.rows[2].global_frame = 127U;
	candidate.rows[3].state_hash = 0xAAAAAAAAU;
	candidate.rows[4].state_hash = 0xAAAAAAAAU;
	const auto same = th03::formats::compare_oracle_splits(base, candidate);
	check(same.status == th03::formats::ComparisonStatus::same, "oracle comparator calibrates duplicate start and checkpoint label delta");
	check(same.checkpoint_global_delta == -1, "oracle comparator reports constant checkpoint delta");
	check(same.terminal_hash_differences.size() == 2U, "oracle comparator preserves terminal hash diagnostics");

	auto bad_hash = base;
	bad_hash.rows[1].state_hash = 0xDEADBEEFU;
	check(th03::formats::compare_oracle_splits(base, bad_hash).status == th03::formats::ComparisonStatus::different, "oracle comparator rejects checkpoint hash mismatch");
	auto prefix = base;
	prefix.rows.resize(2U);
	check(th03::formats::compare_oracle_splits(base, prefix, true).status == th03::formats::ComparisonStatus::same_prefix, "oracle comparator supports explicit candidate prefix");
	auto wrong_schema = base;
	wrong_schema.state_hash_schema = 0U;
	check(th03::formats::compare_oracle_splits(base, wrong_schema).status == th03::formats::ComparisonStatus::different, "oracle comparator rejects state-hash schema mismatch");
	auto unwritable = base;
	unwritable.state_hash_schema = 2U;
	check(!th03::formats::write_t3split(unwritable), "T3SPLIT writer rejects unsupported schema");

	auto bad = encoded;
	put_u8(bad, 0U, 'X');
	check(!th03::formats::parse_t3split(bad), "T3SPLIT rejects bad magic");
	bad = encoded;
	put_u16(bad, 14U, 2U);
	check(!th03::formats::parse_t3split(bad), "T3SPLIT rejects unsupported schema");
	bad = encoded;
	bad.pop_back();
	check(!th03::formats::parse_t3split(bad), "T3SPLIT rejects truncated row");
	bad = encoded;
	put_u8(bad, 16U + 7U, 1U);
	check(!th03::formats::parse_t3split(bad), "T3SPLIT rejects nonzero reserved byte");
	bad = encoded;
	put_u8(bad, 16U + 18U, 0xFAU);
	check(!th03::formats::parse_t3split(bad), "T3SPLIT rejects non-decimal packed score");
}

void test_v11()
{
	auto bytes = make_v11();
	const auto parsed = th03::formats::parse_v11_replay(bytes);
	check(parsed.has_value(), "synthetic V11 replay parses");
	if(parsed) {
		check(parsed->sample_count == 2U && parsed->records.size() == 3U, "V11 RLE expands samples and process boundary");
		check(parsed->init.runtime_snapshot.has_value(), "V11 produces explicit deterministic runtime snapshot");
		if(parsed->init.runtime_snapshot) {
			check(parsed->init.runtime_snapshot->players[0].center_x.raw_bits() == 0x1234U, "V11 fixed-point player position preserves raw bits");
		}
		const auto *sample = std::get_if<th03::replay::InputSample>(&parsed->records[0]);
		check(sample && sample->input.p1 == 0x0020U && sample->input.system == 0x1000U, "V11 neutral session preserves logical input");
	}

	auto bad = bytes;
	put_u8(bad, 0U, 'X');
	check(!th03::formats::parse_v11_replay(bad), "V11 rejects bad magic");
	bad = bytes;
	put_u16(bad, 14U, 0x0007U);
	check(!th03::formats::parse_v11_replay(bad), "V11 rejects Practice replay");
	bad = bytes;
	put_u8(bad, 412U + 8U, 3U);
	check(!th03::formats::parse_v11_replay(bad), "V11 rejects header/checkpoint mismatch");
	bad = bytes;
	constexpr std::size_t input = 400U + (9U * 994U);
	put_u8(bad, input + 1U, 0x07U);
	check(!th03::formats::parse_v11_replay(bad), "V11 rejects missing first Charge keyframe");
	bad = bytes;
	put_u8(bad, input + 10U, 0U);
	check(!th03::formats::parse_v11_replay(bad), "V11 rejects bad process marker");
	bad = bytes;
	put_u32(bad, 46U, 9U);
	check(!th03::formats::parse_v11_replay(bad), "V11 rejects stream without terminal process control");
	bad = bytes;
	put_u32(bad, 26U, 1U);
	check(!th03::formats::parse_v11_replay(bad), "V11 rejects RLE expansion past sample count");
}

} // namespace

int main()
{
	test_numeric();
	test_th3r03_round_start();
	test_legacy_player_hittest_alias();
	test_hyper_callback_transition_delay();
	test_story_result_route();
	test_story_extends();
	test_random();
	test_trig();
	test_t3case();
	test_enedat();
	test_t3split();
	test_v11();
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0 ? 0 : 1;
}

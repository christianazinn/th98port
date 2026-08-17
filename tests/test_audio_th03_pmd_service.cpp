#include "th03/audio/pmd_opna.hpp"
#include "th03/audio/th03_pmd_service.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <utility>
#include <vector>

namespace {

using th03::audio::PmdOpnaConfig;
using th03::audio::PmdOpnaEngine;
using th03::audio::PmdOpnaErrorCode;
using th03::audio::PmdOpnaFidelity;
using th03::audio::PmdOpnaStereoSample;
using th03::audio::Th03PmdServiceErrorCode;
using th03::audio::Th03SoundEffectRule;
using th03::audio::Th03SoundEffectScheduler;
using th03::audio::Th03SoundEffectSchedulerStatus;
using th03::audio::Th03SoundEffectStart;

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

void append(std::vector<std::byte> &bytes, std::uint8_t value)
{
	bytes.push_back(static_cast<std::byte>(value));
}

void set_le16(
	std::vector<std::byte> &bytes,
	std::size_t offset,
	std::uint16_t value
)
{
	bytes[offset] = static_cast<std::byte>(value);
	bytes[offset + 1U] = static_cast<std::byte>(value >> 8U);
}

std::vector<std::byte> synthetic_song()
{
	constexpr std::size_t file_prefix = 1U;
	constexpr std::size_t header_size = 0x18U;
	std::vector<std::byte> bytes(file_prefix + header_size, std::byte{0U});
	const auto emit = [&bytes](std::initializer_list<std::uint8_t> stream) {
		const auto offset = static_cast<std::uint16_t>(bytes.size() - 1U);
		for(const auto value : stream) {
			append(bytes, value);
		}
		return offset;
	};
	const auto fm1 = emit({
		0x24U, 0x30U, 0x28U, 0x30U, 0x2BU, 0x18U, 0x2FU, 0x18U, 0x80U,
	});
	const auto ssg1 = emit({0x40U, 0x18U, 0x43U, 0x18U, 0x80U});
	const auto empty = emit({0x80U});
	const auto rhythm_table = emit({0x80U});
	const std::array<std::uint16_t, 11U> parts{
		fm1, empty, empty, empty, empty, empty,
		ssg1, empty, empty, empty, empty,
	};
	for(std::size_t part = 0U; part < parts.size(); ++part) {
		set_le16(bytes, file_prefix + (part * 2U), parts[part]);
	}
	set_le16(bytes, file_prefix + 0x16U, rhythm_table);
	return bytes;
}

PmdOpnaEngine engine()
{
	auto created = PmdOpnaEngine::create(PmdOpnaConfig{
		.input_clock_hz = 7'987'200U,
		.fidelity = PmdOpnaFidelity::maximum,
	});
	expect(static_cast<bool>(created), "explicit synthetic engine is accepted");
	return std::move(*created);
}

template <typename Predicate>
bool render_until(PmdOpnaEngine &subject, Predicate predicate)
{
	std::array<PmdOpnaStereoSample, 1'024U> samples{};
	for(std::size_t attempt = 0U; attempt < 4'096U; ++attempt) {
		if(predicate(subject.status())) {
			return true;
		}
		subject.render_native(samples);
	}
	return predicate(subject.status());
}

bool render_until_volume_changes(
	PmdOpnaEngine &subject,
	std::uint8_t initial_volume
)
{
	std::array<PmdOpnaStereoSample, 1U> sample{};
	for(std::size_t attempt = 0U; attempt < 1'000'000U; ++attempt) {
		if(subject.status().fade_volume != initial_volume) {
			return true;
		}
		subject.render_native(sample);
	}
	return subject.status().fade_volume != initial_volume;
}

void test_effect_rule_table()
{
	constexpr std::array expected{
		Th03SoundEffectRule{0U, 0U},
		Th03SoundEffectRule{32U, 36U},
		Th03SoundEffectRule{16U, 16U},
		Th03SoundEffectRule{2U, 4U},
		Th03SoundEffectRule{18U, 16U},
		Th03SoundEffectRule{18U, 8U},
		Th03SoundEffectRule{18U, 10U},
		Th03SoundEffectRule{64U, 48U},
		Th03SoundEffectRule{16U, 80U},
		Th03SoundEffectRule{17U, 24U},
		Th03SoundEffectRule{2U, 17U},
		Th03SoundEffectRule{17U, 11U},
		Th03SoundEffectRule{32U, 4U},
		Th03SoundEffectRule{18U, 80U},
		Th03SoundEffectRule{18U, 16U},
		Th03SoundEffectRule{18U, 48U},
		Th03SoundEffectRule{18U, 74U},
		Th03SoundEffectRule{32U, 50U},
		Th03SoundEffectRule{32U, 32U},
		Th03SoundEffectRule{18U, 24U},
		Th03SoundEffectRule{18U, 68U},
		Th03SoundEffectRule{14U, 0U},
	};
	for(std::size_t index = 0U; index < expected.size(); ++index) {
		const auto id = static_cast<std::uint8_t>(index + 1U);
		const auto actual = th03::audio::th03_sound_effect_rule(id);
		expect(
			actual && *actual == expected[index],
			"every TH03 priority/hold entry matches the donor table"
		);
	}
	for(const auto invalid : {std::uint8_t{0U}, std::uint8_t{23U},
		std::uint8_t{0xFFU}}) {
		const auto rule = th03::audio::th03_sound_effect_rule(invalid);
		expect(
			!rule &&
				rule.error().code == Th03PmdServiceErrorCode::invalid_effect_id &&
				rule.error().effect_id == invalid,
			"out-of-table effect IDs are rejected without donor-style OOB access"
		);
	}
}

void test_effect_scheduler()
{
	Th03SoundEffectScheduler subject(false);
	const auto inactive = subject.request(2U);
	expect(inactive && !*inactive, "inactive source service ignores valid requests");
	expect(
		!subject.status().pending_effect && !subject.update(),
		"inactive requests do not publish or advance a pending slot"
	);
	subject.set_active(true);
	expect(subject.request(2U) == true, "first active request occupies the slot");
	expect(
		subject.update() == Th03SoundEffectStart{2U},
		"pending frame zero dispatches exactly once"
	);
	expect(
		subject.status().pending_frame == 1U,
		"the dispatch frame increments the donor byte counter"
	);
	const auto lower = subject.request(1U);
	expect(lower && !*lower, "lower-priority request cannot replace the slot");
	expect(
		subject.status().pending_effect == 2U &&
			subject.status().pending_frame == 1U,
		"rejected priority request preserves both pending ID and frame"
	);
	expect(subject.request(13U) == true, "equal priority replaces the pending ID");
	expect(
		subject.status().pending_effect == 13U &&
			subject.status().pending_frame == 0U,
		"priority replacement restarts at source frame zero"
	);
	expect(
		subject.update() == Th03SoundEffectStart{13U},
		"replacement dispatches on its next update"
	);
	for(std::uint8_t frame = 2U; frame <= 4U; ++frame) {
		expect(!subject.update(), "hold window does not redispatch its effect");
		expect(
			subject.status().pending_frame == frame,
			"hold window increments once per gameplay update"
		);
	}
	expect(!subject.update(), "the clear update has no second dispatch");
	expect(
		!subject.status().pending_effect && subject.status().pending_frame == 0U,
		"hold count 4 clears only after the fifth source update"
	);

	expect(subject.request(1U) == true, "zero-hold effect can be queued");
	expect(
		subject.update() == Th03SoundEffectStart{1U},
		"zero-hold effect dispatches"
	);
	expect(!subject.status().pending_effect, "zero-hold effect clears immediately");

	expect(subject.request(18U) == true, "force precondition occupies a slot");
	const auto forced = subject.force(22U);
	expect(
		forced && *forced == Th03SoundEffectStart{22U},
		"force is reset, request, immediate update"
	);
	expect(!subject.status().pending_effect, "forced zero-hold effect clears itself");

	expect(subject.request(2U) == true, "atomic-invalid fixture occupies a slot");
	static_cast<void>(subject.update());
	const auto before = subject.status();
	const auto invalid = subject.force(0U);
	expect(
		!invalid &&
			invalid.error().code == Th03PmdServiceErrorCode::invalid_effect_id,
		"invalid forced effect is rejected"
	);
	expect(
		subject.status().pending_effect == before.pending_effect &&
			subject.status().pending_frame == before.pending_frame,
		"invalid force validates before its destructive reset"
	);
	subject.set_active(false);
	expect(!subject.update(), "inactive update does not dispatch retained history");
	expect(
		subject.status().pending_frame == before.pending_frame,
		"inactive update does not age retained history"
	);
	subject.set_active(true);
	subject.reset();
	expect(
		!subject.status().pending_effect && subject.status().pending_frame == 0U,
		"explicit reset clears ID and frame"
	);

	const Th03SoundEffectSchedulerStatus restored{
		.active = false,
		.pending_effect = std::uint8_t{2U},
		.pending_frame = 17U,
	};
	expect(subject.restore(restored).has_value() &&
		subject.status().active == restored.active &&
		subject.status().pending_effect == restored.pending_effect &&
		subject.status().pending_frame == restored.pending_frame,
		"semantic scheduler state restores exactly");
	const auto before_invalid_restore = subject.status();
	expect(!subject.restore({
		.active = true,
		.pending_effect = std::uint8_t{2U},
		.pending_frame = 37U,
	}).has_value() &&
		subject.status().active == before_invalid_restore.active &&
		subject.status().pending_effect ==
			before_invalid_restore.pending_effect &&
		subject.status().pending_frame == before_invalid_restore.pending_frame,
		"invalid semantic scheduler restore is atomic");
	expect(!subject.restore({
		.active = true,
		.pending_frame = 1U,
	}).has_value(), "empty scheduler slot requires frame zero");
}

void test_effect_scheduler_exhaustive()
{
	for(std::uint8_t current = th03::audio::th03_sound_effect_first;
		current <= th03::audio::th03_sound_effect_last; ++current) {
		const auto current_rule = th03::audio::th03_sound_effect_rule(current);
		for(std::uint8_t requested = th03::audio::th03_sound_effect_first;
			requested <= th03::audio::th03_sound_effect_last; ++requested) {
			Th03SoundEffectScheduler subject;
			expect(subject.request(current) == true, "pair fixture queues current ID");
			const auto requested_rule =
				th03::audio::th03_sound_effect_rule(requested);
			const bool should_replace =
				current_rule->priority <= requested_rule->priority;
			const auto result = subject.request(requested);
			expect(
				result && *result == should_replace,
				"every ordered priority pair follows source <= replacement"
			);
			const auto status = subject.status();
			expect(
				status.pending_effect ==
					(should_replace ? requested : current),
				"every ordered priority pair retains the correct ID"
			);
			expect(
				status.pending_frame == 0U,
				"priority comparison alone preserves source frame zero"
			);
		}
	}

	for(std::uint8_t effect = th03::audio::th03_sound_effect_first;
		effect <= th03::audio::th03_sound_effect_last; ++effect) {
		Th03SoundEffectScheduler subject;
		const auto rule = th03::audio::th03_sound_effect_rule(effect);
		expect(subject.request(effect) == true, "hold fixture queues effect");
		expect(
			subject.update() == Th03SoundEffectStart{effect},
			"hold fixture dispatches at frame zero"
		);
		if(rule->hold_frames == 0U) {
			expect(!subject.status().pending_effect, "zero hold clears on dispatch");
			continue;
		}
		expect(
			subject.status().pending_frame == 1U,
			"positive hold survives its dispatch update"
		);
		for(std::uint8_t frame = 2U; frame <= rule->hold_frames; ++frame) {
			expect(!subject.update(), "hold interval never redispatches");
			expect(
				subject.status().pending_frame == frame,
				"every source hold interval preserves its exact inclusive bound"
			);
		}
		expect(!subject.update(), "post-hold clear update never dispatches");
		expect(
			!subject.status().pending_effect &&
				subject.status().pending_frame == 0U,
			"every source hold interval clears on hold plus one"
		);
	}
}

void test_transport_and_queries()
{
	auto subject = engine();
	subject.stop();
	subject.pause();
	subject.resume();
	subject.fade(16);
	subject.set_volume(91U);
	expect(
		!subject.status().playing && !subject.status().paused &&
			subject.status().fade_volume == 0U,
		"transport and volume commands before Load are bounded no-ops"
	);
	expect(
		subject.set_part_mask(0U, true).has_value() &&
			subject.status().part_mask == 0U,
		"valid pre-Load part mask is a bounded no-op"
	);
	const auto no_song = subject.play();
	expect(
		!no_song && no_song.error().code == PmdOpnaErrorCode::no_song_loaded,
		"Play before Load reports the typed no-song error"
	);
	const auto song = synthetic_song();
	expect(subject.load_song(song).has_value(), "generated PMD image loads");
	expect(subject.play().has_value(), "loaded image starts");
	auto status = subject.status();
	expect(
		status.playing && !status.paused && status.fade_volume == 0U &&
			status.fade_speed == 0 && status.part_mask == 0U,
		"fresh Play resets transport, volume, fade, and all masks"
	);

	for(const auto part : {std::uint8_t{0U}, std::uint8_t{10U},
		std::uint8_t{13U}, std::uint8_t{14U}, std::uint8_t{15U},
		std::uint8_t{23U}}) {
		expect(
			subject.set_part_mask(part, true).has_value(),
			"each scoped PMD part family can be masked"
		);
	}
	constexpr std::uint32_t expected_mask =
		(1U << 0U) | (1U << 10U) | (1U << 13U) | (1U << 14U) |
		(1U << 15U) | (1U << 23U);
	expect(
		subject.status().part_mask == expected_mask,
		"status reports ordinary, rhythm-all, effect, and PPZ mask domains"
	);
	expect(subject.set_part_mask(10U, false).has_value(), "part can be unmasked");
	expect(
		subject.status().part_mask == (expected_mask & ~(1U << 10U)),
		"unmask clears only the requested part"
	);
	for(std::uint8_t part = 0U; part <= 23U; ++part) {
		expect(
			subject.set_part_mask(part, true).has_value(),
			"every documented PMD part ID can be masked"
		);
	}
	expect(
		subject.status().part_mask == 0x00FF'FFFFU,
		"all 24 documented PMD part-mask bits are queryable"
	);
	for(std::uint8_t part = 0U; part <= 23U; ++part) {
		expect(
			subject.set_part_mask(part, false).has_value(),
			"every documented PMD part ID can be unmasked"
		);
	}
	expect(subject.status().part_mask == 0U, "all PMD part masks clear independently");
	const auto before_invalid = subject.status().part_mask;
	const auto invalid_part = subject.set_part_mask(24U, true);
	expect(
		!invalid_part && invalid_part.error().code == PmdOpnaErrorCode::invalid_part &&
			invalid_part.error().supplied_size == 24U,
		"part 24 is rejected at the typed boundary"
	);
	expect(
		subject.status().part_mask == before_invalid,
		"invalid part request is atomic"
	);

	for(const auto speed : {
		std::int8_t{3}, std::int8_t{8}, std::int8_t{16},
	}) {
		subject.set_volume(0U);
		subject.fade(speed);
		expect(
			render_until_volume_changes(subject, 0U) &&
				subject.status().fade_volume == static_cast<std::uint8_t>(speed),
			"each live TH03 fade speed is one signed PMD volume step"
		);
		subject.fade(0);
	}
	subject.set_volume(32U);
	subject.fade(16);
	subject.pause();
	status = subject.status();
	expect(status.paused && status.fade_speed == 16, "Pause preserves pending fade");
	const auto paused_measure = status.measure;
	const auto paused_timer_b = status.timer_b_count;
	const auto paused_volume = status.fade_volume;
	std::vector<PmdOpnaStereoSample> paused_samples(262'144U);
	subject.render_native(paused_samples);
	status = subject.status();
	expect(
		status.measure == paused_measure && status.timer_b_count == paused_timer_b &&
			status.fade_volume == paused_volume,
		"paused timer acknowledgements freeze sequence, measure, and fade state"
	);
	subject.resume();
	expect(!subject.status().paused, "Resume clears the public pause state");
	expect(
		render_until(subject, [](const auto &value) {
			return value.fade_volume > 32U;
		}),
		"resumed Timer-A service advances the signed fade"
	);
	expect(
		subject.status().measure > paused_measure ||
			subject.status().timer_b_count > paused_timer_b,
		"resumed song sequencing advances again"
	);

	subject.set_volume(250U);
	subject.fade(16);
	expect(
		render_until(subject, [](const auto &value) {
			return value.fade_volume == 255U && value.fade_speed == 0;
		}),
		"positive signed fade saturates at PMD volume 255 and completes"
	);
	subject.set_volume(5U);
	subject.fade(-8);
	expect(
		render_until(subject, [](const auto &value) {
			return value.fade_volume == 0U && value.fade_speed == 0;
		}),
		"negative signed fade saturates at loudest volume zero and completes"
	);
	subject.set_volume(77U);
	subject.fade(3);
	subject.fade(0);
	expect(
		subject.status().fade_volume == 77U && subject.status().fade_speed == 0,
		"speed zero cancels a fade without changing overall volume"
	);

	const auto writes_before_stop = subject.status().register_writes;
	subject.stop();
	status = subject.status();
	expect(
		!status.playing && !status.paused && status.fade_volume == 255U &&
			status.fade_speed == 0 && status.register_writes > writes_before_stop,
		"Stop applies the PMD mute sequence and retains the prepared song"
	);
	const auto stopped_measure = status.measure;
	std::vector<PmdOpnaStereoSample> stopped_samples(65'536U);
	subject.render_native(stopped_samples);
	expect(
		subject.status().measure == stopped_measure,
		"stopped song does not resume sequencing while the chip drains"
	);
	expect(subject.play().has_value(), "Play after Stop starts the prepared song anew");
	status = subject.status();
	expect(
		status.playing && !status.paused && status.fade_volume == 0U &&
			status.fade_speed == 0 && status.part_mask == 0U && status.measure == 0U,
		"fresh restart discards prior pause/fade/mask/measure state"
	);
}

} // namespace

int main()
{
	test_effect_rule_table();
	test_effect_scheduler();
	test_effect_scheduler_exhaustive();
	test_transport_and_queries();
	if(failures == 0U) {
		std::cout << "all " << checks << " TH03 PMD service checks passed\n";
		return 0;
	}
	std::cerr << failures << " of " << checks
		<< " TH03 PMD service checks failed\n";
	return 1;
}

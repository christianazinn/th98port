#ifndef TH03_AUDIO_TH03_PMD_SERVICE_HPP
#define TH03_AUDIO_TH03_PMD_SERVICE_HPP

#include <cstdint>
#include <expected>
#include <optional>

namespace th03::audio {

inline constexpr std::uint8_t th03_sound_effect_first = 1U;
inline constexpr std::uint8_t th03_sound_effect_last = 22U;

struct Th03SoundEffectRule {
	std::uint8_t priority = 0U;
	std::uint8_t hold_frames = 0U;

	friend constexpr bool operator==(
		const Th03SoundEffectRule &,
		const Th03SoundEffectRule &
	) = default;
};

struct Th03SoundEffectStart {
	std::uint8_t effect_id = 0U;

	friend constexpr bool operator==(
		const Th03SoundEffectStart &,
		const Th03SoundEffectStart &
	) = default;
};

enum class Th03PmdServiceErrorCode : std::uint8_t {
	invalid_effect_id,
};

struct Th03PmdServiceError {
	Th03PmdServiceErrorCode code =
		Th03PmdServiceErrorCode::invalid_effect_id;
	std::uint8_t effect_id = 0U;
};

std::expected<Th03SoundEffectRule, Th03PmdServiceError>
th03_sound_effect_rule(std::uint8_t effect_id) noexcept;

struct Th03SoundEffectSchedulerStatus {
	bool active = true;
	std::optional<std::uint8_t> pending_effect{};
	std::uint8_t pending_frame = 0U;
};

// Source-exact one-slot TH03 scheduler. update() returns the PMD effect ID that
// function 0Ch would receive on this gameplay frame. Hold frames arbitrate later
// requests; they are not decoded effect durations.
class Th03SoundEffectScheduler {
public:
	explicit constexpr Th03SoundEffectScheduler(bool active = true) noexcept :
		active_(active)
	{
	}

	void set_active(bool active) noexcept;
	void reset() noexcept;
	// true means that the pending slot was installed or replaced. A valid lower-
	// priority request returns false and leaves both ID and frame untouched.
	std::expected<bool, Th03PmdServiceError> request(
		std::uint8_t effect_id
	) noexcept;
	std::optional<Th03SoundEffectStart> update() noexcept;
	// Exact snd_se_play_force(): reset, request, then one immediate update.
	std::expected<std::optional<Th03SoundEffectStart>, Th03PmdServiceError> force(
		std::uint8_t effect_id
	) noexcept;

	[[nodiscard]] Th03SoundEffectSchedulerStatus status() const noexcept;

private:
	bool active_ = true;
	std::optional<std::uint8_t> pending_{};
	std::uint8_t frame_ = 0U;
};

} // namespace th03::audio

#endif

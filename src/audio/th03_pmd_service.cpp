#include "th03/audio/th03_pmd_service.hpp"

#include <array>

namespace th03::audio {
namespace {

constexpr std::array<Th03SoundEffectRule, th03_sound_effect_last> rules{{
	{0U, 0U}, {32U, 36U}, {16U, 16U}, {2U, 4U}, {18U, 16U},
	{18U, 8U}, {18U, 10U}, {64U, 48U}, {16U, 80U}, {17U, 24U},
	{2U, 17U}, {17U, 11U}, {32U, 4U}, {18U, 80U}, {18U, 16U},
	{18U, 48U}, {18U, 74U}, {32U, 50U}, {32U, 32U}, {18U, 24U},
	{18U, 68U}, {14U, 0U},
}};

std::expected<std::size_t, Th03PmdServiceError> rule_index(
	std::uint8_t effect_id
) noexcept
{
	if(effect_id < th03_sound_effect_first ||
		effect_id > th03_sound_effect_last) {
		return std::unexpected(Th03PmdServiceError{
			.code = Th03PmdServiceErrorCode::invalid_effect_id,
			.effect_id = effect_id,
		});
	}
	return static_cast<std::size_t>(effect_id - th03_sound_effect_first);
}

} // namespace

std::expected<Th03SoundEffectRule, Th03PmdServiceError>
th03_sound_effect_rule(std::uint8_t effect_id) noexcept
{
	const auto index = rule_index(effect_id);
	if(!index) {
		return std::unexpected(index.error());
	}
	return rules[*index];
}

void Th03SoundEffectScheduler::set_active(bool active) noexcept
{
	active_ = active;
}

void Th03SoundEffectScheduler::reset() noexcept
{
	frame_ = 0U;
	pending_.reset();
}

std::expected<bool, Th03PmdServiceError>
Th03SoundEffectScheduler::request(std::uint8_t effect_id) noexcept
{
	const auto requested_rule = th03_sound_effect_rule(effect_id);
	if(!requested_rule) {
		return std::unexpected(requested_rule.error());
	}
	if(!active_) {
		return false;
	}
	if(!pending_) {
		pending_ = effect_id;
		return true;
	}
	const auto pending_rule = rules[
		static_cast<std::size_t>(*pending_ - th03_sound_effect_first)
	];
	if(pending_rule.priority <= requested_rule->priority) {
		pending_ = effect_id;
		frame_ = 0U;
		return true;
	}
	return false;
}

std::optional<Th03SoundEffectStart> Th03SoundEffectScheduler::update() noexcept
{
	if(!active_ || !pending_) {
		return std::nullopt;
	}
	const auto effect = *pending_;
	const auto dispatched = frame_ == 0U ?
		std::optional{Th03SoundEffectStart{.effect_id = effect}} : std::nullopt;
	++frame_;
	const auto rule = rules[
		static_cast<std::size_t>(effect - th03_sound_effect_first)
	];
	if(rule.hold_frames < frame_) {
		reset();
	}
	return dispatched;
}

std::expected<std::optional<Th03SoundEffectStart>, Th03PmdServiceError>
Th03SoundEffectScheduler::force(std::uint8_t effect_id) noexcept
{
	const auto validated = th03_sound_effect_rule(effect_id);
	if(!validated) {
		return std::unexpected(validated.error());
	}
	reset();
	const auto requested = request(effect_id);
	if(!requested) {
		return std::unexpected(requested.error());
	}
	return update();
}

Th03SoundEffectSchedulerStatus Th03SoundEffectScheduler::status() const noexcept
{
	return Th03SoundEffectSchedulerStatus{
		.active = active_,
		.pending_effect = pending_,
		.pending_frame = frame_,
	};
}

} // namespace th03::audio

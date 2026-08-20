#ifndef TH03_APP_UPDATE_ACTIVATION_HPP
#define TH03_APP_UPDATE_ACTIVATION_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>

namespace th03::app {

inline constexpr std::size_t update_activation_file_bytes = 256U;

struct UpdateGenerationIdentity final {
	std::uint64_t sequence = 0U;
	std::array<std::uint8_t, 32U> artifact_sha256{};

	friend bool operator==(
		const UpdateGenerationIdentity &,
		const UpdateGenerationIdentity &
	) = default;
};

enum class UpdateActivationPhase : std::uint8_t {
	stable = 0U,
	pending = 1U,
	trial = 2U,
};

struct UpdateActivationState final {
	std::uint64_t revision = 0U;
	std::uint64_t highest_sequence = 0U;
	std::uint32_t failed_trials = 0U;
	UpdateActivationPhase phase = UpdateActivationPhase::stable;
	std::optional<UpdateGenerationIdentity> active{};
	std::optional<UpdateGenerationIdentity> pending{};
	std::optional<UpdateGenerationIdentity> previous{};
	std::optional<UpdateGenerationIdentity> last_failed{};

	friend bool operator==(
		const UpdateActivationState &,
		const UpdateActivationState &
	) = default;
};

struct UpdateTrialToken final {
	std::uint64_t revision = 0U;
	UpdateGenerationIdentity generation{};

	friend bool operator==(
		const UpdateTrialToken &,
		const UpdateTrialToken &
	) = default;
};

struct UpdateTrialBeginResult final {
	UpdateActivationState state{};
	UpdateTrialToken token{};
};

enum class UpdateActivationCodecErrorCode : std::uint8_t {
	input_size,
	magic,
	version,
	declared_size,
	phase,
	presence,
	reserved,
	digest,
	state,
};

struct UpdateActivationCodecError final {
	UpdateActivationCodecErrorCode code =
		UpdateActivationCodecErrorCode::input_size;
	std::size_t offset = 0U;
};

enum class UpdateActivationTransitionError : std::uint8_t {
	invalid_state,
	invalid_generation,
	busy,
	current_or_older,
	sequence_reused,
	no_pending,
	no_active,
	not_trial,
	stale_token,
	revision_exhausted,
	failure_count_exhausted,
};

[[nodiscard]] bool valid_update_generation_identity(
	const UpdateGenerationIdentity &identity
) noexcept;

// Returns the only permitted on-disk directory name for this exact signed
// artifact identity, or an empty string for an invalid identity.
[[nodiscard]] std::string update_generation_directory_name(
	const UpdateGenerationIdentity &identity
);

[[nodiscard]] bool valid_update_activation_state(
	const UpdateActivationState &state
) noexcept;

[[nodiscard]] std::expected<
	std::array<std::byte, update_activation_file_bytes>,
	UpdateActivationCodecError
> encode_update_activation_state(const UpdateActivationState &state) noexcept;

[[nodiscard]] std::expected<
	UpdateActivationState,
	UpdateActivationCodecError
> decode_update_activation_state(std::span<const std::byte> bytes) noexcept;

[[nodiscard]] std::expected<
	UpdateActivationState,
	UpdateActivationTransitionError
> nominate_update_generation(
	const UpdateActivationState &state,
	const UpdateGenerationIdentity &candidate
) noexcept;

[[nodiscard]] std::expected<
	UpdateTrialBeginResult,
	UpdateActivationTransitionError
> begin_update_trial(const UpdateActivationState &state) noexcept;

[[nodiscard]] std::expected<
	UpdateActivationState,
	UpdateActivationTransitionError
> acknowledge_update_trial(
	const UpdateActivationState &state,
	const UpdateTrialToken &token
) noexcept;

[[nodiscard]] std::expected<
	UpdateActivationState,
	UpdateActivationTransitionError
> fail_update_trial(
	const UpdateActivationState &state,
	const UpdateTrialToken &token
) noexcept;

[[nodiscard]] std::expected<
	UpdateActivationState,
	UpdateActivationTransitionError
> recover_interrupted_update_trial(
	const UpdateActivationState &state
) noexcept;

// Records an active generation that the bootstrap could not launch and
// restores the retained previous generation (or the legacy install). The
// expected identity prevents an obsolete bootstrap owner from rolling back a
// newer state.
[[nodiscard]] std::expected<
	UpdateActivationState,
	UpdateActivationTransitionError
> rollback_unlaunchable_active_generation(
	const UpdateActivationState &state,
	const UpdateGenerationIdentity &expected_active
) noexcept;

} // namespace th03::app

#endif

#include "th03/app/update_activation.hpp"

#include <mbedtls/sha256.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <span>

namespace th03::app {
namespace {

constexpr std::array<std::byte, 8U> activation_magic{
	std::byte{'T'}, std::byte{'3'}, std::byte{'A'}, std::byte{'C'},
	std::byte{'T'}, std::byte{'0'}, std::byte{'0'}, std::byte{'1'},
};
constexpr std::uint16_t activation_version = 1U;
constexpr std::size_t phase_offset = 12U;
constexpr std::size_t presence_offset = 13U;
constexpr std::size_t revision_offset = 16U;
constexpr std::size_t highest_sequence_offset = 24U;
constexpr std::size_t failed_trials_offset = 32U;
constexpr std::size_t header_reserved_offset = 36U;
constexpr std::size_t identities_offset = 64U;
constexpr std::size_t identity_bytes = 40U;
constexpr std::size_t digest_offset = 224U;
constexpr std::uint8_t active_present = 0x01U;
constexpr std::uint8_t pending_present = 0x02U;
constexpr std::uint8_t previous_present = 0x04U;
constexpr std::uint8_t failed_present = 0x08U;

[[nodiscard]] constexpr std::uint8_t byte(std::byte value) noexcept
{
	return std::to_integer<std::uint8_t>(value);
}

[[nodiscard]] constexpr std::uint16_t u16(
	std::span<const std::byte> bytes,
	std::size_t offset
) noexcept
{
	return static_cast<std::uint16_t>(byte(bytes[offset])) |
		static_cast<std::uint16_t>(byte(bytes[offset + 1U]) << 8U);
}

[[nodiscard]] constexpr std::uint32_t u32(
	std::span<const std::byte> bytes,
	std::size_t offset
) noexcept
{
	std::uint32_t result = 0U;
	for(std::size_t index = 0U; index < 4U; ++index) {
		result |= static_cast<std::uint32_t>(byte(bytes[offset + index])) <<
			(index * 8U);
	}
	return result;
}

[[nodiscard]] constexpr std::uint64_t u64(
	std::span<const std::byte> bytes,
	std::size_t offset
) noexcept
{
	std::uint64_t result = 0U;
	for(std::size_t index = 0U; index < 8U; ++index) {
		result |= static_cast<std::uint64_t>(byte(bytes[offset + index])) <<
			(index * 8U);
	}
	return result;
}

constexpr void put_u16(
	std::span<std::byte> bytes,
	std::size_t offset,
	std::uint16_t value
) noexcept
{
	bytes[offset] = static_cast<std::byte>(value & 0xFFU);
	bytes[offset + 1U] = static_cast<std::byte>(value >> 8U);
}

constexpr void put_u32(
	std::span<std::byte> bytes,
	std::size_t offset,
	std::uint32_t value
) noexcept
{
	for(std::size_t index = 0U; index < 4U; ++index) {
		bytes[offset + index] = static_cast<std::byte>(value >> (index * 8U));
	}
}

constexpr void put_u64(
	std::span<std::byte> bytes,
	std::size_t offset,
	std::uint64_t value
) noexcept
{
	for(std::size_t index = 0U; index < 8U; ++index) {
		bytes[offset + index] = static_cast<std::byte>(value >> (index * 8U));
	}
}

[[nodiscard]] UpdateActivationCodecError codec_error(
	UpdateActivationCodecErrorCode code,
	std::size_t offset
) noexcept
{
	return {.code = code, .offset = offset};
}

[[nodiscard]] bool sha256(
	std::span<const std::byte> bytes,
	std::span<std::byte, 32U> output
) noexcept
{
	return mbedtls_sha256(
		reinterpret_cast<const unsigned char *>(bytes.data()), bytes.size(),
		reinterpret_cast<unsigned char *>(output.data()), 0
	) == 0;
}

[[nodiscard]] std::uint8_t presence_bits(
	const UpdateActivationState &state
) noexcept
{
	return static_cast<std::uint8_t>(
		(state.active ? active_present : 0U) |
		(state.pending ? pending_present : 0U) |
		(state.previous ? previous_present : 0U) |
		(state.last_failed ? failed_present : 0U)
	);
}

void put_identity(
	std::span<std::byte> bytes,
	std::size_t offset,
	const std::optional<UpdateGenerationIdentity> &identity
) noexcept
{
	if(!identity) {
		return;
	}
	put_u64(bytes, offset, identity->sequence);
	auto digest = bytes.subspan(offset + 8U, identity->artifact_sha256.size());
	std::ranges::transform(
		identity->artifact_sha256, digest.begin(),
		[](std::uint8_t value) { return static_cast<std::byte>(value); }
	);
}

[[nodiscard]] std::optional<UpdateGenerationIdentity> get_identity(
	std::span<const std::byte> bytes,
	std::size_t offset,
	bool present
) noexcept
{
	if(!present) {
		return std::nullopt;
	}
	UpdateGenerationIdentity result{};
	result.sequence = u64(bytes, offset);
	std::ranges::transform(
		bytes.subspan(offset + 8U, result.artifact_sha256.size()),
		result.artifact_sha256.begin(), byte
	);
	return result;
}

[[nodiscard]] bool zero_region(
	std::span<const std::byte> bytes,
	std::size_t begin,
	std::size_t end
) noexcept
{
	return std::ranges::all_of(
		bytes.subspan(begin, end - begin),
		[](std::byte value) { return value == std::byte{}; }
	);
}

[[nodiscard]] bool absent_identity_zero(
	std::span<const std::byte> bytes,
	std::size_t offset,
	bool present
) noexcept
{
	return present || zero_region(bytes, offset, offset + identity_bytes);
}

[[nodiscard]] std::expected<std::uint64_t, UpdateActivationTransitionError>
next_revision(const UpdateActivationState &state) noexcept
{
	if(state.revision == std::numeric_limits<std::uint64_t>::max()) {
		return std::unexpected(
			UpdateActivationTransitionError::revision_exhausted
		);
	}
	return state.revision + 1U;
}

[[nodiscard]] std::expected<UpdateActivationState, UpdateActivationTransitionError>
failed_state(const UpdateActivationState &state) noexcept
{
	if(state.failed_trials == std::numeric_limits<std::uint32_t>::max()) {
		return std::unexpected(
			UpdateActivationTransitionError::failure_count_exhausted
		);
	}
	auto revision = next_revision(state);
	if(!revision) {
		return std::unexpected(revision.error());
	}
	auto result = state;
	result.revision = *revision;
	++result.failed_trials;
	result.phase = UpdateActivationPhase::stable;
	result.last_failed = result.pending;
	result.pending.reset();
	return result;
}

} // namespace

bool valid_update_generation_identity(
	const UpdateGenerationIdentity &identity
) noexcept
{
	return identity.sequence != 0U && std::ranges::any_of(
		identity.artifact_sha256,
		[](std::uint8_t value) { return value != 0U; }
	);
}

std::string update_generation_directory_name(
	const UpdateGenerationIdentity &identity
)
{
	if(!valid_update_generation_identity(identity)) {
		return {};
	}
	constexpr char hex[] = "0123456789abcdef";
	std::string digest;
	digest.reserve(16U);
	for(std::size_t index = 0U; index < 8U; ++index) {
		digest.push_back(hex[identity.artifact_sha256[index] >> 4U]);
		digest.push_back(hex[identity.artifact_sha256[index] & 0x0FU]);
	}
	return "PODD-GEN-" + std::to_string(identity.sequence) + "-" + digest;
}

bool valid_update_activation_state(const UpdateActivationState &state) noexcept
{
	if(state.phase != UpdateActivationPhase::stable &&
		state.phase != UpdateActivationPhase::pending &&
		state.phase != UpdateActivationPhase::trial) {
		return false;
	}
	if((state.phase == UpdateActivationPhase::stable) != !state.pending) {
		return false;
	}
	const std::array identities{
		&state.active, &state.pending, &state.previous, &state.last_failed,
	};
	bool any = false;
	std::uint64_t greatest_sequence = 0U;
	for(const auto *identity : identities) {
		if(*identity) {
			any = true;
			if(!valid_update_generation_identity(**identity) ||
				(*identity)->sequence > state.highest_sequence) {
				return false;
			}
			greatest_sequence = std::max(
				greatest_sequence, (*identity)->sequence
			);
		}
	}
	if(any != (state.highest_sequence != 0U) ||
		state.highest_sequence != greatest_sequence) {
		return false;
	}
	if(state.last_failed && state.failed_trials == 0U) {
		return false;
	}
	if(state.previous && !state.active) {
		return false;
	}
	if(state.previous && state.active &&
		state.previous->sequence >= state.active->sequence) {
		return false;
	}
	if(state.pending && state.pending->sequence != state.highest_sequence) {
		return false;
	}
	if(state.last_failed &&
		state.last_failed->sequence != state.highest_sequence) {
		return false;
	}
	if(state.pending && state.last_failed &&
		*state.pending != *state.last_failed) {
		return false;
	}
	if(state.pending && state.active && *state.pending == *state.active) {
		return false;
	}
	return true;
}

std::expected<
	std::array<std::byte, update_activation_file_bytes>,
	UpdateActivationCodecError
> encode_update_activation_state(const UpdateActivationState &state) noexcept
{
	if(!valid_update_activation_state(state)) {
		return std::unexpected(codec_error(
			UpdateActivationCodecErrorCode::state, 0U
		));
	}
	std::array<std::byte, update_activation_file_bytes> result{};
	std::copy(activation_magic.begin(), activation_magic.end(), result.begin());
	put_u16(result, 8U, activation_version);
	put_u16(result, 10U, update_activation_file_bytes);
	result[phase_offset] = static_cast<std::byte>(state.phase);
	result[presence_offset] = static_cast<std::byte>(presence_bits(state));
	put_u64(result, revision_offset, state.revision);
	put_u64(result, highest_sequence_offset, state.highest_sequence);
	put_u32(result, failed_trials_offset, state.failed_trials);
	put_identity(result, identities_offset, state.active);
	put_identity(result, identities_offset + identity_bytes, state.pending);
	put_identity(result, identities_offset + (identity_bytes * 2U), state.previous);
	put_identity(
		result, identities_offset + (identity_bytes * 3U), state.last_failed
	);
	if(!sha256(
		std::span<const std::byte>(result).first(digest_offset),
		std::span<std::byte>(result).subspan<digest_offset, 32U>()
	)) {
		return std::unexpected(codec_error(
			UpdateActivationCodecErrorCode::digest, digest_offset
		));
	}
	return result;
}

std::expected<UpdateActivationState, UpdateActivationCodecError>
decode_update_activation_state(std::span<const std::byte> bytes) noexcept
{
	if(bytes.size() != update_activation_file_bytes) {
		return std::unexpected(codec_error(
			UpdateActivationCodecErrorCode::input_size, bytes.size()
		));
	}
	if(!std::ranges::equal(activation_magic, bytes.first(activation_magic.size()))) {
		return std::unexpected(codec_error(
			UpdateActivationCodecErrorCode::magic, 0U
		));
	}
	if(u16(bytes, 8U) != activation_version) {
		return std::unexpected(codec_error(
			UpdateActivationCodecErrorCode::version, 8U
		));
	}
	if(u16(bytes, 10U) != update_activation_file_bytes) {
		return std::unexpected(codec_error(
			UpdateActivationCodecErrorCode::declared_size, 10U
		));
	}
	const auto phase = static_cast<UpdateActivationPhase>(byte(bytes[phase_offset]));
	if(phase != UpdateActivationPhase::stable &&
		phase != UpdateActivationPhase::pending &&
		phase != UpdateActivationPhase::trial) {
		return std::unexpected(codec_error(
			UpdateActivationCodecErrorCode::phase, phase_offset
		));
	}
	const auto presence = byte(bytes[presence_offset]);
	if((presence & 0xF0U) != 0U) {
		return std::unexpected(codec_error(
			UpdateActivationCodecErrorCode::presence, presence_offset
		));
	}
	if(!zero_region(bytes, 14U, revision_offset) ||
		!zero_region(bytes, header_reserved_offset, identities_offset)) {
		return std::unexpected(codec_error(
			UpdateActivationCodecErrorCode::reserved, 14U
		));
	}
	std::array<std::byte, 32U> expected_digest{};
	if(!sha256(bytes.first(digest_offset), expected_digest) ||
		!std::ranges::equal(expected_digest, bytes.subspan(digest_offset))) {
		return std::unexpected(codec_error(
			UpdateActivationCodecErrorCode::digest, digest_offset
		));
	}
	const std::array present{
		(presence & active_present) != 0U,
		(presence & pending_present) != 0U,
		(presence & previous_present) != 0U,
		(presence & failed_present) != 0U,
	};
	for(std::size_t index = 0U; index < present.size(); ++index) {
		const auto offset = identities_offset + (index * identity_bytes);
		if(!absent_identity_zero(bytes, offset, present[index])) {
			return std::unexpected(codec_error(
				UpdateActivationCodecErrorCode::presence, offset
			));
		}
	}
	UpdateActivationState result{};
	result.phase = phase;
	result.revision = u64(bytes, revision_offset);
	result.highest_sequence = u64(bytes, highest_sequence_offset);
	result.failed_trials = u32(bytes, failed_trials_offset);
	result.active = get_identity(bytes, identities_offset, present[0]);
	result.pending = get_identity(
		bytes, identities_offset + identity_bytes, present[1]
	);
	result.previous = get_identity(
		bytes, identities_offset + (identity_bytes * 2U), present[2]
	);
	result.last_failed = get_identity(
		bytes, identities_offset + (identity_bytes * 3U), present[3]
	);
	if(!valid_update_activation_state(result)) {
		return std::unexpected(codec_error(
			UpdateActivationCodecErrorCode::state, phase_offset
		));
	}
	return result;
}

std::expected<UpdateActivationState, UpdateActivationTransitionError>
nominate_update_generation(
	const UpdateActivationState &state,
	const UpdateGenerationIdentity &candidate
) noexcept
{
	if(!valid_update_activation_state(state)) {
		return std::unexpected(UpdateActivationTransitionError::invalid_state);
	}
	if(!valid_update_generation_identity(candidate)) {
		return std::unexpected(UpdateActivationTransitionError::invalid_generation);
	}
	if(state.phase != UpdateActivationPhase::stable) {
		return std::unexpected(UpdateActivationTransitionError::busy);
	}
	if(state.active && candidate == *state.active) {
		return std::unexpected(UpdateActivationTransitionError::current_or_older);
	}
	const bool exact_retry = state.last_failed && candidate == *state.last_failed;
	if(candidate.sequence < state.highest_sequence) {
		return std::unexpected(UpdateActivationTransitionError::current_or_older);
	}
	if(candidate.sequence == state.highest_sequence && !exact_retry) {
		return std::unexpected(UpdateActivationTransitionError::sequence_reused);
	}
	auto revision = next_revision(state);
	if(!revision) {
		return std::unexpected(revision.error());
	}
	auto result = state;
	result.revision = *revision;
	result.phase = UpdateActivationPhase::pending;
	result.pending = candidate;
	if(!exact_retry) {
		result.highest_sequence = candidate.sequence;
		result.last_failed.reset();
	}
	return result;
}

std::expected<UpdateTrialBeginResult, UpdateActivationTransitionError>
begin_update_trial(const UpdateActivationState &state) noexcept
{
	if(!valid_update_activation_state(state)) {
		return std::unexpected(UpdateActivationTransitionError::invalid_state);
	}
	if(state.phase != UpdateActivationPhase::pending || !state.pending) {
		return std::unexpected(UpdateActivationTransitionError::no_pending);
	}
	auto revision = next_revision(state);
	if(!revision) {
		return std::unexpected(revision.error());
	}
	auto result = state;
	result.revision = *revision;
	result.phase = UpdateActivationPhase::trial;
	return UpdateTrialBeginResult{
		.state = std::move(result),
		.token = {.revision = *revision, .generation = *state.pending},
	};
}

std::expected<UpdateActivationState, UpdateActivationTransitionError>
acknowledge_update_trial(
	const UpdateActivationState &state,
	const UpdateTrialToken &token
) noexcept
{
	if(!valid_update_activation_state(state)) {
		return std::unexpected(UpdateActivationTransitionError::invalid_state);
	}
	if(state.phase != UpdateActivationPhase::trial || !state.pending) {
		return std::unexpected(UpdateActivationTransitionError::not_trial);
	}
	if(token.revision != state.revision || token.generation != *state.pending) {
		return std::unexpected(UpdateActivationTransitionError::stale_token);
	}
	auto revision = next_revision(state);
	if(!revision) {
		return std::unexpected(revision.error());
	}
	auto result = state;
	result.revision = *revision;
	result.phase = UpdateActivationPhase::stable;
	result.previous = result.active;
	result.active = result.pending;
	result.pending.reset();
	result.last_failed.reset();
	return result;
}

std::expected<UpdateActivationState, UpdateActivationTransitionError>
fail_update_trial(
	const UpdateActivationState &state,
	const UpdateTrialToken &token
) noexcept
{
	if(!valid_update_activation_state(state)) {
		return std::unexpected(UpdateActivationTransitionError::invalid_state);
	}
	if(state.phase != UpdateActivationPhase::trial || !state.pending) {
		return std::unexpected(UpdateActivationTransitionError::not_trial);
	}
	if(token.revision != state.revision || token.generation != *state.pending) {
		return std::unexpected(UpdateActivationTransitionError::stale_token);
	}
	return failed_state(state);
}

std::expected<UpdateActivationState, UpdateActivationTransitionError>
recover_interrupted_update_trial(const UpdateActivationState &state) noexcept
{
	if(!valid_update_activation_state(state)) {
		return std::unexpected(UpdateActivationTransitionError::invalid_state);
	}
	if(state.phase != UpdateActivationPhase::trial) {
		return state;
	}
	return failed_state(state);
}

} // namespace th03::app

#include "th03/app/update/update_bootstrap.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace th03::app {

struct UpdateBootstrapLeaseHandle final {
#if defined(_WIN32)
	HANDLE value = INVALID_HANDLE_VALUE;
#else
	int value = -1;
#endif

	~UpdateBootstrapLeaseHandle()
	{
#if defined(_WIN32)
		if(value != INVALID_HANDLE_VALUE) {
			(void)CloseHandle(value);
		}
#else
		if(value >= 0) {
			(void)::flock(value, LOCK_UN);
			(void)::close(value);
		}
#endif
	}
};

void UpdateBootstrapLeaseHandleDeleter::operator()(
	UpdateBootstrapLeaseHandle *handle
) const noexcept
{
	delete handle;
}

namespace {

namespace fs = std::filesystem;

constexpr std::string_view token_magic = "T3TRL001";
constexpr std::size_t revision_offset = 9U;
constexpr std::size_t sequence_offset = 26U;
constexpr std::size_t digest_offset = 43U;
constexpr std::size_t transaction_attempts = 16U;

[[nodiscard]] UpdateTrialTokenTextError token_error(
	UpdateTrialTokenTextErrorCode code,
	std::size_t offset
) noexcept
{
	return {.code = code, .offset = offset};
}

[[nodiscard]] UpdateBootstrapError bootstrap_error(
	UpdateBootstrapErrorCode code,
	std::error_code system = {},
	std::optional<UpdateActivationStoreError> store = std::nullopt,
	std::optional<UpdateActivationTransitionError> transition = std::nullopt
) noexcept
{
	return {
		.code = code,
		.system = system,
		.store = std::move(store),
		.transition = transition,
	};
}

[[nodiscard]] std::error_code last_system_error() noexcept
{
#if defined(_WIN32)
	return {static_cast<int>(GetLastError()), std::system_category()};
#else
	return {errno, std::generic_category()};
#endif
}

[[nodiscard]] constexpr char hex_digit(std::uint8_t value) noexcept
{
	constexpr char hex[] = "0123456789abcdef";
	return hex[value & 0x0FU];
}

void append_u64_hex(std::string &result, std::uint64_t value)
{
	for(std::size_t shift = 64U; shift != 0U; shift -= 4U) {
		result.push_back(hex_digit(static_cast<std::uint8_t>(
			value >> (shift - 4U)
		)));
	}
}

[[nodiscard]] std::optional<std::uint8_t> parse_hex(char value) noexcept
{
	if(value >= '0' && value <= '9') {
		return static_cast<std::uint8_t>(value - '0');
	}
	if(value >= 'a' && value <= 'f') {
		return static_cast<std::uint8_t>(value - 'a' + 10);
	}
	return std::nullopt;
}

[[nodiscard]] std::expected<std::uint64_t, UpdateTrialTokenTextError>
parse_u64(std::string_view text, std::size_t offset) noexcept
{
	std::uint64_t result = 0U;
	for(std::size_t index = 0U; index < 16U; ++index) {
		auto digit = parse_hex(text[offset + index]);
		if(!digit) {
			return std::unexpected(token_error(
				UpdateTrialTokenTextErrorCode::hex_character, offset + index
			));
		}
		result = (result << 4U) | *digit;
	}
	return result;
}

[[nodiscard]] bool valid_token(const UpdateTrialToken &token) noexcept
{
	return token.revision != 0U &&
		valid_update_generation_identity(token.generation);
}

[[nodiscard]] UpdateLaunchPlan launch_plan(
	const std::optional<UpdateGenerationIdentity> &generation,
	bool recovered
)
{
	if(!generation) {
		return {
			.source = UpdateLaunchSource::legacy_install,
			.recovered_interrupted_trial = recovered,
		};
	}
	return {
		.source = UpdateLaunchSource::active_generation,
		.generation = generation,
		.generation_directory = update_generation_directory_name(*generation),
		.recovered_interrupted_trial = recovered,
	};
}

[[nodiscard]] bool retryable(const UpdateActivationStoreError &error) noexcept
{
	return error.code == UpdateActivationStoreErrorCode::stale_revision;
}

} // namespace

std::expected<std::string, UpdateTrialTokenTextError>
encode_update_trial_token(const UpdateTrialToken &token)
{
	if(!valid_token(token)) {
		return std::unexpected(token_error(
			UpdateTrialTokenTextErrorCode::invalid_token, 0U
		));
	}
	std::string result;
	result.reserve(update_trial_token_text_bytes);
	result.append(token_magic);
	result.push_back(':');
	append_u64_hex(result, token.revision);
	result.push_back(':');
	append_u64_hex(result, token.generation.sequence);
	result.push_back(':');
	for(const auto byte : token.generation.artifact_sha256) {
		result.push_back(hex_digit(byte >> 4U));
		result.push_back(hex_digit(byte));
	}
	return result;
}

std::expected<UpdateTrialToken, UpdateTrialTokenTextError>
decode_update_trial_token(std::string_view text) noexcept
{
	if(text.size() != update_trial_token_text_bytes) {
		return std::unexpected(token_error(
			UpdateTrialTokenTextErrorCode::input_size, text.size()
		));
	}
	if(text.substr(0U, token_magic.size()) != token_magic) {
		return std::unexpected(token_error(
			UpdateTrialTokenTextErrorCode::magic, 0U
		));
	}
	for(const auto offset : std::array{8U, 25U, 42U}) {
		if(text[offset] != ':') {
			return std::unexpected(token_error(
				UpdateTrialTokenTextErrorCode::separator, offset
			));
		}
	}
	auto revision = parse_u64(text, revision_offset);
	if(!revision) {
		return std::unexpected(revision.error());
	}
	auto sequence = parse_u64(text, sequence_offset);
	if(!sequence) {
		return std::unexpected(sequence.error());
	}
	UpdateTrialToken result{
		.revision = *revision,
		.generation = {.sequence = *sequence},
	};
	for(std::size_t index = 0U;
		index < result.generation.artifact_sha256.size(); ++index) {
		auto high = parse_hex(text[digest_offset + (index * 2U)]);
		if(!high) {
			return std::unexpected(token_error(
				UpdateTrialTokenTextErrorCode::hex_character,
				digest_offset + (index * 2U)
			));
		}
		auto low = parse_hex(text[digest_offset + (index * 2U) + 1U]);
		if(!low) {
			return std::unexpected(token_error(
				UpdateTrialTokenTextErrorCode::hex_character,
				digest_offset + (index * 2U) + 1U
			));
		}
		result.generation.artifact_sha256[index] =
			static_cast<std::uint8_t>((*high << 4U) | *low);
	}
	if(!valid_token(result)) {
		return std::unexpected(token_error(
			UpdateTrialTokenTextErrorCode::invalid_token, 0U
		));
	}
	return result;
}

UpdateBootstrapLease::UpdateBootstrapLease(
	fs::path state_path,
	std::unique_ptr<
		UpdateBootstrapLeaseHandle,
		UpdateBootstrapLeaseHandleDeleter
	> handle
) noexcept :
	state_path_(std::move(state_path)), handle_(std::move(handle))
{
}

UpdateBootstrapLease::UpdateBootstrapLease(UpdateBootstrapLease &&) noexcept =
	default;
UpdateBootstrapLease &UpdateBootstrapLease::operator=(
	UpdateBootstrapLease &&
) noexcept = default;
UpdateBootstrapLease::~UpdateBootstrapLease() = default;

UpdateBootstrapLease::operator bool() const noexcept
{
	return handle_ != nullptr;
}

const fs::path &UpdateBootstrapLease::state_path() const noexcept
{
	return state_path_;
}

std::expected<UpdateBootstrapLease, UpdateBootstrapError>
acquire_update_bootstrap_lease(const fs::path &state_path)
{
	if(state_path.empty() || !state_path.has_filename() ||
		state_path.parent_path().empty()) {
		return std::unexpected(bootstrap_error(
			UpdateBootstrapErrorCode::invalid_path
		));
	}
	std::error_code system;
	fs::create_directories(state_path.parent_path(), system);
	if(system) {
		return std::unexpected(bootstrap_error(
			UpdateBootstrapErrorCode::status, system
		));
	}
	auto lock_path = state_path;
	lock_path += fs::path(".bootstrap.lock").native();
	std::unique_ptr<
		UpdateBootstrapLeaseHandle,
		UpdateBootstrapLeaseHandleDeleter
	> handle(new UpdateBootstrapLeaseHandle{});
#if defined(_WIN32)
	handle->value = CreateFileW(
		lock_path.c_str(), GENERIC_READ | GENERIC_WRITE, 0U, nullptr,
		OPEN_ALWAYS, FILE_ATTRIBUTE_HIDDEN, nullptr
	);
	if(handle->value == INVALID_HANDLE_VALUE) {
		const auto error = last_system_error();
		const auto code = error.value() == ERROR_SHARING_VIOLATION ||
			error.value() == ERROR_LOCK_VIOLATION ?
			UpdateBootstrapErrorCode::already_running :
			UpdateBootstrapErrorCode::lock;
		return std::unexpected(bootstrap_error(code, error));
	}
#else
	handle->value = ::open(
		lock_path.c_str(), O_CREAT | O_RDWR | O_NOFOLLOW, 0600
	);
	if(handle->value < 0) {
		return std::unexpected(bootstrap_error(
			UpdateBootstrapErrorCode::lock, last_system_error()
		));
	}
	if(::flock(handle->value, LOCK_EX | LOCK_NB) != 0) {
		const auto error = last_system_error();
		const auto code = error.value() == EWOULDBLOCK ||
			error.value() == EAGAIN ?
			UpdateBootstrapErrorCode::already_running :
			UpdateBootstrapErrorCode::lock;
		return std::unexpected(bootstrap_error(code, error));
	}
#endif
	return UpdateBootstrapLease(state_path, std::move(handle));
}

std::expected<UpdateLaunchPlan, UpdateBootstrapError>
prepare_update_launch(const UpdateBootstrapLease &lease)
{
	if(!lease || lease.state_path().empty()) {
		return std::unexpected(bootstrap_error(
			UpdateBootstrapErrorCode::invalid_path
		));
	}
	for(std::size_t attempt = 0U; attempt < transaction_attempts; ++attempt) {
		auto loaded = load_update_activation_state(lease.state_path());
		if(!loaded) {
			return std::unexpected(bootstrap_error(
				UpdateBootstrapErrorCode::store, {}, loaded.error()
			));
		}
		const auto &state = loaded->state;
		if(state.phase == UpdateActivationPhase::stable) {
			return launch_plan(state.active, false);
		}
		if(state.phase == UpdateActivationPhase::pending) {
			auto begun = begin_update_trial(state);
			if(!begun) {
				return std::unexpected(bootstrap_error(
					UpdateBootstrapErrorCode::transition, {}, {}, begun.error()
				));
			}
			auto committed = commit_update_activation_state(
				lease.state_path(), begun->state
			);
			if(!committed) {
				if(retryable(committed.error())) {
					continue;
				}
				return std::unexpected(bootstrap_error(
					UpdateBootstrapErrorCode::store, {}, committed.error()
				));
			}
			return UpdateLaunchPlan{
				.source = UpdateLaunchSource::trial_generation,
				.generation = begun->token.generation,
				.trial_token = begun->token,
				.generation_directory = update_generation_directory_name(
					begun->token.generation
				),
			};
		}
		auto recovered = recover_interrupted_update_trial(state);
		if(!recovered) {
			return std::unexpected(bootstrap_error(
				UpdateBootstrapErrorCode::transition, {}, {}, recovered.error()
			));
		}
		auto committed = commit_update_activation_state(
			lease.state_path(), *recovered
		);
		if(!committed) {
			if(retryable(committed.error())) {
				continue;
			}
			return std::unexpected(bootstrap_error(
				UpdateBootstrapErrorCode::store, {}, committed.error()
			));
		}
		return launch_plan(recovered->active, true);
	}
	return std::unexpected(bootstrap_error(
		UpdateBootstrapErrorCode::retry_exhausted
	));
}

std::expected<void, UpdateBootstrapError>
acknowledge_update_launch(
	const fs::path &state_path,
	const UpdateTrialToken &token
)
{
	if(!valid_token(token)) {
		return std::unexpected(bootstrap_error(
			UpdateBootstrapErrorCode::transition, {}, {},
			UpdateActivationTransitionError::stale_token
		));
	}
	for(std::size_t attempt = 0U; attempt < transaction_attempts; ++attempt) {
		auto loaded = load_update_activation_state(state_path);
		if(!loaded) {
			return std::unexpected(bootstrap_error(
				UpdateBootstrapErrorCode::store, {}, loaded.error()
			));
		}
		if(loaded->state.phase == UpdateActivationPhase::stable &&
			loaded->state.active == token.generation &&
			token.revision != std::numeric_limits<std::uint64_t>::max() &&
			loaded->state.revision == token.revision + 1U) {
			return {};
		}
		auto acknowledged = acknowledge_update_trial(loaded->state, token);
		if(!acknowledged) {
			return std::unexpected(bootstrap_error(
				UpdateBootstrapErrorCode::transition, {}, {},
				acknowledged.error()
			));
		}
		auto committed = commit_update_activation_state(
			state_path, *acknowledged
		);
		if(!committed) {
			if(retryable(committed.error())) {
				continue;
			}
			return std::unexpected(bootstrap_error(
				UpdateBootstrapErrorCode::store, {}, committed.error()
			));
		}
		return {};
	}
	return std::unexpected(bootstrap_error(
		UpdateBootstrapErrorCode::retry_exhausted
	));
}

std::expected<void, UpdateBootstrapError>
fail_update_launch(
	const fs::path &state_path,
	const UpdateTrialToken &token
)
{
	if(!valid_token(token)) {
		return std::unexpected(bootstrap_error(
			UpdateBootstrapErrorCode::transition, {}, {},
			UpdateActivationTransitionError::stale_token
		));
	}
	for(std::size_t attempt = 0U; attempt < transaction_attempts; ++attempt) {
		auto loaded = load_update_activation_state(state_path);
		if(!loaded) {
			return std::unexpected(bootstrap_error(
				UpdateBootstrapErrorCode::store, {}, loaded.error()
			));
		}
		if(loaded->state.phase == UpdateActivationPhase::stable &&
			loaded->state.last_failed == token.generation &&
			token.revision != std::numeric_limits<std::uint64_t>::max() &&
			loaded->state.revision == token.revision + 1U) {
			return {};
		}
		auto failed = fail_update_trial(loaded->state, token);
		if(!failed) {
			return std::unexpected(bootstrap_error(
				UpdateBootstrapErrorCode::transition, {}, {}, failed.error()
			));
		}
		auto committed = commit_update_activation_state(state_path, *failed);
		if(!committed) {
			if(retryable(committed.error())) {
				continue;
			}
			return std::unexpected(bootstrap_error(
				UpdateBootstrapErrorCode::store, {}, committed.error()
			));
		}
		return {};
	}
	return std::unexpected(bootstrap_error(
		UpdateBootstrapErrorCode::retry_exhausted
	));
}

} // namespace th03::app

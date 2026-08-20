#ifndef TH03_APP_UPDATE_BOOTSTRAP_HPP
#define TH03_APP_UPDATE_BOOTSTRAP_HPP

#include "th03/app/update_activation.hpp"
#include "th03/app/update_activation_store.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace th03::app {

inline constexpr std::size_t update_trial_token_text_bytes = 107U;

enum class UpdateTrialTokenTextErrorCode : std::uint8_t {
	input_size,
	magic,
	separator,
	hex_character,
	invalid_token,
};

struct UpdateTrialTokenTextError final {
	UpdateTrialTokenTextErrorCode code =
		UpdateTrialTokenTextErrorCode::input_size;
	std::size_t offset = 0U;
};

[[nodiscard]] std::expected<std::string, UpdateTrialTokenTextError>
encode_update_trial_token(const UpdateTrialToken &token);

[[nodiscard]] std::expected<UpdateTrialToken, UpdateTrialTokenTextError>
decode_update_trial_token(std::string_view text) noexcept;

enum class UpdateLaunchSource : std::uint8_t {
	legacy_install,
	active_generation,
	trial_generation,
};

struct UpdateLaunchPlan final {
	UpdateLaunchSource source = UpdateLaunchSource::legacy_install;
	std::optional<UpdateGenerationIdentity> generation{};
	std::optional<UpdateTrialToken> trial_token{};
	std::string generation_directory{};
	bool recovered_interrupted_trial = false;
};

enum class UpdateBootstrapErrorCode : std::uint8_t {
	invalid_path,
	status,
	already_running,
	lock,
	store,
	transition,
	retry_exhausted,
};

struct UpdateBootstrapError final {
	UpdateBootstrapErrorCode code = UpdateBootstrapErrorCode::invalid_path;
	std::error_code system{};
	std::optional<UpdateActivationStoreError> store{};
	std::optional<UpdateActivationTransitionError> transition{};
};

struct UpdateBootstrapLeaseHandle;
struct UpdateBootstrapLeaseHandleDeleter final {
	void operator()(UpdateBootstrapLeaseHandle *handle) const noexcept;
};

// A bootstrap owner keeps this lease alive for the complete child lifetime.
// Consequently, a newly acquired lease proves that a durable trial from a
// prior owner is interrupted rather than merely still starting.
class UpdateBootstrapLease final {
public:
	UpdateBootstrapLease(const UpdateBootstrapLease &) = delete;
	UpdateBootstrapLease &operator=(const UpdateBootstrapLease &) = delete;
	UpdateBootstrapLease(UpdateBootstrapLease &&) noexcept;
	UpdateBootstrapLease &operator=(UpdateBootstrapLease &&) noexcept;
	~UpdateBootstrapLease();

	[[nodiscard]] explicit operator bool() const noexcept;
	[[nodiscard]] const std::filesystem::path &state_path() const noexcept;

private:
	UpdateBootstrapLease(
		std::filesystem::path state_path,
		std::unique_ptr<
			UpdateBootstrapLeaseHandle,
			UpdateBootstrapLeaseHandleDeleter
		> handle
	) noexcept;

	std::filesystem::path state_path_{};
	std::unique_ptr<
		UpdateBootstrapLeaseHandle,
		UpdateBootstrapLeaseHandleDeleter
	> handle_{};

	friend std::expected<UpdateBootstrapLease, UpdateBootstrapError>
	acquire_update_bootstrap_lease(const std::filesystem::path &);
};

[[nodiscard]] std::expected<UpdateBootstrapLease, UpdateBootstrapError>
acquire_update_bootstrap_lease(const std::filesystem::path &state_path);

// The caller must retain lease through process launch and the complete child
// lifetime. Pending state is durably changed to trial before a trial plan is
// returned. A prior trial is first failed and the old active/legacy target is
// returned; it is never retried implicitly on the same startup.
[[nodiscard]] std::expected<UpdateLaunchPlan, UpdateBootstrapError>
prepare_update_launch(const UpdateBootstrapLease &lease);

// These operations are safe for the child process while the parent retains
// the bootstrap lease. An exact already-committed result is idempotent.
[[nodiscard]] std::expected<void, UpdateBootstrapError>
acknowledge_update_launch(
	const std::filesystem::path &state_path,
	const UpdateTrialToken &token
);

[[nodiscard]] std::expected<void, UpdateBootstrapError>
fail_update_launch(
	const std::filesystem::path &state_path,
	const UpdateTrialToken &token
);

} // namespace th03::app

#endif

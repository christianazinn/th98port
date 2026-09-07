#include "th03/app/update/update_activation.hpp"
#include "th03/app/update/update_activation_store.hpp"
#include "th03/app/update/update_bootstrap.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <utility>

namespace {

using namespace th03::app;
namespace fs = std::filesystem;

std::size_t checks = 0U;
std::size_t failures = 0U;

void expect(bool condition, const char *label)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << label << '\n';
	}
}

[[nodiscard]] UpdateGenerationIdentity identity(
	std::uint64_t sequence,
	std::uint8_t seed
)
{
	UpdateGenerationIdentity result{.sequence = sequence};
	for(std::size_t index = 0U; index < result.artifact_sha256.size(); ++index) {
		result.artifact_sha256[index] = static_cast<std::uint8_t>(seed + index);
	}
	return result;
}

class TemporaryDirectory final {
public:
	TemporaryDirectory()
	{
		path_ = fs::temp_directory_path() / ("th03-update-bootstrap-" +
			std::to_string(
				std::chrono::steady_clock::now().time_since_epoch().count()
			));
		fs::create_directories(path_);
	}
	~TemporaryDirectory()
	{
		std::error_code ignored;
		fs::remove_all(path_, ignored);
	}
	[[nodiscard]] const fs::path &path() const noexcept { return path_; }

private:
	fs::path path_{};
};

[[nodiscard]] UpdateActivationState load_state(const fs::path &path)
{
	const auto loaded = load_update_activation_state(path);
	if(!loaded) {
		++failures;
		std::cerr << "FAIL: activation state did not load\n";
		return {};
	}
	return loaded->state;
}

} // namespace

int main()
{
	const auto first = identity(10U, 1U);
	const auto second = identity(11U, 33U);
	const UpdateTrialToken vector{
		.revision = 0x0123456789ABCDEFULL,
		.generation = first,
	};
	const auto encoded = encode_update_trial_token(vector);
	expect(encoded && encoded->size() == update_trial_token_text_bytes &&
		*encoded ==
		"T3TRL001:0123456789abcdef:000000000000000a:"
		"0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f20",
		"trial token has one frozen lowercase text representation");
	const auto decoded = encoded ? decode_update_trial_token(*encoded) :
		std::expected<UpdateTrialToken, UpdateTrialTokenTextError>{
			std::unexpected(UpdateTrialTokenTextError{})
		};
	expect(decoded && *decoded == vector,
		"trial token text round-trips exactly");
	if(encoded) {
		auto bad_separator = *encoded;
		bad_separator[25U] = '-';
		const auto rejected_separator = decode_update_trial_token(bad_separator);
		expect(!rejected_separator && rejected_separator.error().code ==
			UpdateTrialTokenTextErrorCode::separator,
			"trial token rejects a changed separator");
		auto uppercase = *encoded;
		uppercase[9U + 10U] = 'A';
		const auto rejected_uppercase = decode_update_trial_token(uppercase);
		expect(!rejected_uppercase && rejected_uppercase.error().code ==
			UpdateTrialTokenTextErrorCode::hex_character,
			"trial token rejects noncanonical uppercase hex");
		expect(!decode_update_trial_token(encoded->substr(1U)) &&
			decode_update_trial_token(encoded->substr(1U)).error().code ==
				UpdateTrialTokenTextErrorCode::input_size,
			"trial token rejects the wrong exact length");
	}
	UpdateTrialToken invalid{};
	expect(!encode_update_trial_token(invalid) &&
		encode_update_trial_token(invalid).error().code ==
			UpdateTrialTokenTextErrorCode::invalid_token,
		"invalid trial token cannot be transported");

	TemporaryDirectory temporary;
	const auto base = temporary.path() / "PODD.UPDATE";
	auto lease = acquire_update_bootstrap_lease(base);
	expect(lease.has_value() && static_cast<bool>(*lease),
		"first bootstrap owner acquires the lifetime lease");
	const auto duplicate_lease = acquire_update_bootstrap_lease(base);
	expect(!duplicate_lease && duplicate_lease.error().code ==
		UpdateBootstrapErrorCode::already_running,
		"second bootstrap owner cannot overlap the child lifetime");
	const auto legacy = lease ? prepare_update_launch(*lease) :
		std::expected<UpdateLaunchPlan, UpdateBootstrapError>{
			std::unexpected(UpdateBootstrapError{})
		};
	expect(legacy && legacy->source == UpdateLaunchSource::legacy_install &&
		!legacy->generation && !legacy->trial_token &&
		legacy->generation_directory.empty(),
		"absent activation state selects the legacy install");

	const auto nominated_first = nominate_update_generation(
		load_state(base), first
	);
	expect(nominated_first &&
		commit_update_activation_state(base, *nominated_first),
		"first authenticated generation is nominated durably");
	const auto trial_first = lease ? prepare_update_launch(*lease) :
		std::expected<UpdateLaunchPlan, UpdateBootstrapError>{
			std::unexpected(UpdateBootstrapError{})
		};
	expect(trial_first &&
		trial_first->source == UpdateLaunchSource::trial_generation &&
		trial_first->generation == first && trial_first->trial_token &&
		trial_first->generation_directory ==
			"PODD-GEN-10-0102030405060708" &&
		load_state(base).phase == UpdateActivationPhase::trial,
		"pending generation is durably marked trial before launch plan returns");
	if(trial_first && trial_first->trial_token) {
		auto stale = *trial_first->trial_token;
		--stale.revision;
		const auto rejected = acknowledge_update_launch(base, stale);
		expect(!rejected && rejected.error().code ==
			UpdateBootstrapErrorCode::transition &&
			rejected.error().transition ==
				UpdateActivationTransitionError::stale_token,
			"stale transported token cannot acknowledge the trial");
		const auto acknowledged = acknowledge_update_launch(
			base, *trial_first->trial_token
		);
		expect(acknowledged.has_value(),
			"exact child health token acknowledges the trial");
		expect(acknowledge_update_launch(
			base, *trial_first->trial_token
		).has_value(), "exact acknowledgement retry is idempotent");
	}
	const auto active_first = load_state(base);
	expect(active_first.phase == UpdateActivationPhase::stable &&
		active_first.active == first,
		"acknowledged trial becomes the active generation");
	const auto active_plan = lease ? prepare_update_launch(*lease) :
		std::expected<UpdateLaunchPlan, UpdateBootstrapError>{
			std::unexpected(UpdateBootstrapError{})
		};
	expect(active_plan &&
		active_plan->source == UpdateLaunchSource::active_generation &&
		active_plan->generation == first && !active_plan->trial_token,
		"stable state selects active generation without a trial token");

	const auto nominated_second = nominate_update_generation(
		load_state(base), second
	);
	expect(nominated_second &&
		commit_update_activation_state(base, *nominated_second),
		"second generation is nominated");
	const auto trial_second = lease ? prepare_update_launch(*lease) :
		std::expected<UpdateLaunchPlan, UpdateBootstrapError>{
			std::unexpected(UpdateBootstrapError{})
		};
	expect(trial_second && trial_second->trial_token &&
		trial_second->generation == second,
		"second generation enters a trial");

	// Releasing this lease models a bootstrap process dying with its child
	// before an acknowledgement. A new owner can now prove interruption.
	lease = std::unexpected(UpdateBootstrapError{});
	auto recovered_lease = acquire_update_bootstrap_lease(base);
	expect(recovered_lease.has_value(),
		"new bootstrap owner acquires lease after predecessor exits");
	const auto recovered = recovered_lease ? prepare_update_launch(
		*recovered_lease
	) : std::expected<UpdateLaunchPlan, UpdateBootstrapError>{
		std::unexpected(UpdateBootstrapError{})
	};
	expect(recovered && recovered->recovered_interrupted_trial &&
		recovered->source == UpdateLaunchSource::active_generation &&
		recovered->generation == first && !recovered->trial_token,
		"interrupted trial rolls back and launches prior active generation");
	const auto recovered_state = load_state(base);
	expect(recovered_state.phase == UpdateActivationPhase::stable &&
		recovered_state.active == first &&
		recovered_state.last_failed == second &&
		recovered_state.failed_trials == 1U,
		"interrupted trial records the exact failed generation once");

	const auto retry_second = nominate_update_generation(
		recovered_state, second
	);
	expect(retry_second &&
		commit_update_activation_state(base, *retry_second),
		"exact failed generation can be nominated for explicit retry");
	const auto retry_trial = recovered_lease ? prepare_update_launch(
		*recovered_lease
	) : std::expected<UpdateLaunchPlan, UpdateBootstrapError>{
		std::unexpected(UpdateBootstrapError{})
	};
	expect(retry_trial && retry_trial->trial_token,
		"explicit retry begins one new trial epoch");
	if(retry_trial && retry_trial->trial_token) {
		const auto failed = fail_update_launch(base, *retry_trial->trial_token);
		expect(failed.has_value(), "exact launch failure commits rollback");
		expect(fail_update_launch(
			base, *retry_trial->trial_token
		).has_value(), "exact failure retry is idempotent");
	}
	const auto failed_state = load_state(base);
	expect(failed_state.phase == UpdateActivationPhase::stable &&
		failed_state.active == first && failed_state.last_failed == second &&
		failed_state.failed_trials == 2U,
		"explicit launch failure preserves active and increments once");
	if(trial_first && trial_first->trial_token) {
		const auto stale_old_child = acknowledge_update_launch(
			base, *trial_first->trial_token
		);
		expect(!stale_old_child && stale_old_child.error().code ==
			UpdateBootstrapErrorCode::transition,
			"old child cannot approve a later activation state");
	}

	const auto invalid_lease = acquire_update_bootstrap_lease({});
	expect(!invalid_lease && invalid_lease.error().code ==
		UpdateBootstrapErrorCode::invalid_path,
		"empty activation path cannot acquire a bootstrap lease");

	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}

#include "th03/app/update_activation.hpp"
#include "th03/app/update_activation_store.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <span>
#include <string>

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
		path_ = fs::temp_directory_path() / ("th03-update-activation-" +
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

void write_bytes(
	const fs::path &path,
	std::span<const std::byte> bytes
)
{
	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	output.write(
		reinterpret_cast<const char *>(bytes.data()),
		static_cast<std::streamsize>(bytes.size())
	);
}

} // namespace

int main()
{
	const UpdateActivationState initial{};
	const auto encoded_initial = encode_update_activation_state(initial);
	expect(encoded_initial.has_value(), "initial stable state encodes");
	if(encoded_initial) {
		const auto decoded = decode_update_activation_state(*encoded_initial);
		expect(decoded && *decoded == initial,
			"initial activation state round-trips exactly");
		auto corrupted = *encoded_initial;
		corrupted[32U] ^= std::byte{1U};
		const auto rejected = decode_update_activation_state(corrupted);
		expect(!rejected && rejected.error().code ==
			UpdateActivationCodecErrorCode::digest,
			"state digest rejects a mutated failure counter");
	}

	const auto first = identity(10U, 1U);
	const auto second = identity(11U, 33U);
	const auto replacement_same_sequence = identity(11U, 80U);
	expect(update_generation_directory_name(first) ==
		"PODD-GEN-10-0102030405060708",
		"generation directory is derived only from signed identity");
	const auto nominated = nominate_update_generation(initial, first);
	expect(nominated && nominated->revision == 1U &&
		nominated->phase == UpdateActivationPhase::pending &&
		nominated->pending == first && nominated->highest_sequence == 10U,
		"newer generation nomination creates one pending revision");
	const auto begun = nominated ? begin_update_trial(*nominated) :
		std::expected<UpdateTrialBeginResult, UpdateActivationTransitionError>{
			std::unexpected(UpdateActivationTransitionError::invalid_state)
		};
	expect(begun && begun->state.revision == 2U &&
		begun->state.phase == UpdateActivationPhase::trial &&
		begun->token.generation == first,
		"begin trial returns an exact epoch-bound token");
	if(begun) {
		auto stale = begun->token;
		--stale.revision;
		const auto rejected = acknowledge_update_trial(begun->state, stale);
		expect(!rejected && rejected.error() ==
			UpdateActivationTransitionError::stale_token,
			"stale child cannot acknowledge a trial");
	}
	const auto accepted = begun ? acknowledge_update_trial(
		begun->state, begun->token
	) : std::expected<UpdateActivationState, UpdateActivationTransitionError>{
		std::unexpected(UpdateActivationTransitionError::invalid_state)
	};
	expect(accepted && accepted->revision == 3U &&
		accepted->phase == UpdateActivationPhase::stable &&
		accepted->active == first && !accepted->pending && !accepted->previous,
		"exact health acknowledgement commits first active generation");

	const auto nominated_second = accepted ? nominate_update_generation(
		*accepted, second
	) : std::expected<UpdateActivationState, UpdateActivationTransitionError>{
		std::unexpected(UpdateActivationTransitionError::invalid_state)
	};
	const auto begun_second = nominated_second ? begin_update_trial(
		*nominated_second
	) : std::expected<UpdateTrialBeginResult, UpdateActivationTransitionError>{
		std::unexpected(UpdateActivationTransitionError::invalid_state)
	};
	const auto recovered = begun_second ? recover_interrupted_update_trial(
		begun_second->state
	) : std::expected<UpdateActivationState, UpdateActivationTransitionError>{
		std::unexpected(UpdateActivationTransitionError::invalid_state)
	};
	expect(recovered && recovered->revision == 6U &&
		recovered->phase == UpdateActivationPhase::stable &&
		recovered->active == first && recovered->last_failed == second &&
		recovered->failed_trials == 1U,
		"interrupted trial preserves active and records exact failed candidate");
	const auto retry = recovered ? nominate_update_generation(*recovered, second) :
		std::expected<UpdateActivationState, UpdateActivationTransitionError>{
			std::unexpected(UpdateActivationTransitionError::invalid_state)
		};
	expect(retry && retry->revision == 7U && retry->pending == second,
		"exact failed generation can be retried at the same signed sequence");
	const auto retry_begun = retry ? begin_update_trial(*retry) :
		std::expected<UpdateTrialBeginResult, UpdateActivationTransitionError>{
			std::unexpected(UpdateActivationTransitionError::invalid_state)
		};
	const auto retry_failed = retry_begun ? fail_update_trial(
		retry_begun->state, retry_begun->token
	) : std::expected<UpdateActivationState, UpdateActivationTransitionError>{
		std::unexpected(UpdateActivationTransitionError::invalid_state)
	};
	expect(retry_failed && retry_failed->revision == 9U &&
		retry_failed->active == first && retry_failed->last_failed == second &&
		retry_failed->failed_trials == 2U,
		"explicit trial failure preserves active and counts the failure once");
	const auto final_retry = retry_failed ? nominate_update_generation(
		*retry_failed, second
	) : std::expected<UpdateActivationState, UpdateActivationTransitionError>{
		std::unexpected(UpdateActivationTransitionError::invalid_state)
	};
	const auto final_begin = final_retry ? begin_update_trial(*final_retry) :
		std::expected<UpdateTrialBeginResult, UpdateActivationTransitionError>{
			std::unexpected(UpdateActivationTransitionError::invalid_state)
	};
	const auto final_accept = final_begin ? acknowledge_update_trial(
		final_begin->state, final_begin->token
	) : std::expected<UpdateActivationState, UpdateActivationTransitionError>{
		std::unexpected(UpdateActivationTransitionError::invalid_state)
	};
	expect(final_accept && final_accept->revision == 12U &&
		final_accept->active == second && final_accept->previous == first &&
		!final_accept->pending && !final_accept->last_failed,
		"successful retry retains the prior active generation for rollback");
	const auto rolled_back = final_accept ?
		rollback_unlaunchable_active_generation(*final_accept, second) :
		std::expected<UpdateActivationState, UpdateActivationTransitionError>{
			std::unexpected(UpdateActivationTransitionError::invalid_state)
		};
	expect(rolled_back && rolled_back->revision == 13U &&
		rolled_back->active == first && !rolled_back->previous &&
		rolled_back->last_failed == second &&
		rolled_back->failed_trials == 3U,
		"unlaunchable active generation rolls back to retained previous");
	if(rolled_back) {
		const auto legacy = rollback_unlaunchable_active_generation(
			*rolled_back, first
		);
		expect(legacy && !legacy->active && !legacy->previous &&
			legacy->last_failed == second && legacy->highest_sequence == 11U &&
			legacy->failed_trials == 4U,
			"second unlaunchable active rolls back to legacy without losing the newer failure identity");
		const auto stale = rollback_unlaunchable_active_generation(
			*rolled_back, second
		);
		expect(!stale && stale.error() ==
			UpdateActivationTransitionError::stale_token,
			"rollback rejects an obsolete active-generation identity");
	}
	if(recovered) {
		const auto reused = nominate_update_generation(
			*recovered, replacement_same_sequence
		);
		const auto older = nominate_update_generation(*recovered, first);
		expect(!reused && reused.error() ==
			UpdateActivationTransitionError::sequence_reused &&
			!older && older.error() ==
				UpdateActivationTransitionError::current_or_older,
			"same-sequence substitution and signed rollback are rejected");
	}
	if(retry) {
		const auto unchanged = recover_interrupted_update_trial(*retry);
		expect(unchanged && *unchanged == *retry,
			"unlaunched pending update is not treated as a failed trial");
	}

	UpdateActivationState exhausted = initial;
	exhausted.revision = std::numeric_limits<std::uint64_t>::max();
	const auto cannot_nominate = nominate_update_generation(exhausted, first);
	expect(!cannot_nominate && cannot_nominate.error() ==
		UpdateActivationTransitionError::revision_exhausted,
		"activation revision exhaustion fails closed");

	TemporaryDirectory temporary;
	const auto base = temporary.path() / "PODD.UPDATE";
	const auto missing = load_update_activation_state(base);
	expect(missing && !missing->file_present && missing->state == initial,
		"absent alternating state family loads the stable default");
	expect(nominated && commit_update_activation_state(base, *nominated),
		"first pending state commits to the empty family");
	const auto loaded_pending = load_update_activation_state(base);
	expect(loaded_pending && loaded_pending->file_present &&
		!loaded_pending->recovered_sibling && nominated &&
		loaded_pending->state == *nominated,
		"first committed state loads exactly");
	expect(begun && commit_update_activation_state(base, begun->state),
		"next trial revision commits to the alternate sibling");
	const auto loaded_trial = load_update_activation_state(base);
	expect(loaded_trial && begun && loaded_trial->state == begun->state,
		"greatest valid sibling revision wins");

	const auto newest_path = base.string() + ".b";
	std::array<std::byte, update_activation_file_bytes> garbage{};
	write_bytes(newest_path, garbage);
	const auto fallback = load_update_activation_state(base);
	expect(fallback && fallback->recovered_sibling && nominated &&
		fallback->state == *nominated,
		"corrupt newest sibling recovers the prior valid revision");
	if(fallback) {
		const auto resumed_trial = begin_update_trial(fallback->state);
		expect(resumed_trial && commit_update_activation_state(
			base, resumed_trial->state
		), "recovery can replace only the corrupt inactive sibling");
	}
	const auto stale_commit = nominated ?
		commit_update_activation_state(base, *nominated) :
		std::expected<void, UpdateActivationStoreError>{};
	expect(!stale_commit && stale_commit.error().code ==
		UpdateActivationStoreErrorCode::stale_revision,
		"stale state cannot overwrite a newer durable revision");

	TemporaryDirectory ambiguous_directory;
	const auto ambiguous_base = ambiguous_directory.path() / "PODD.UPDATE";
	if(nominated) {
		const auto committed = commit_update_activation_state(
			ambiguous_base, *nominated
		);
		expect(committed.has_value(),
			"ambiguous fixture base commits");
		auto alternate = *nominated;
		alternate.pending = replacement_same_sequence;
		alternate.highest_sequence = replacement_same_sequence.sequence;
		const auto alternate_bytes = encode_update_activation_state(alternate);
		if(alternate_bytes) {
			write_bytes(ambiguous_base.string() + ".b", *alternate_bytes);
		}
	}
	const auto ambiguous = load_update_activation_state(ambiguous_base);
	expect(!ambiguous && ambiguous.error().code ==
		UpdateActivationStoreErrorCode::ambiguous,
		"equal revisions with different identities fail closed");

	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}

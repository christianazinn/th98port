#include "th03/app/update_install.hpp"

#include "th03/app/update_activation.hpp"

#include <cstddef>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <utility>

namespace th03::app {
namespace {

constexpr std::size_t transaction_attempts = 8U;

[[nodiscard]] UpdateInstallError install_error(
	UpdateInstallErrorCode code
) noexcept
{
	return {.code = code};
}

[[nodiscard]] UpdateInstallResult result(
	UpdateInstallStatus status,
	const UpdateGenerationIdentity &generation,
	std::filesystem::path generation_path,
	UpdateActivationState state
)
{
	return {
		.status = status,
		.generation = generation,
		.generation_path = std::move(generation_path),
		.state = std::move(state),
	};
}

} // namespace

std::expected<UpdateInstallResult, UpdateInstallError>
install_update_for_next_launch(
	std::string manifest_url,
	std::span<const UpdatePinnedKey> pinned_keys,
	UpdatePlatform platform,
	UpdateChannel channel,
	const std::filesystem::path &activation_state,
	const std::filesystem::path &generation_root,
	const UpdateTransport &transport
)
{
	auto initial = load_update_activation_state(activation_state);
	if(!initial) {
		return std::unexpected(UpdateInstallError{
			.code = UpdateInstallErrorCode::activation_load,
			.activation = initial.error(),
		});
	}
	const auto current_sequence = initial->state.highest_sequence;
	auto acquired = acquire_update_payload(
		std::move(manifest_url), pinned_keys, platform, channel,
		current_sequence, transport
	);
	if(!acquired) {
		return std::unexpected(UpdateInstallError{
			.code = UpdateInstallErrorCode::acquisition,
			.acquisition = std::move(acquired.error()),
		});
	}
	auto generation_path = materialize_authenticated_update_package(
		acquired->manifest_bytes, pinned_keys, platform, channel,
		current_sequence, acquired->artifact_bytes, generation_root
	);
	if(!generation_path) {
		return std::unexpected(UpdateInstallError{
			.code = UpdateInstallErrorCode::materialization,
			.materialization = std::move(generation_path.error()),
		});
	}
	const UpdateGenerationIdentity generation{
		.sequence = acquired->manifest.sequence,
		.artifact_sha256 = acquired->manifest.artifact_sha256,
	};
	for(std::size_t attempt = 0U; attempt < transaction_attempts; ++attempt) {
		auto loaded = load_update_activation_state(activation_state);
		if(!loaded) {
			return std::unexpected(UpdateInstallError{
				.code = UpdateInstallErrorCode::activation_load,
				.activation = loaded.error(),
			});
		}
		if(loaded->state.pending == generation) {
			return result(
				UpdateInstallStatus::already_nominated, generation,
				*generation_path, std::move(loaded->state)
			);
		}
		if(loaded->state.active == generation &&
			loaded->state.phase == UpdateActivationPhase::stable) {
			return result(
				UpdateInstallStatus::already_active, generation,
				*generation_path, std::move(loaded->state)
			);
		}
		auto nominated = nominate_update_generation(
			loaded->state, generation
		);
		if(!nominated) {
			return std::unexpected(UpdateInstallError{
				.code = UpdateInstallErrorCode::transition,
				.transition = nominated.error(),
			});
		}
		auto committed = commit_update_activation_state(
			activation_state, *nominated
		);
		if(committed) {
			return result(
				UpdateInstallStatus::nominated, generation,
				*generation_path, std::move(*nominated)
			);
		}
		if(committed.error().code !=
			UpdateActivationStoreErrorCode::stale_revision) {
			return std::unexpected(UpdateInstallError{
				.code = UpdateInstallErrorCode::activation_commit,
				.activation = committed.error(),
			});
		}
	}
	return std::unexpected(install_error(
		UpdateInstallErrorCode::transaction_exhausted
	));
}

} // namespace th03::app

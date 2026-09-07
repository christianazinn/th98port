#ifndef TH03_APP_STARTUP_SEQUENCE_HPP
#define TH03_APP_STARTUP_SEQUENCE_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>

#include "th03/assets/types.hpp"
#include "th03/present/source_surface.hpp"

namespace th03::app {

// Executed once, in this order, before the New Classic product root. The second
// slot intentionally has no presenter yet; it is the stable insertion point
// for the later PoDD Port development-team credit animation.
enum class StartupSlot : std::uint8_t {
	zun_soft,
	project_credits,
	complete,
};

enum class StartupSequenceStatus : std::uint8_t {
	running,
	complete,
	quit_requested,
	failed,
};

struct StartupSequenceInput final {
	bool any_input = false;
	bool app_quit_requested = false;
};

struct StartupSequenceResult final {
	bool surface_changed = false;
	StartupSequenceStatus status = StartupSequenceStatus::running;
	StartupSlot slot = StartupSlot::zun_soft;
};

enum class StartupSequenceErrorCode : std::uint8_t {
	invalid_path,
	file_status_failed,
	file_missing,
	file_not_regular,
	file_size_failed,
	file_size_mismatch,
	file_open_failed,
	file_read_failed,
	digest_mismatch,
	asset_decode_failed,
	asset_profile_mismatch,
	asset_prepare_failed,
	allocation_failed,
	draw_failed,
};

struct StartupSequenceError final {
	StartupSequenceErrorCode code = StartupSequenceErrorCode::invalid_path;
	std::uintmax_t observed_size = 0U;
};

namespace detail {
struct StartupSequenceTestAccess;
}

// Source-derived port of the TH01-TH03 ZUN Soft logo executable. This owner is
// platform-neutral: the SDL shell supplies VSync ticks and physical input,
// while the scene owns only source pages, palette, objects, and cadence.
class StartupSequence final {
public:
	[[nodiscard]] static std::expected<
		StartupSequence,
		StartupSequenceError
	> load(const std::filesystem::path &logo_path);

	~StartupSequence();
	StartupSequence(const StartupSequence &) = delete;
	StartupSequence &operator=(const StartupSequence &) = delete;
	StartupSequence(StartupSequence &&) noexcept;
	StartupSequence &operator=(StartupSequence &&) noexcept;

	[[nodiscard]] StartupSequenceResult consume(
		StartupSequenceInput input
	) noexcept;
	[[nodiscard]] present::SourceSurface surface() const noexcept;
	[[nodiscard]] StartupSequenceStatus status() const noexcept;
	[[nodiscard]] StartupSlot slot() const noexcept;
	[[nodiscard]] std::uint16_t source_frame() const noexcept;

private:
	struct Impl;
	explicit StartupSequence(std::unique_ptr<Impl> impl) noexcept;

	friend struct detail::StartupSequenceTestAccess;
	[[nodiscard]] static std::expected<
		StartupSequence,
		StartupSequenceError
	> create_from_decoded_logo(assets::BfntSheet logo);

	std::unique_ptr<Impl> impl_;
};

} // namespace th03::app

#endif

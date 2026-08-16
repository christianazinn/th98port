#ifndef TH03_APP_PIXEL_ORACLE_IDENTITY_HPP
#define TH03_APP_PIXEL_ORACLE_IDENTITY_HPP

#include <cstdint>
#include <iosfwd>
#include <optional>
#include <string_view>

namespace th03::app {

// Architecture-neutral source ownership for the developer-only pixel oracle.
// Values deliberately mirror the three original executable domains.
enum class PixelOracleProcess : std::uint8_t {
	op = 1U,
	main = 2U,
	mainl = 3U,
};

// Canonical visible scene identity shared with the PC-98 capture producer.
enum class PixelOracleScene : std::uint8_t {
	unknown = 0U,
	title = 1U,
	options = 2U,
	character_select = 3U,
	practice_setup = 4U,
	key_config = 5U,
	replay_browser = 6U,
	replay_save = 7U,
	high_score = 8U,
	gameplay = 9U,
	pause = 10U,
	stage_splash = 11U,
	win = 12U,
	continue_menu = 13U,
	ending = 14U,
	staff_roll = 15U,
	music_room = 16U,
	game_over = 17U,
};

// A source boundary is retained even when it produces no changed scanout.
// More specific donor transactions can be added without changing T3PXH2 or
// T3RGB1; this first production slice covers the scheduler-visible source wait.
enum class PixelOracleBoundary : std::uint8_t {
	bootstrap,
	scene_enter,
	source_tick,
};

// Owner-supplied identity before route-wide invocation and visit ordinals are
// assigned. Host scheduler ticks and SDL publications are intentionally absent.
struct PixelOracleSourceIdentity final {
	PixelOracleProcess process = PixelOracleProcess::op;
	PixelOracleScene scene = PixelOracleScene::unknown;
	std::optional<std::uint32_t> logical_sample{};
	std::optional<std::uint32_t> replay_global_frame{};
	std::optional<std::uint32_t> round_frame{};
	PixelOracleBoundary boundary = PixelOracleBoundary::source_tick;
};

struct PixelOracleIdentity final {
	std::uint64_t event = 0U;
	std::uint64_t process_instance = 0U;
	PixelOracleProcess process = PixelOracleProcess::op;
	PixelOracleScene scene = PixelOracleScene::unknown;
	std::uint32_t scene_visit = 0U;
	std::optional<std::uint32_t> logical_sample{};
	std::optional<std::uint32_t> replay_global_frame{};
	std::optional<std::uint32_t> round_frame{};
	std::uint32_t commit = 0U;
	PixelOracleBoundary boundary = PixelOracleBoundary::source_tick;
	bool source_changed = false;

	[[nodiscard]] friend constexpr bool operator==(
		const PixelOracleIdentity &,
		const PixelOracleIdentity &
	) = default;
};

// Assigns route-wide identities deterministically. Every transition into a
// different original executable domain receives a new process instance; a
// direct replay capture seeds the omitted OP/MAINL prefix explicitly.
class PixelOracleIdentityTracker final {
public:
	explicit PixelOracleIdentityTracker(
		std::uint64_t first_process_instance = 0U
	) noexcept : next_process_instance_(first_process_instance) {}

	[[nodiscard]] PixelOracleIdentity bootstrap(
		PixelOracleSourceIdentity source
	);
	[[nodiscard]] PixelOracleIdentity consume(
		PixelOracleSourceIdentity source,
		bool source_changed
	);

private:
	[[nodiscard]] PixelOracleIdentity assign(
		PixelOracleSourceIdentity source,
		bool source_changed
	);

	std::uint64_t next_event_ = 0U;
	std::uint64_t next_process_instance_ = 0U;
	std::optional<PixelOracleProcess> previous_process{};
	std::uint64_t active_process_instance_ = 0U;
	std::optional<std::uint64_t> previous_scene_process_instance_{};
	std::optional<PixelOracleScene> previous_scene_{};
	std::uint32_t active_scene_visit_ = 0U;
	std::optional<PixelOracleIdentity> previous_identity_{};
};

[[nodiscard]] std::string_view pixel_oracle_process_name(
	PixelOracleProcess process
) noexcept;
[[nodiscard]] std::string_view pixel_oracle_scene_name(
	PixelOracleScene scene
) noexcept;
[[nodiscard]] std::string_view pixel_oracle_boundary_name(
	PixelOracleBoundary boundary
) noexcept;

// T3ALI1 is a cadence/identity sidecar. [publication] is absent for unchanged
// boundaries and otherwise names the successfully committed T3PXH2/T3RGB1 row.
[[nodiscard]] bool write_t3ali1_header(std::ostream &output);
[[nodiscard]] bool write_t3ali1_event(
	std::ostream &output,
	std::string_view fixture_sha256,
	const PixelOracleIdentity &identity,
	std::optional<std::uint64_t> publication
);

[[nodiscard]] bool valid_pixel_oracle_fixture_sha256(
	std::string_view fixture_sha256
) noexcept;
[[nodiscard]] constexpr bool pixel_oracle_identity_capture_hidden(
	std::string_view identity_log_path
) noexcept
{
	return !identity_log_path.empty();
}

// Direct UserPlayback capture omits OP selection drawing, not the source
// executable chain that produced the selected replay owner.
[[nodiscard]] constexpr std::uint64_t
pixel_oracle_direct_replay_first_process_instance(
	PixelOracleProcess process
) noexcept
{
	return process == PixelOracleProcess::main ? 2U :
		(process == PixelOracleProcess::mainl ? 1U : 0U);
}

} // namespace th03::app

#endif

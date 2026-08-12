#ifndef TH03_APP_NEW_CLASSIC_FRONTEND_HPP
#define TH03_APP_NEW_CLASSIC_FRONTEND_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>

#include "th03/app/host_menu_canvas.hpp"
#include "th03/app/replay_slot_store.hpp"
#include "th03/formats/replay.hpp"

namespace th03::app {

struct NewClassicUiInput final {
	bool up = false;
	bool down = false;
	bool left = false;
	bool right = false;
	bool confirm = false;
	bool cancel = false;
	bool app_quit_requested = false;
};

enum class NewClassicLandingChoice : std::uint8_t {
	play,
	replay,
	practice,
	key_config,
	options,
	back,
};

enum class NewClassicLandingStatus : std::uint8_t {
	running,
	selected,
	application_quit,
};

struct NewClassicLandingResult final {
	bool surface_changed = false;
	NewClassicLandingStatus status = NewClassicLandingStatus::running;
	NewClassicLandingChoice choice = NewClassicLandingChoice::play;
};

class NewClassicLandingMenu final {
public:
	NewClassicLandingMenu();

	[[nodiscard]] NewClassicLandingResult consume(
		NewClassicUiInput input
	) noexcept;
	[[nodiscard]] present::SourceSurface surface() const noexcept;
	[[nodiscard]] NewClassicLandingChoice choice() const noexcept;

private:
	void draw() noexcept;
	HostMenuCanvas canvas_{};
	NewClassicLandingChoice choice_ = NewClassicLandingChoice::play;
	NewClassicLandingStatus status_ = NewClassicLandingStatus::running;
	bool input_allowed_ = false;
};

enum class ReplayCatalogSlotState : std::uint8_t {
	empty,
	ready,
	invalid,
};

struct ReplayCatalogSlot final {
	ReplayCatalogSlotState state = ReplayCatalogSlotState::empty;
	std::array<char, 8U> name{};
	std::uint8_t game_mode = 0U;
	std::uint8_t rank = 0U;
	std::uint8_t stage_reached = 0U;
	std::uint8_t checkpoint_count = 0U;
	std::array<std::uint8_t, 4U> final_score{};
};

struct NewClassicReplayCatalog final {
	std::array<ReplayCatalogSlot, replay_slot_count> slots{};
};

[[nodiscard]] std::expected<
	NewClassicReplayCatalog,
	ReplaySlotStoreError
> scan_new_classic_replays(const std::filesystem::path &replay_root);

enum class NewClassicReplayBrowserStatus : std::uint8_t {
	running,
	play_selected,
	back,
	application_quit,
};

struct NewClassicReplayBrowserResult final {
	bool surface_changed = false;
	NewClassicReplayBrowserStatus status =
		NewClassicReplayBrowserStatus::running;
};

class NewClassicReplayBrowser final {
public:
	[[nodiscard]] static std::expected<
		std::unique_ptr<NewClassicReplayBrowser>,
		ReplaySlotStoreError
	> load(std::filesystem::path replay_root);

	[[nodiscard]] std::expected<
		NewClassicReplayBrowserResult,
		ReplaySlotStoreError
	> consume(NewClassicUiInput input);
	[[nodiscard]] present::SourceSurface surface() const noexcept;
	[[nodiscard]] std::uint8_t selected_slot() const noexcept;
	[[nodiscard]] std::size_t selected_checkpoint() const noexcept;
	[[nodiscard]] std::optional<formats::ReplayDocument>
	take_selected_document();

private:
	explicit NewClassicReplayBrowser(
		std::filesystem::path replay_root,
		NewClassicReplayCatalog catalog
	);
	void draw() noexcept;
	[[nodiscard]] std::expected<void, ReplaySlotStoreError>
	open_selected();

	std::filesystem::path replay_root_{};
	NewClassicReplayCatalog catalog_{};
	HostMenuCanvas canvas_{};
	std::uint8_t selected_slot_ = 0U;
	std::size_t selected_checkpoint_ = 0U;
	std::optional<formats::ReplayDocument> detail_document_{};
	NewClassicReplayBrowserStatus status_ =
		NewClassicReplayBrowserStatus::running;
	bool input_allowed_ = false;
};

enum class NewClassicReplaySaveStatus : std::uint8_t {
	running,
	save_requested,
	saved,
	discarded,
	application_quit,
};

struct NewClassicReplaySaveResult final {
	bool surface_changed = false;
	NewClassicReplaySaveStatus status = NewClassicReplaySaveStatus::running;
};

// UI-only save transaction. The caller performs the atomic commit when
// save_requested is returned, reports success/failure, and retains document
// ownership until success so endpoint failures are retryable.
class NewClassicReplaySaveMenu final {
public:
	explicit NewClassicReplaySaveMenu(formats::ReplayDocument document);

	[[nodiscard]] NewClassicReplaySaveResult consume(
		NewClassicUiInput input,
		const NewClassicReplayCatalog &catalog
	) noexcept;
	void report_commit(bool success) noexcept;
	[[nodiscard]] present::SourceSurface surface() const noexcept;
	[[nodiscard]] std::uint8_t selected_slot() const noexcept;
	[[nodiscard]] const formats::ReplayDocument &document() const noexcept;
	[[nodiscard]] formats::ReplayDocument take_document();
	[[nodiscard]] bool commit_failed() const noexcept;
	[[nodiscard]] std::array<char, 8U> edited_name() const noexcept;

private:
	friend struct NewClassicReplaySaveTestAccess;
	enum class Phase : std::uint8_t {
		prompt,
		name,
		slots,
		overwrite,
		failure,
	};
	void draw(const NewClassicReplayCatalog &catalog) noexcept;
	void apply_name() noexcept;

	formats::ReplayDocument document_{};
	HostMenuCanvas canvas_{};
	Phase phase_ = Phase::prompt;
	NewClassicReplaySaveStatus status_ = NewClassicReplaySaveStatus::running;
	std::array<char, 8U> name_{{'P','L','A','Y','E','R',' ',' '}};
	std::size_t name_position_ = 0U;
	std::size_t keyboard_position_ = 0U;
	std::uint8_t selected_slot_ = 0U;
	bool yes_ = true;
	bool input_allowed_ = false;
};

struct NewClassicReplaySaveTestAccess final {
	static void select_end(NewClassicReplaySaveMenu &menu) noexcept;
	static void set_slot(NewClassicReplaySaveMenu &menu, std::uint8_t slot)
		noexcept;
};

} // namespace th03::app

#endif

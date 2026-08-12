#ifndef TH03_APP_NEW_CLASSIC_FRONTEND_HPP
#define TH03_APP_NEW_CLASSIC_FRONTEND_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "th03/app/host_menu_canvas.hpp"
#include "th03/app/classic_config.hpp"
#include "th03/app/new_classic_key_config_store.hpp"
#include "th03/app/replay_slot_store.hpp"
#include "th03/formats/replay.hpp"

namespace th03::app {

struct NewClassicUiInput final {
	bool up = false;
	bool down = false;
	bool left = false;
	bool right = false;
	bool confirm = false;
	bool bomb = false;
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

enum class NewClassicOptionsStatus : std::uint8_t {
	running,
	key_config_requested,
	apply_requested,
	applied,
	application_quit,
};

struct NewClassicOptionsResult final {
	bool surface_changed = false;
	NewClassicOptionsStatus status = NewClassicOptionsStatus::running;
};

// Replay Patch's expanded option set. The caller proves whether the complete
// authenticated translated-resource profile is available; a missing profile
// is disclosed and can never leave a persisted partial-English selection.
class NewClassicOptionsMenu final {
public:
	explicit NewClassicOptionsMenu(
		ClassicConfigOptions config,
		bool english_available = false
	) noexcept;
	[[nodiscard]] NewClassicOptionsResult consume(
		NewClassicUiInput input
	) noexcept;
	void report_commit(bool success) noexcept;
	void resume_after_key_config() noexcept;
	void synchronize_legacy_autofire(bool enabled) noexcept;
	[[nodiscard]] present::SourceSurface surface() const noexcept;
	[[nodiscard]] const ClassicConfigOptions &config() const noexcept;
	[[nodiscard]] NewClassicOptionsStatus status() const noexcept;

private:
	void draw() noexcept;
	HostMenuCanvas canvas_{};
	ClassicConfigOptions config_{};
	std::size_t selected_ = 0U;
	NewClassicOptionsStatus status_ = NewClassicOptionsStatus::running;
	bool input_allowed_ = false;
	bool commit_failure_ = false;
	bool english_unavailable_notice_ = false;
	bool english_available_ = false;
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
	bool practice = false;
	std::int8_t p1_paletted_character = 0;
	formats::ReplayEndReason end_reason = formats::ReplayEndReason::none;
	std::uint8_t practice_stage = 0U;
	std::uint8_t practice_round = 0U;
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

enum class NewClassicReplayDetailPage : std::uint8_t {
	splits,
	clear_bonuses,
	timers,
};

// Replay Patch 0.4.9's frozen display conversion. This is reporting metadata;
// the exact rational scheduler remains authoritative for runtime cadence.
[[nodiscard]] std::string new_classic_replay_timer_label(
	std::uint32_t real_frames
);

struct NewClassicReplayBrowserResult final {
	bool surface_changed = false;
	NewClassicReplayBrowserStatus status =
		NewClassicReplayBrowserStatus::running;
};

struct NewClassicReplayBrowserTestAccess;

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
	[[nodiscard]] NewClassicReplayDetailPage detail_page() const noexcept;
	[[nodiscard]] std::optional<formats::ReplayDocument>
	take_selected_document();

private:
	friend struct NewClassicReplayBrowserTestAccess;
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
	NewClassicReplayDetailPage detail_page_ =
		NewClassicReplayDetailPage::splits;
	std::optional<formats::ReplayDocument> detail_document_{};
	bool checkpoint_focus_ = true;
	bool practice_settings_modal_ = false;
	NewClassicReplayBrowserStatus status_ =
		NewClassicReplayBrowserStatus::running;
	bool input_allowed_ = false;
};

struct NewClassicReplayBrowserTestAccess final {
	[[nodiscard]] static bool checkpoint_focus(
		const NewClassicReplayBrowser &browser
	) noexcept;
	[[nodiscard]] static bool practice_settings_visible(
		const NewClassicReplayBrowser &browser
	) noexcept;
	[[nodiscard]] static std::size_t physical_row_count(
		const NewClassicReplayBrowser &browser
	) noexcept;
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
	void report_commit(
		bool success,
		const NewClassicReplayCatalog &catalog
	) noexcept;
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
		quit_confirm,
		failure,
		saved_wait,
	};
	void draw(const NewClassicReplayCatalog &catalog) noexcept;
	void apply_name() noexcept;

	formats::ReplayDocument document_{};
	HostMenuCanvas canvas_{};
	Phase phase_ = Phase::prompt;
	NewClassicReplaySaveStatus status_ = NewClassicReplaySaveStatus::running;
	std::array<char, 8U> name_{{' ',' ',' ',' ',' ',' ',' ',' '}};
	std::size_t name_position_ = 0U;
	std::size_t keyboard_position_ = 0U;
	std::uint8_t selected_slot_ = 0U;
	bool yes_ = true;
	Phase return_phase_ = Phase::prompt;
	bool return_yes_ = true;
	bool input_allowed_ = false;
};

struct NewClassicReplaySaveTestAccess final {
	static void select_end(NewClassicReplaySaveMenu &menu) noexcept;
	static void set_slot(NewClassicReplaySaveMenu &menu, std::uint8_t slot)
		noexcept;
	[[nodiscard]] static bool in_name(
		const NewClassicReplaySaveMenu &menu
	) noexcept;
	[[nodiscard]] static bool in_slots(
		const NewClassicReplaySaveMenu &menu
	) noexcept;
	[[nodiscard]] static bool in_overwrite(
		const NewClassicReplaySaveMenu &menu
	) noexcept;
	[[nodiscard]] static bool in_quit_confirm(
		const NewClassicReplaySaveMenu &menu
	) noexcept;
	[[nodiscard]] static std::size_t name_position(
		const NewClassicReplaySaveMenu &menu
	) noexcept;
};

enum class NewClassicKeyConfigStatus : std::uint8_t {
	running,
	apply_requested,
	applied,
	back,
	application_quit,
};

struct NewClassicKeyConfigInput final {
	NewClassicUiInput ui{};
	std::optional<std::uint16_t> scancode_pressed{};
	bool any_scancode_held = false;
};

struct NewClassicKeyConfigResult final {
	bool surface_changed = false;
	NewClassicKeyConfigStatus status = NewClassicKeyConfigStatus::running;
};

using NewClassicHostKeyName = std::string_view (*)(
	void *context,
	std::uint16_t key
) noexcept;

// Replay Patch's three-page keyboard transaction, expressed in host scancodes.
// Persistence and SDL remapping remain caller-owned and retryable.
class NewClassicKeyConfigMenu final {
public:
	NewClassicKeyConfigMenu(
		NewClassicKeyConfig config,
		NewClassicKeyConfig defaults,
		std::uint16_t maximum_key,
		std::uint16_t capture_cancel_key,
		NewClassicHostKeyName key_name,
		void *key_name_context = nullptr
	) noexcept;

	[[nodiscard]] NewClassicKeyConfigResult consume(
		NewClassicKeyConfigInput input
	) noexcept;
	void report_commit(bool success) noexcept;
	[[nodiscard]] present::SourceSurface surface() const noexcept;
	[[nodiscard]] const NewClassicKeyConfig &config() const noexcept;
	[[nodiscard]] NewClassicKeyConfigStatus status() const noexcept;

private:
	enum class Page : std::uint8_t { p1, p2, story };
	enum class Phase : std::uint8_t {
		menu,
		capture_wait_release,
		capture_wait_press,
		capture_wait_final_release,
		discard,
		commit_failure,
	};
	void draw() noexcept;
	void set_page(Page page) noexcept;
	void restore_defaults() noexcept;
	void assign(std::uint16_t key) noexcept;
	[[nodiscard]] std::size_t row_count() const noexcept;
	[[nodiscard]] std::optional<std::size_t> binding_index() const noexcept;

	NewClassicKeyConfig original_{};
	NewClassicKeyConfig config_{};
	NewClassicKeyConfig defaults_{};
	std::uint16_t maximum_key_ = 0U;
	std::uint16_t capture_cancel_key_ = 0U;
	NewClassicHostKeyName key_name_ = nullptr;
	void *key_name_context_ = nullptr;
	HostMenuCanvas canvas_{};
	Page page_ = Page::story;
	Phase phase_ = Phase::menu;
	std::size_t selected_ = 0U;
	std::optional<std::size_t> capture_binding_{};
	NewClassicKeyConfigStatus status_ = NewClassicKeyConfigStatus::running;
	bool input_allowed_ = false;
};

} // namespace th03::app

#endif

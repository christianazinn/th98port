#ifndef TH03_APP_NEW_CLASSIC_FRONTEND_HPP
#define TH03_APP_NEW_CLASSIC_FRONTEND_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "th03/app/authentic_text.hpp"
#include "th03/app/classic_config.hpp"
#include "th03/app/classic_score.hpp"
#include "th03/app/new_classic_assets.hpp"
#include "th03/app/new_classic_key_config_store.hpp"
#include "th03/app/new_classic_online.hpp"
#include "th03/app/replay_slot_store.hpp"
#include "th03/formats/replay.hpp"
#include "th03/present/graphics_text.hpp"
#include "th03/present/superimpose.hpp"

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

using NewClassicHostKeyName = std::string_view (*)(
	void *context,
	std::uint16_t key
) noexcept;

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
	> load(
		std::filesystem::path replay_root,
		std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets
	);

	[[nodiscard]] std::expected<
		NewClassicReplayBrowserResult,
		ReplaySlotStoreError
	> consume(NewClassicUiInput input);
	[[nodiscard]] present::SourceSurface surface() const noexcept;
	[[nodiscard]] std::uint8_t selected_slot() const noexcept;
	[[nodiscard]] std::size_t selected_checkpoint() const noexcept;
	[[nodiscard]] NewClassicReplayDetailPage detail_page() const noexcept;
	// Mirrors OP's game-exit handoff before replay playback: blacken and clear
	// both source pages before the synchronous hidden MAINL/MAIN reconstruction.
	[[nodiscard]] bool prepare_playback_transition() noexcept;
	[[nodiscard]] std::optional<formats::ReplayDocument>
	take_selected_document();

private:
	friend struct NewClassicReplayBrowserTestAccess;
	explicit NewClassicReplayBrowser(
		std::filesystem::path replay_root,
		NewClassicReplayCatalog catalog,
		std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets
	);
	void draw() noexcept;
	void draw_patch() noexcept;
	[[nodiscard]] std::expected<void, ReplaySlotStoreError>
	open_selected();

	std::filesystem::path replay_root_{};
	NewClassicReplayCatalog catalog_{};
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets_{};
	NewClassicPatchSurface patch_surface_{};
	std::uint8_t selected_slot_ = 0U;
	std::uint8_t top_slot_ = 0U;
	std::size_t selected_checkpoint_ = 0U;
	NewClassicReplayDetailPage detail_page_ =
		NewClassicReplayDetailPage::splits;
	std::optional<formats::ReplayDocument> detail_document_{};
	bool checkpoint_focus_ = true;
	bool practice_settings_modal_ = false;
	bool fade_in_active_ = false;
	std::uint8_t fade_boundary_ = 0U;
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

enum class NewClassicHighScoreMenuStatus : std::uint8_t {
	running,
	local_selected,
	back,
	application_quit,
};

struct NewClassicHighScoreMenuResult final {
	bool surface_changed = false;
	NewClassicHighScoreMenuStatus status =
		NewClassicHighScoreMenuStatus::running;
};

enum class NewClassicOnlineReplayDownloadOutcome : std::uint8_t {
	downloaded,
	failed,
	unavailable,
};

// New Classic-only source-styled hub for local and cached online score views.
// Every online row was already validated by the T3ONL002 decoder. The owner
// may emit one immutable replay-download intent, but never performs network or
// filesystem I/O during source-time scene advancement. An empty board set is
// invalid so the existing direct local HiScore route remains the only visible
// path while offline/unconfigured.
class NewClassicHighScoreMenu final {
public:
	NewClassicHighScoreMenu(
		std::vector<NewClassicOnlineCacheDocument> boards,
		std::uint8_t rank,
		std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets
	);

	[[nodiscard]] bool valid() const noexcept;
	[[nodiscard]] NewClassicHighScoreMenuResult consume(
		NewClassicUiInput input
	) noexcept;
	[[nodiscard]] present::SourceSurface surface() const noexcept;
	[[nodiscard]] NewClassicHighScoreMenuStatus status() const noexcept;
	[[nodiscard]] bool viewing_online() const noexcept;
	[[nodiscard]] bool viewing_detail() const noexcept;
	[[nodiscard]] std::size_t selected_board() const noexcept;
	[[nodiscard]] std::size_t selected_entry() const noexcept;
	[[nodiscard]] std::optional<NewClassicOnlineReplayRecord>
	take_replay_download_request() noexcept;
	[[nodiscard]] bool report_replay_download(
		const NewClassicOnlineReplayId &replay_id,
		NewClassicOnlineReplayDownloadOutcome outcome
	) noexcept;

private:
	enum class Page : std::uint8_t { hub, board, detail };
	enum class DownloadState : std::uint8_t {
		idle,
		pending,
		failed,
		unavailable,
	};
	void draw() noexcept;
	void draw_hub() noexcept;
	void draw_board() noexcept;
	void draw_detail() noexcept;
	[[nodiscard]] const NewClassicOnlineReplayRecord *selected_replay() const
		noexcept;

	std::vector<NewClassicOnlineCacheDocument> boards_{};
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets_{};
	NewClassicPatchSurface patch_surface_{};
	Page page_ = Page::hub;
	NewClassicHighScoreMenuStatus status_ =
		NewClassicHighScoreMenuStatus::running;
	std::size_t hub_selection_ = 0U;
	std::size_t board_index_ = 0U;
	std::size_t selected_entry_ = 0U;
	std::size_t top_entry_ = 0U;
	DownloadState download_state_ = DownloadState::idle;
	std::optional<NewClassicOnlineReplayId> download_target_{};
	std::optional<NewClassicOnlineReplayRecord> download_request_{};
	bool redraw_pending_ = false;
	bool valid_ = false;
	bool fade_in_active_ = true;
	std::uint8_t fade_boundary_ = 0U;
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

enum class NewClassicReplaySaveArtErrorCode : std::uint8_t {
	normal_glyphs_failed,
	highlight_glyphs_failed,
	invalid_glyph_geometry,
	font_failed,
	character_name_failed,
};

struct NewClassicReplaySaveArtError final {
	NewClassicReplaySaveArtErrorCode code =
		NewClassicReplaySaveArtErrorCode::normal_glyphs_failed;
	std::optional<assets::AssetError> asset_error{};
	std::optional<AuthenticTextError> font_error{};
	std::optional<present::GraphicsTextError> text_error{};
};

// Prepared source REGI patterns and FONT.ROM character names used by the
// Replay Patch save screen. Preparation reproduces the patch's post-load
// REGI color-plane mutations while preserving master.lib's opacity masks.
class NewClassicReplaySaveArt final {
public:
	NewClassicReplaySaveArt(NewClassicReplaySaveArt &&) noexcept = default;
	NewClassicReplaySaveArt &operator=(NewClassicReplaySaveArt &&) noexcept =
		default;
	NewClassicReplaySaveArt(const NewClassicReplaySaveArt &) = delete;
	NewClassicReplaySaveArt &operator=(const NewClassicReplaySaveArt &) = delete;

	[[nodiscard]] static std::expected<
		std::shared_ptr<const NewClassicReplaySaveArt>,
		NewClassicReplaySaveArtError
	> prepare(
		std::span<const std::byte> normal_bfnt,
		std::span<const std::byte> highlight_bfnt,
		std::span<const std::byte> font_rom
	);

private:
	friend class NewClassicReplaySaveMenu;
	present::PreparedSuperimposeSheet normal_;
	present::PreparedSuperimposeSheet highlight_;
	std::array<
		std::optional<present::PreparedGraphicsText8x16>,
		10U
	> character_names_{};

	NewClassicReplaySaveArt(
		present::PreparedSuperimposeSheet normal,
		present::PreparedSuperimposeSheet highlight
	) noexcept;
};

using NewClassicDosDateProvider = std::uint16_t (*)(void *) noexcept;

// UI-only save transaction. The caller performs the atomic commit when
// save_requested is returned, reports success/failure, and retains document
// ownership until success so endpoint failures are retryable. AZINN supplies
// SLB1B/SLB1 and MNUFONT; exact name-entry glyph art additionally requires the
// caller's prepared REGI1.BFT/REGI2.BFT MAINL resources (not AZINN members).
class NewClassicReplaySaveMenu final {
public:
	explicit NewClassicReplaySaveMenu(
		formats::ReplayDocument document,
		std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
		std::shared_ptr<const NewClassicReplaySaveArt> save_art = {},
		NewClassicDosDateProvider date_provider = nullptr,
		void *date_provider_context = nullptr
	);

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
		saved_wait,
	};
	void draw(const NewClassicReplayCatalog &catalog) noexcept;
	void draw_patch(const NewClassicReplayCatalog &catalog) noexcept;
	void apply_name() noexcept;
	void capture_date() noexcept;

	formats::ReplayDocument document_{};
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets_{};
	std::shared_ptr<const NewClassicReplaySaveArt> save_art_{};
	NewClassicDosDateProvider date_provider_ = nullptr;
	void *date_provider_context_ = nullptr;
	NewClassicPatchSurface patch_surface_{};
	Phase phase_ = Phase::prompt;
	NewClassicReplaySaveStatus status_ = NewClassicReplaySaveStatus::running;
	std::array<char, 8U> name_{{' ',' ',' ',' ',' ',' ',' ',' '}};
	std::size_t name_position_ = 0U;
	std::size_t keyboard_position_ = 0U;
	std::uint8_t selected_slot_ = 0U;
	std::uint8_t top_slot_ = 0U;
	bool yes_ = true;
	Phase return_phase_ = Phase::prompt;
	bool return_yes_ = true;
	bool input_allowed_ = false;
	bool commit_failed_ = false;
	bool fade_in_active_ = false;
	std::uint8_t fade_boundary_ = 0U;
	std::optional<Phase> drawn_phase_{};
	std::optional<present::Page> dialog_restore_page_{};
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

} // namespace th03::app

#endif

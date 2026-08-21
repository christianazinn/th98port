#ifndef TH03_APP_NEW_CLASSIC_NETPLAY_ROOM_HPP
#define TH03_APP_NEW_CLASSIC_NETPLAY_ROOM_HPP

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "th03/app/classic_audio.hpp"
#include "th03/app/new_classic_assets.hpp"
#include "th03/app/new_classic_frontend.hpp"
#include "th03/app/new_classic_netplay_match_contract.hpp"
#include "th03/app/new_classic_settings.hpp"

namespace th03::app {

inline constexpr std::array<ClassicAudioCommand, 3U>
	new_classic_netplay_match_ready_audio_commands{{
		{
			.type = ClassicAudioCommandType::stop_song,
			.cue = ClassicMusicCue::menu_main,
		},
		{
			.type = ClassicAudioCommandType::load_song,
			.cue = ClassicMusicCue::winner,
		},
		{
			.type = ClassicAudioCommandType::play_song,
			.cue = ClassicMusicCue::winner,
		},
	}};

inline constexpr std::array<ClassicAudioCommand, 3U>
	new_classic_netplay_menu_audio_restore_commands{{
		{
			.type = ClassicAudioCommandType::stop_song,
		},
		{
			.type = ClassicAudioCommandType::load_song,
			.cue = ClassicMusicCue::menu_main,
		},
		{
			.type = ClassicAudioCommandType::play_song,
			.cue = ClassicMusicCue::menu_main,
		},
	}};

struct NewClassicNetplayMatchReadyAudioGate final {
	bool observed_jingle = false;
	bool complete = false;

	constexpr void advance(ClassicAudioStatus audio) noexcept
	{
		if(complete) {
			return;
		}
		if(!audio.bgm_active) {
			complete = true;
			return;
		}
		if(audio.song_playing) {
			observed_jingle = true;
		} else if(observed_jingle) {
			complete = true;
		}
	}
};

enum class NewClassicNetplayRoomLaunchMode : std::uint8_t {
	quick_match,
	host,
	join,
};

enum class NewClassicNetplayRoomTransport : std::uint8_t {
	online_service,
	direct_udp,
};

struct NewClassicNetplayRoomLaunch final {
	NewClassicNetplayRoomLaunchMode mode =
		NewClassicNetplayRoomLaunchMode::host;
	NewClassicNetplayRoomTransport transport =
		NewClassicNetplayRoomTransport::online_service;
	std::uint16_t host_port = 31'415U;
	std::string join_endpoint{};
	std::uint8_t character = 1U;
	std::uint8_t palette = 1U;
	std::uint8_t rank = 1U;
	bool relay_only = false;
	std::uint8_t input_delay_frames = 0U;
};

struct NewClassicNetplayRoomInput final {
	NewClassicUiInput ui{};
	std::optional<char> text_character{};
	bool erase = false;
};

enum class NewClassicNetplayRoomStatus : std::uint8_t {
	running,
	back,
	profile_requested,
	launch_requested,
	application_quit,
};

struct NewClassicNetplayRoomResult final {
	bool surface_changed = false;
	NewClassicNetplayRoomStatus status =
		NewClassicNetplayRoomStatus::running;
};

// Source-styled product owner for the direct UDP preview. Browsing and editing
// are pure: the platform may create a socket only after take_launch() returns.
class NewClassicNetplayRoom final {
public:
	explicit NewClassicNetplayRoom(
		std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
		NewClassicLanguageAssetProfile language =
			NewClassicLanguageAssetProfile::english,
		std::uint8_t initial_rank = 1U,
		NewClassicNetplayPreferences preferences = {}
	);

	[[nodiscard]] bool valid() const noexcept;
	[[nodiscard]] NewClassicNetplayRoomResult consume(
		NewClassicNetplayRoomInput input
	) noexcept;
	[[nodiscard]] present::SourceSurface surface() const noexcept;
	[[nodiscard]] NewClassicNetplayRoomStatus status() const noexcept;
	[[nodiscard]] bool text_entry_active() const noexcept;
	[[nodiscard]] std::optional<NewClassicNetplayRoomLaunch>
		take_launch() noexcept;
	// A failed pre-game connection stays inside this retained owner. The next
	// fresh interface input returns to the launch caller with its values intact.
	[[nodiscard]] bool show_launch_failure() noexcept;
	[[nodiscard]] bool show_profile_failure() noexcept;
	// A canceled platform connection is a child of the launch screen, not a
	// terminal product route. Restore the exact caller and retain all entered
	// values so the player can retry or back out one level.
	[[nodiscard]] bool resume_after_launch() noexcept;
	[[nodiscard]] bool resume_after_profile() noexcept;
	[[nodiscard]] NewClassicNetplayPreferences preferences() const noexcept;

private:
	enum class Screen : std::uint8_t {
		menu,
		player_setup,
		host_confirmation,
		join_entry,
		port_entry,
		connection_failed,
		settings,
	};

	[[nodiscard]] bool draw() noexcept;
	[[nodiscard]] bool valid_join_endpoint() const noexcept;
	void restore_launch_caller() noexcept;
	void require_release() noexcept;

	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets_{};
	NewClassicLanguageAssetProfile language_ =
		NewClassicLanguageAssetProfile::english;
	NewClassicPatchSurface patch_surface_{};
	NewClassicNetplayRoomStatus status_ =
		NewClassicNetplayRoomStatus::running;
	Screen screen_ = Screen::menu;
	std::uint8_t selection_ = 0U;
	std::uint16_t host_port_ = 31'415U;
	std::string port_entry_{};
	std::string join_endpoint_{};
	NewClassicNetplayRoomTransport transport_ =
		NewClassicNetplayRoomTransport::online_service;
	bool relay_only_ = false;
	std::uint8_t input_delay_frames_ = 0U;
	NewClassicNetplayRoomLaunchMode pending_mode_ =
		NewClassicNetplayRoomLaunchMode::host;
	std::uint8_t character_ = 1U;
	std::uint8_t palette_ = 1U;
	std::uint8_t rank_ = 1U;
	std::optional<NewClassicNetplayRoomLaunch> launch_{};
	bool release_required_ = true;
	bool endpoint_invalid_ = false;
	bool profile_failure_ = false;
	bool valid_ = false;
};

enum class NewClassicNetplayLobbyViewRole : std::uint8_t {
	matchmaking,
	host,
	join,
};

// Static source-styled presentation for the socket-owned handshake. This
// owner has no input or network behavior; the platform presents it before
// binding and redraws it only after the reliable lock exchange reaches ready.
class NewClassicNetplayLobbyView final {
public:
	NewClassicNetplayLobbyView(
		std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
		NewClassicLanguageAssetProfile language,
		NewClassicNetplayLobbyViewRole role,
		NewClassicNetplayCharacterLock local_lock,
		std::uint8_t rank,
		std::optional<std::uint16_t> direct_host_port = std::nullopt
	);

	[[nodiscard]] bool valid() const noexcept;
	[[nodiscard]] bool mark_ready(
		const NewClassicNetplayMatchContract &contract
	) noexcept;
	[[nodiscard]] bool mark_room_code(std::string room_code) noexcept;
	[[nodiscard]] present::SourceSurface surface() const noexcept;

private:
	[[nodiscard]] bool draw() noexcept;

	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets_{};
	NewClassicLanguageAssetProfile language_ =
		NewClassicLanguageAssetProfile::english;
	NewClassicNetplayLobbyViewRole role_ =
		NewClassicNetplayLobbyViewRole::host;
	NewClassicNetplayCharacterLock local_lock_{};
	std::uint8_t rank_ = 1U;
	std::optional<std::uint16_t> direct_host_port_{};
	std::optional<NewClassicNetplayMatchContract> ready_contract_{};
	std::string room_code_{};
	NewClassicPatchSurface patch_surface_{};
	bool valid_ = false;
};

} // namespace th03::app

#endif

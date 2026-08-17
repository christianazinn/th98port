#ifndef TH03_APP_NEW_CLASSIC_NETPLAY_ROOM_HPP
#define TH03_APP_NEW_CLASSIC_NETPLAY_ROOM_HPP

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "th03/app/new_classic_assets.hpp"
#include "th03/app/new_classic_frontend.hpp"
#include "th03/app/new_classic_netplay_match_contract.hpp"

namespace th03::app {

enum class NewClassicNetplayRoomLaunchMode : std::uint8_t {
	host,
	join,
};

struct NewClassicNetplayRoomLaunch final {
	NewClassicNetplayRoomLaunchMode mode =
		NewClassicNetplayRoomLaunchMode::host;
	std::uint16_t host_port = 31'415U;
	std::string join_endpoint{};
	std::uint8_t character = 1U;
	std::uint8_t palette = 1U;
	std::uint8_t rank = 1U;
};

struct NewClassicNetplayRoomInput final {
	NewClassicUiInput ui{};
	std::optional<char> text_character{};
	bool erase = false;
};

enum class NewClassicNetplayRoomStatus : std::uint8_t {
	running,
	back,
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
		std::uint8_t initial_rank = 1U
	);

	[[nodiscard]] bool valid() const noexcept;
	[[nodiscard]] NewClassicNetplayRoomResult consume(
		NewClassicNetplayRoomInput input
	) noexcept;
	[[nodiscard]] present::SourceSurface surface() const noexcept;
	[[nodiscard]] NewClassicNetplayRoomStatus status() const noexcept;
	[[nodiscard]] std::optional<NewClassicNetplayRoomLaunch>
		take_launch() noexcept;

private:
	enum class Screen : std::uint8_t {
		menu,
		player_setup,
		host_confirmation,
		join_entry,
		settings,
	};

	[[nodiscard]] bool draw() noexcept;
	[[nodiscard]] bool valid_join_endpoint() const noexcept;
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
	std::string join_endpoint_{};
	NewClassicNetplayRoomLaunchMode pending_mode_ =
		NewClassicNetplayRoomLaunchMode::host;
	std::uint8_t character_ = 1U;
	std::uint8_t palette_ = 1U;
	std::uint8_t rank_ = 1U;
	std::optional<NewClassicNetplayRoomLaunch> launch_{};
	bool release_required_ = true;
	bool endpoint_invalid_ = false;
	bool valid_ = false;
};

enum class NewClassicNetplayLobbyViewRole : std::uint8_t {
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
		std::uint8_t rank
	);

	[[nodiscard]] bool valid() const noexcept;
	[[nodiscard]] bool mark_ready(
		const NewClassicNetplayMatchContract &contract
	) noexcept;
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
	std::optional<NewClassicNetplayMatchContract> ready_contract_{};
	NewClassicPatchSurface patch_surface_{};
	bool valid_ = false;
};

} // namespace th03::app

#endif

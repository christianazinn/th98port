#ifndef TH03_APP_NEW_CLASSIC_ONLINE_PROFILE_SCENE_HPP
#define TH03_APP_NEW_CLASSIC_ONLINE_PROFILE_SCENE_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "th03/app/new_classic/content/new_classic_assets.hpp"
#include "th03/app/new_classic/new_classic_frontend.hpp"
#include "th03/app/new_classic/online/new_classic_online_client.hpp"

namespace th03::app {

enum class NewClassicOnlineProfileActionType : std::uint8_t {
	update_nametag,
	create_link_code,
	redeem_link_code,
	back,
	application_quit,
};

struct NewClassicOnlineProfileAction final {
	NewClassicOnlineProfileActionType type =
		NewClassicOnlineProfileActionType::back;
	std::string text{};
};

struct NewClassicOnlineProfileInput final {
	NewClassicUiInput ui{};
	std::optional<char> text_character{};
	std::string text_utf8{};
	bool erase = false;
	bool delete_forward = false;
	bool home = false;
	bool end = false;
};

// Retained source-styled account editor. HTTPS and credential storage remain
// outside this deterministic presentation owner and are acknowledged through
// the report_* methods.
class NewClassicOnlineProfileScene final {
public:
	NewClassicOnlineProfileScene(
		std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
		NewClassicLanguageAssetProfile language,
		std::optional<std::string> nametag
	);

	[[nodiscard]] bool valid() const noexcept;
	[[nodiscard]] bool text_entry_active() const noexcept;
	[[nodiscard]] present::SourceSurface surface() const noexcept;
	[[nodiscard]] bool consume(NewClassicOnlineProfileInput input) noexcept;
	[[nodiscard]] std::optional<NewClassicOnlineProfileAction>
		take_action() noexcept;
	[[nodiscard]] bool report_profile(NewClassicOnlineProfile profile) noexcept;
	[[nodiscard]] bool report_link_code(std::string code) noexcept;
	[[nodiscard]] bool report_linked(NewClassicOnlineProfile profile) noexcept;
	[[nodiscard]] bool report_failure() noexcept;

private:
	enum class Phase : std::uint8_t {
		fade_in,
		menu,
		fade_out,
	};

	enum class Screen : std::uint8_t {
		menu,
		name_entry,
		link_entry,
		link_code,
		linked,
		failed,
	};

	[[nodiscard]] bool draw() noexcept;
	[[nodiscard]] bool consume_fade() noexcept;
	void begin_name_entry() noexcept;
	[[nodiscard]] std::string name_entry_text() const;
	void reset_cursor_blink() noexcept;
	void begin_fade_out(NewClassicOnlineProfileAction action) noexcept;
	void require_release() noexcept;

	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets_{};
	NewClassicLanguageAssetProfile language_ =
		NewClassicLanguageAssetProfile::english;
	NewClassicPatchSurface patch_surface_{};
	Phase phase_ = Phase::fade_in;
	std::uint8_t fade_boundary_ = 0U;
	NewClassicOnlineProfileAction fade_out_action_{};
	Screen screen_ = Screen::menu;
	std::uint8_t selection_ = 0U;
	std::optional<std::string> nametag_{};
	std::array<std::string, 2U> name_entry_lines_{};
	std::size_t name_entry_row_ = 0U;
	std::size_t name_entry_column_ = 0U;
	std::string entry_{};
	std::size_t entry_cursor_ = 0U;
	std::string link_code_{};
	std::optional<NewClassicOnlineProfileAction> action_{};
	bool release_required_ = true;
	bool request_pending_ = false;
	bool invalid_entry_ = false;
	std::uint8_t cursor_blink_ticks_ = 0U;
	bool cursor_visible_ = true;
	bool valid_ = false;
};

} // namespace th03::app

#endif

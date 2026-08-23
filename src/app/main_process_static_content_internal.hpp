#ifndef TH03_APP_MAIN_PROCESS_STATIC_CONTENT_INTERNAL_HPP
#define TH03_APP_MAIN_PROCESS_STATIC_CONTENT_INTERNAL_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "th03/app/authentic_text.hpp"
#include "th03/app/main_process.hpp"
#include "th03/core/game.hpp"
#include "th03/present/staff_flakes.hpp"
#include "th03/present/superimpose.hpp"

namespace th03::app {

class NewClassicGameTextCatalog;

class MainProcessStaticContent final {
public:
	MainProcessStaticContent(const MainProcessStaticContent &) = delete;
	MainProcessStaticContent &operator=(const MainProcessStaticContent &) = delete;
	MainProcessStaticContent(MainProcessStaticContent &&) = delete;
	MainProcessStaticContent &operator=(MainProcessStaticContent &&) = delete;

	[[nodiscard]] const MainCompiledContent &compiled() const noexcept;
	[[nodiscard]] const Pc98FontRomProvider &font() const noexcept;
	[[nodiscard]] const PreparedGameftGaiji &gameft_gaiji() const noexcept;
	[[nodiscard]] const GameContent &game_content() const noexcept;
	[[nodiscard]] const assets::BfntSheet &zikicw_sheet() const noexcept;
	[[nodiscard]] const present::PreparedSuperimposeSheet &lose() const noexcept;
	[[nodiscard]] const present::PreparedSuperimposeSheet &round() const noexcept;
	[[nodiscard]] const present::PreparedSuperimposeSheet &zikicw() const noexcept;
	[[nodiscard]] present::StaffFlakeTrigTables trig() const noexcept;

private:
	friend struct MainProcessStaticContentAccess;
	explicit MainProcessStaticContent(
		MainCompiledContent compiled,
		std::span<const std::byte> font_rom
	);

	MainCompiledContent compiled_;
	std::vector<std::byte> font_rom_{};
	std::optional<Pc98FontRomProvider> font_provider_{};
	std::optional<PreparedGameftGaiji> gameft_gaiji_{};
	GameContent game_content_{};
	assets::BfntSheet zikicw_sheet_{};
	std::optional<present::PreparedSuperimposeSheet> prepared_lose_{};
	std::optional<present::PreparedSuperimposeSheet> prepared_round_{};
	std::optional<present::PreparedSuperimposeSheet> prepared_zikicw_{};
	std::array<std::int16_t, 256U> sine_q8_{};
	std::array<std::int16_t, 256U> cosine_q8_{};
};

struct MainProcessStaticContentAccess final {
	[[nodiscard]] static std::expected<
		std::shared_ptr<const MainProcessStaticContent>,
		MainProcessError
	> create(
		std::span<const LoadedMainResource> resources,
		MainCompiledContent compiled,
		std::span<const std::byte> font_rom,
		bool authenticate_resource_bytes = true,
		const NewClassicGameTextCatalog *localized_hud_text = nullptr
	);
};

} // namespace th03::app

#endif

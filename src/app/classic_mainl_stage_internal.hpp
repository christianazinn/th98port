#ifndef TH03_APP_CLASSIC_MAINL_STAGE_INTERNAL_HPP
#define TH03_APP_CLASSIC_MAINL_STAGE_INTERNAL_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "th03/app/authentic_text.hpp"
#include "main_stage_handoff_internal.hpp"
#include "th03/app/main_process.hpp"
#include "th03/app/resources.hpp"
#include "th03/assets/types.hpp"
#include "th03/present/graphics_text.hpp"
#include "th03/present/text.hpp"
#include "th03/present/tram.hpp"

namespace th03::app {

struct MainlStageCharacterText final {
	std::vector<std::uint8_t> title{};
	std::vector<std::uint8_t> name{};
};

// Owning typed seam used until the authenticated stock DIET-packed MAINL.EXE
// importer lands. No original text bytes are compiled into the port.
struct MainlStageTextAssets final {
	std::array<MainlStageCharacterText, 9U> characters{};
};

enum class ClassicMainlStagePhase : std::uint8_t {
	black_in,
	waiting,
	white_out,
	white_in,
	ready,
	consumed,
	failed,
};

enum class ClassicMainlStageErrorCode : std::uint8_t {
	invalid_config,
	resource_set_mismatch,
	resource_bytes_missing,
	resource_integrity_failed,
	asset_decode_failed,
	font_prepare_failed,
	invalid_asset_geometry,
	palette_missing,
	draw_failed,
	invalid_state,
};

struct ClassicMainlStageError final {
	ClassicMainlStageErrorCode code =
		ClassicMainlStageErrorCode::invalid_config;
	std::optional<MainResourceKey> resource{};
	std::optional<assets::AssetError> asset_error{};
	std::optional<present::GraphicsTextError> text_error{};
};

struct ClassicMainlStageTick final {
	ClassicMainlStagePhase phase = ClassicMainlStagePhase::failed;
	std::uint8_t wait_count = 0U;
	bool input_accepted = false;
	bool handoff_ready = false;
};

// Fixed-storage source-ordered Story stage splash. Construction decodes and
// prepares the complete authenticated closure before publishing any Raster.
// advance() resumes from one source VSync boundary to the next (or to the
// final MAIN handoff boundary).
class ClassicMainlStageTransition final {
public:
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicMainlStageTransition>,
		ClassicMainlStageError
	> create(
		const MainStoryConfig &config,
		std::span<const LoadedMainResource> resources,
		MainlStageTextAssets text,
		const present::AnkGlyphProvider &font
	);
	~ClassicMainlStageTransition();

	ClassicMainlStageTransition(const ClassicMainlStageTransition &) = delete;
	ClassicMainlStageTransition &operator=(
		const ClassicMainlStageTransition &
	) = delete;
	ClassicMainlStageTransition(ClassicMainlStageTransition &&) = delete;
	ClassicMainlStageTransition &operator=(
		ClassicMainlStageTransition &&
	) = delete;

	[[nodiscard]] std::expected<
		ClassicMainlStageTick,
		ClassicMainlStageError
	> advance(bool dismiss_pressed) noexcept;

	[[nodiscard]] ClassicMainlStagePhase phase() const noexcept;
	[[nodiscard]] std::uint8_t wait_count() const noexcept;
	[[nodiscard]] const present::Raster &raster() const noexcept;
	[[nodiscard]] const present::Tram &tram() const noexcept;

private:
	friend struct ClassicMainlStageTestAccess;
	friend struct MainProcessStoryAccess;

	struct Prepared final {
		MainStoryConfig config{};
		std::array<assets::PlanarCel4, 2U> portraits{};
		std::optional<assets::PlanarCel4> stage_number{};
		assets::PiImage splash_base{};
		assets::PiImage splash_background{};
		std::array<assets::PiImage, 2U> player_shots{};
		std::array<assets::PiImage, 2U> player_extra_shots{};
		assets::PiImage enemy_center{};
		assets::PiImage opponent_enemies{};
		std::array<
			std::optional<present::PreparedGraphicsText8x16>,
			4U
		> text{};
		std::optional<PreparedGameftGaiji> mainl_gaiji{};
	};
	explicit ClassicMainlStageTransition(
		std::unique_ptr<Prepared> prepared
	) noexcept;

	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicMainlStageTransition>,
		ClassicMainlStageError
	> create_impl(
		const MainStoryConfig &config,
		std::span<const LoadedMainResource> resources,
		MainlStageTextAssets text,
		const present::AnkGlyphProvider &font,
		bool authenticate_resource_bytes
	);
	[[nodiscard]] bool stage_hidden_sheet() noexcept;
	[[nodiscard]] bool finalize_handoff() noexcept;
	[[nodiscard]] const MainStageHandoff *handoff() const noexcept;
	[[nodiscard]] bool matches_process_config(
		const MainStoryConfig &config
	) const noexcept;
	void consume_handoff() noexcept;
	void fail() noexcept;

	std::unique_ptr<Prepared> prepared_;
	std::uint8_t handoff_game_mode_ = 0U;
	std::uint8_t handoff_story_stage_ = 0U;
	std::array<std::int8_t, 2U> handoff_paletted_character_{};
	present::Raster raster_{};
	present::Tram tram_{};
	std::optional<MainStageHandoff> handoff_{};
	ClassicMainlStagePhase phase_ = ClassicMainlStagePhase::black_in;
	std::uint8_t fade_index_ = 0U;
	std::uint8_t wait_count_ = 0U;
};

struct ClassicMainlStageTestAccess final {
	[[nodiscard]] static std::unique_ptr<ClassicMainlStageTransition>
	create_ready_with_handoff(
		const MainStoryConfig &config,
		present::Raster raster
	);

	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicMainlStageTransition>,
		ClassicMainlStageError
	> create_with_synthetic_resources(
		const MainStoryConfig &config,
		std::span<const LoadedMainResource> resources,
		MainlStageTextAssets text,
		const present::AnkGlyphProvider &font
	)
	{
		return ClassicMainlStageTransition::create_impl(
			config,
			resources,
			std::move(text),
			font,
			false
		);
	}

	[[nodiscard]] static const MainStageHandoff *handoff(
		const ClassicMainlStageTransition &transition
	) noexcept
	{
		return transition.handoff();
	}
};

} // namespace th03::app

#endif

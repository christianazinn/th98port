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
#include "th03/app/classic_audio.hpp"
#include "main_stage_handoff_internal.hpp"
#include "th03/app/main_process.hpp"
#include "th03/formats/replay.hpp"
#include "th03/app/resources.hpp"
#include "th03/assets/types.hpp"
#include "th03/present/graphics_text.hpp"
#include "th03/present/text.hpp"
#include "th03/present/tram.hpp"

namespace th03::app {

class ClassicOpSelectionCompletion;
class ClassicOpCompletion;

// Source-private identity shared by MAINL's Load and MAIN's Play command.
// The returned command always has type load_song.
[[nodiscard]] std::optional<ClassicAudioCommand>
classic_main_gameplay_song(const GameInit &resident) noexcept;

struct MainlStageCharacterText final {
	std::vector<std::uint8_t> title{};
	std::vector<std::uint8_t> title_second{};
	std::vector<std::uint8_t> name{};
};

// Owning typed seam produced by the authenticated MAINL profile importer or
// supplied directly by source-private hermetic tests. It includes text shared
// with MAINL registration because both consumers receive the same immutable
// executable-profile asset. No original text bytes are compiled into the port.
struct MainlStageTextAssets final {
	std::array<MainlStageCharacterText, 9U> characters{};
	std::array<std::vector<std::uint8_t>, 10U> registration_playchars{};
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

// Fixed-storage source-ordered MAINL stage splash for Story, selected VS, and
// automatic Demo. Construction decodes and prepares the complete authenticated
// closure before publishing any Raster. advance() resumes from one source
// VSync boundary to the next (or to the final MAIN handoff boundary).
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
	// Stock OP selection always enters MAINL before MAIN. The private
	// completion proves the non-Story resident instead of accepting a second,
	// caller-forgeable GameInit.
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicMainlStageTransition>,
		ClassicMainlStageError
	> create_from_selection(
		const ClassicOpSelectionCompletion &completion,
		present::KotohimeBombFadeCompatibility kotohime_compatibility,
		std::span<const LoadedMainResource> resources,
		MainlStageTextAssets text,
		const present::AnkGlyphProvider &font,
		GameOptions game_options = {}
	);
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicMainlStageTransition>,
		ClassicMainlStageError
	> create_from_restart(
		GameInit resident,
		present::KotohimeBombFadeCompatibility kotohime_compatibility,
		std::span<const LoadedMainResource> resources,
		MainlStageTextAssets text,
		const present::AnkGlyphProvider &font,
		GameOptions game_options,
		std::optional<formats::ReplayPracticeConfig> practice = std::nullopt
	);
	// Automatic Demo follows OP -> MAINL -> MAIN without a character selector.
	// The private OP completion proves the exact built-in pairing/RNG recipe.
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicMainlStageTransition>,
		ClassicMainlStageError
	> create_from_demo(
		const ClassicOpCompletion &completion,
		present::KotohimeBombFadeCompatibility kotohime_compatibility,
		std::span<const LoadedMainResource> resources,
		MainlStageTextAssets text,
		const present::AnkGlyphProvider &font,
		GameOptions game_options = {}
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
	[[nodiscard]] std::optional<ClassicAudioCommand>
	take_audio_command() noexcept;

private:
	friend class ClassicStorySession;
	friend struct ClassicStoryBackend;
	friend struct ClassicMainlStageTestAccess;
	friend struct MainProcessStoryAccess;
	friend struct MainProcessClassicVsAccess;
	friend struct MainProcessDemoAccess;

	enum class EntryPolicy : std::uint8_t {
		story,
		classic_vs,
		demo,
	};

	struct EntryConfig final {
		GameInit resident{};
		GameOptions game_options{};
		present::KotohimeBombFadeCompatibility kotohime_compatibility{};
		EntryPolicy policy = EntryPolicy::story;
		std::optional<formats::ReplayPracticeConfig> practice{};
	};

	struct Prepared final {
		EntryConfig entry{};
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
			6U
		> text{};
		bool english_text = false;
		std::optional<PreparedGameftGaiji> mainl_gaiji{};
	};
	explicit ClassicMainlStageTransition(
		std::unique_ptr<Prepared> prepared
	) noexcept;

	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicMainlStageTransition>,
		ClassicMainlStageError
	> create_impl(
		EntryConfig entry,
		std::span<const LoadedMainResource> resources,
		MainlStageTextAssets text,
		const present::AnkGlyphProvider &font,
		bool authenticate_resource_bytes
	);
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicMainlStageTransition>,
		ClassicMainlStageError
	> create_from_selection_impl(
		const ClassicOpSelectionCompletion &completion,
		present::KotohimeBombFadeCompatibility kotohime_compatibility,
		std::span<const LoadedMainResource> resources,
		MainlStageTextAssets text,
		const present::AnkGlyphProvider &font,
		GameOptions game_options,
		bool authenticate_resource_bytes
	);
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicMainlStageTransition>,
		ClassicMainlStageError
	> create_from_demo_impl(
		const ClassicOpCompletion &completion,
		present::KotohimeBombFadeCompatibility kotohime_compatibility,
		std::span<const LoadedMainResource> resources,
		MainlStageTextAssets text,
		const present::AnkGlyphProvider &font,
		GameOptions game_options,
		bool authenticate_resource_bytes
	);
	[[nodiscard]] bool stage_hidden_sheet() noexcept;
	[[nodiscard]] bool begin_splash_show() noexcept;
	[[nodiscard]] bool preload_splash_into(
		present::Raster &target
	) const noexcept;
	[[nodiscard]] bool adopt_preloaded_raster(
		present::Raster raster
	) noexcept;
	[[nodiscard]] bool finalize_handoff() noexcept;
	[[nodiscard]] bool push_audio_command(
		ClassicAudioCommand command
	) noexcept;
	[[nodiscard]] const MainStageHandoff *handoff() const noexcept;
	[[nodiscard]] bool matches_process_config(
		const MainStoryConfig &config
	) const noexcept;
	[[nodiscard]] bool is_classic_vs_entry() const noexcept;
	[[nodiscard]] bool is_demo_entry() const noexcept;
	void consume_handoff() noexcept;
	void fail() noexcept;

	std::unique_ptr<Prepared> prepared_;
	std::uint8_t handoff_game_mode_ = 0U;
	std::uint8_t handoff_story_stage_ = 0U;
	std::array<std::int8_t, 2U> handoff_paletted_character_{};
	present::Raster raster_{};
	present::Tram tram_{};
	std::optional<MainStageHandoff> handoff_{};
	std::array<ClassicAudioCommand, 3U> audio_commands_{};
	std::size_t audio_head_ = 0U;
	std::size_t audio_count_ = 0U;
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
	[[nodiscard]] static std::unique_ptr<ClassicMainlStageTransition>
	create_ready_vs_with_handoff(
		GameInit resident,
		present::KotohimeBombFadeCompatibility kotohime_compatibility,
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
			{
				.resident = config.resident,
				.game_options = config.game_options,
				.kotohime_compatibility = config.kotohime_compatibility,
				.policy = ClassicMainlStageTransition::EntryPolicy::story,
			},
			resources,
			std::move(text),
			font,
			false
		);
	}

	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicMainlStageTransition>,
		ClassicMainlStageError
	> create_vs_with_synthetic_resources(
		const ClassicOpSelectionCompletion &completion,
		present::KotohimeBombFadeCompatibility kotohime_compatibility,
		std::span<const LoadedMainResource> resources,
		MainlStageTextAssets text,
		const present::AnkGlyphProvider &font
	)
	{
		return ClassicMainlStageTransition::create_from_selection_impl(
			completion,
			kotohime_compatibility,
			resources,
			std::move(text),
			font,
			{},
			false
		);
	}

	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicMainlStageTransition>,
		ClassicMainlStageError
	> create_demo_with_synthetic_resources(
		const ClassicOpCompletion &completion,
		present::KotohimeBombFadeCompatibility kotohime_compatibility,
		std::span<const LoadedMainResource> resources,
		MainlStageTextAssets text,
		const present::AnkGlyphProvider &font,
		GameOptions game_options = {}
	)
	{
		return ClassicMainlStageTransition::create_from_demo_impl(
			completion,
			kotohime_compatibility,
			resources,
			std::move(text),
			font,
			game_options,
			false
		);
	}

	[[nodiscard]] static GameOptions entry_game_options(
		const ClassicMainlStageTransition &transition
	) noexcept
	{
		return transition.prepared_ ?
			transition.prepared_->entry.game_options : GameOptions{};
	}

	[[nodiscard]] static const MainStageHandoff *handoff(
		const ClassicMainlStageTransition &transition
	) noexcept
	{
		return transition.handoff();
	}

	[[nodiscard]] static bool preload_splash_into(
		const ClassicMainlStageTransition &transition,
		present::Raster &target
	) noexcept
	{
		return transition.preload_splash_into(target);
	}

	[[nodiscard]] static bool adopt_preloaded_raster(
		ClassicMainlStageTransition &transition,
		present::Raster raster
	) noexcept
	{
		return transition.adopt_preloaded_raster(std::move(raster));
	}
};

} // namespace th03::app

#endif

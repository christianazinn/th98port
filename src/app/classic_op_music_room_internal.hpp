#ifndef TH03_APP_CLASSIC_OP_MUSIC_ROOM_INTERNAL_HPP
#define TH03_APP_CLASSIC_OP_MUSIC_ROOM_INTERNAL_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>

#include "classic_op_scene_internal.hpp"
#include "th03/app/new_classic_game_text.hpp"
#include "op_compiled_text_internal.hpp"
#include "th03/app/classic_audio.hpp"
#include "th03/assets/types.hpp"
#include "th03/core/game.hpp"
#include "th03/present/graphics_text.hpp"
#include "th03/present/raster.hpp"
#include "th03/present/tram.hpp"

namespace th03::app {

inline constexpr std::size_t classic_music_room_track_count = 19U;
inline constexpr std::size_t classic_music_room_choice_count = 21U;
inline constexpr std::size_t classic_music_room_polygon_count = 16U;
inline constexpr std::size_t classic_music_room_comment_lines = 20U;
inline constexpr std::size_t classic_music_room_comment_text_bytes = 40U;
inline constexpr std::size_t classic_music_room_comment_line_bytes = 42U;
inline constexpr std::size_t classic_music_room_comment_file_bytes =
	classic_music_room_track_count * classic_music_room_comment_lines *
	classic_music_room_comment_line_bytes;

struct ClassicOpMusicPolygon final {
	std::int16_t center_x = 0;
	std::int16_t center_y_q12_4 = 0;
	std::int16_t velocity_x = 0;
	std::int16_t velocity_y_q12_4 = 0;
	std::uint8_t angle = 0U;
	std::uint8_t angle_speed = 0U;

	friend constexpr bool operator==(
		const ClassicOpMusicPolygon &,
		const ClassicOpMusicPolygon &
	) = default;
};

// OP.EXE process-local Music Room state. The caller owns and reuses this
// carry across title returns and applies character-selection seed resets.
struct ClassicOpMusicRoomCarry final {
	std::array<ClassicOpMusicPolygon, classic_music_room_polygon_count>
		polygons{};
	std::uint32_t random_seed = 1U;
	std::uint8_t track_playing = 0U;
	bool polygons_initialized = false;

	friend constexpr bool operator==(
		const ClassicOpMusicRoomCarry &,
		const ClassicOpMusicRoomCarry &
	) = default;
};

struct ClassicOpMusicRoomInput final {
	ClassicOpInput controls{};
};

enum class ClassicOpMusicRoomPhase : std::uint8_t {
	held_release_poll,
	controls_poll,
	selected_track_frame,
	exit_release_poll,
	ready,
	consumed,
	failed,
};

enum class ClassicOpMusicRoomBoundary : std::uint8_t {
	immediate,
	vsync,
	input_poll,
};

enum class ClassicOpMusicRoomErrorCode : std::uint8_t {
	invalid_route,
	resource_set_mismatch,
	resource_bytes_missing,
	resource_integrity_failed,
	asset_decode_failed,
	compiled_text_import_failed,
	invalid_context,
	invalid_background,
	invalid_compiled_text,
	malformed_comment_file,
	font_prepare_failed,
	draw_failed,
	audio_command_pending,
	invalid_state,
};

struct ClassicOpMusicRoomError final {
	ClassicOpMusicRoomErrorCode code =
		ClassicOpMusicRoomErrorCode::invalid_state;
	std::uint8_t track = 0U;
	std::uint8_t line = 0U;
	std::optional<MainResourceKey> resource{};
	std::optional<assets::AssetError> asset_error{};
	std::optional<OpCompiledTextError> compiled_text_error{};
	std::optional<present::GraphicsTextError> text_error{};
};

struct ClassicOpMusicRoomTick final {
	ClassicOpMusicRoomPhase phase = ClassicOpMusicRoomPhase::failed;
	ClassicOpMusicRoomBoundary boundary =
		ClassicOpMusicRoomBoundary::immediate;
	bool surface_changed = false;
	bool input_sampled = false;
	bool page_flipped = false;
	bool selection_changed = false;
	bool carry_changed = false;
	bool ready = false;
};

class ClassicOpMusicRoomCompletion final {
public:
	ClassicOpMusicRoomCompletion(const ClassicOpMusicRoomCompletion &) = delete;
	ClassicOpMusicRoomCompletion &operator=(
		const ClassicOpMusicRoomCompletion &
	) = delete;
	ClassicOpMusicRoomCompletion(ClassicOpMusicRoomCompletion &&) noexcept =
		default;
	ClassicOpMusicRoomCompletion &operator=(
		ClassicOpMusicRoomCompletion &&
	) noexcept = default;

	[[nodiscard]] const GameInit &resident() const noexcept;
	[[nodiscard]] const ClassicOpMusicRoomCarry &carry() const noexcept;
	[[nodiscard]] const present::Raster &raster() const noexcept;
	[[nodiscard]] const present::Tram &tram() const noexcept;

private:
	friend class ClassicOpMusicRoomScene;
	ClassicOpMusicRoomCompletion(
		GameInit resident,
		ClassicOpMusicRoomCarry carry,
		present::Raster raster,
		present::Tram tram
	) noexcept;

	GameInit resident_{};
	ClassicOpMusicRoomCarry carry_{};
	present::Raster raster_{};
	present::Tram tram_{};
};

// Source-private owner of TH03's blocking Music Room. Construction prepares
// every executable label and MUSIC.TXT line; advance() consumes at most one
// exact input poll or VSync boundary and never allocates.
class ClassicOpMusicRoomScene final {
public:
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicOpMusicRoomScene>,
		ClassicOpMusicRoomError
	> create_from_op(
		const ClassicOpCompletion &completion,
		ClassicOpMusicRoomCarry carry,
		std::span<const LoadedMainResource> resources,
		std::span<const std::byte> op_executable,
		const present::AnkGlyphProvider &font,
		const NewClassicGameTextCatalog *localized_text = nullptr
	);
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicOpMusicRoomScene>,
		ClassicOpMusicRoomError
	> create_from_imported_text(
		const ClassicOpCompletion &completion,
		ClassicOpMusicRoomCarry carry,
		std::span<const LoadedMainResource> resources,
		const OpMusicRoomTextImport &labels,
		const present::AnkGlyphProvider &font,
		const NewClassicGameTextCatalog *localized_text = nullptr
	);

	~ClassicOpMusicRoomScene();
	ClassicOpMusicRoomScene(const ClassicOpMusicRoomScene &) = delete;
	ClassicOpMusicRoomScene &operator=(
		const ClassicOpMusicRoomScene &
	) = delete;
	ClassicOpMusicRoomScene(ClassicOpMusicRoomScene &&) = delete;
	ClassicOpMusicRoomScene &operator=(ClassicOpMusicRoomScene &&) = delete;

	[[nodiscard]] std::expected<
		ClassicOpMusicRoomTick,
		ClassicOpMusicRoomError
	> advance(const ClassicOpMusicRoomInput &input = {});
	[[nodiscard]] std::optional<ClassicAudioCommand>
	take_audio_command() noexcept;
	[[nodiscard]] std::optional<ClassicOpMusicRoomCompletion>
	take_completion() noexcept;

	[[nodiscard]] ClassicOpMusicRoomPhase phase() const noexcept;
	[[nodiscard]] std::uint8_t selection() const noexcept;
	[[nodiscard]] const ClassicOpMusicRoomCarry &carry() const noexcept;
	[[nodiscard]] const present::Raster &raster() const noexcept;
	[[nodiscard]] const present::Tram &tram() const noexcept;

private:
	friend struct ClassicOpMusicRoomSceneTestAccess;

	struct Prepared final {
		assets::PiImage background{};
		std::array<
			std::optional<present::PreparedGraphicsText8x16>,
			classic_music_room_choice_count
		> choices{};
		std::array<
			std::array<
				std::optional<present::PreparedGraphicsText8x16>,
				classic_music_room_comment_lines
			>,
			classic_music_room_track_count
		> comments{};
	};

	ClassicOpMusicRoomScene(
		std::unique_ptr<Prepared> prepared,
		GameInit resident,
		ClassicOpMusicRoomCarry carry,
		present::Raster raster,
		present::Tram tram
	) noexcept;

	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicOpMusicRoomScene>,
		ClassicOpMusicRoomError
	> create_predecoded(
		GameInit resident,
		ClassicOpMusicRoomCarry carry,
		assets::PiImage background,
		const OpMusicRoomTextImport &labels,
		std::span<const std::byte> comments,
		const present::AnkGlyphProvider &font,
		present::Raster raster,
		present::Tram tram,
		const NewClassicGameTextCatalog *localized_text
	);

	[[nodiscard]] bool initialize_visuals() noexcept;
	[[nodiscard]] bool draw_track(
		std::uint8_t choice,
		present::GraphicsTextStyle style
	) noexcept;
	[[nodiscard]] bool draw_track_both(
		std::uint8_t choice,
		present::GraphicsTextStyle style
	) noexcept;
	[[nodiscard]] bool draw_tracklist() noexcept;
	[[nodiscard]] bool restore_comment_background() noexcept;
	[[nodiscard]] bool put_comment_text(std::uint8_t track) noexcept;
	[[nodiscard]] bool draw_comment(std::uint8_t track) noexcept;
	[[nodiscard]] bool snapshot_blue_plane() noexcept;
	[[nodiscard]] bool snapshot_comment_blue() noexcept;
	[[nodiscard]] bool restore_blue_plane() noexcept;
	void initialize_polygon(ClassicOpMusicPolygon &polygon) noexcept;
	[[nodiscard]] bool render_polygons() noexcept;
	[[nodiscard]] bool flip_page() noexcept;
	[[nodiscard]] bool render_and_flip() noexcept;
	[[nodiscard]] bool finish() noexcept;
	[[nodiscard]] bool push_audio_command(ClassicAudioCommand command) noexcept;
	[[nodiscard]] ClassicOpMusicRoomTick tick(
		ClassicOpMusicRoomBoundary boundary,
		bool surface_changed = false,
		bool input_sampled = false,
		bool page_flipped = false,
		bool selection_changed = false,
		bool carry_changed = false
	) const noexcept;
	void fail() noexcept;

	std::unique_ptr<Prepared> prepared_{};
	GameInit resident_{};
	ClassicOpMusicRoomCarry carry_{};
	present::Raster raster_{};
	present::Tram tram_{};
	std::array<std::uint8_t, 32000U> no_polygon_blue_{};
	std::array<ClassicAudioCommand, 3U> audio_commands_{};
	std::size_t audio_head_ = 0U;
	std::size_t audio_count_ = 0U;
	ClassicOpMusicRoomPhase phase_ = ClassicOpMusicRoomPhase::failed;
	std::uint8_t selection_ = 0U;
	bool exit_after_selected_frame_ = false;
	std::optional<ClassicOpMusicRoomCompletion> completion_{};
	present::Page music_page_accessed_ = present::Page::one;
	std::array<std::uint8_t, 2U> selection_on_page_{};
};

struct ClassicOpMusicRoomSceneTestAccess final {
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicOpMusicRoomScene>,
		ClassicOpMusicRoomError
	> create_with_predecoded_assets(
		GameInit resident,
		ClassicOpMusicRoomCarry carry,
		assets::PiImage background,
		const OpMusicRoomTextImport &labels,
		std::span<const std::byte> comments,
		const present::AnkGlyphProvider &font,
		present::Raster raster = {},
		present::Tram tram = {},
		const NewClassicGameTextCatalog *localized_text = nullptr
	);
};

} // namespace th03::app

#endif

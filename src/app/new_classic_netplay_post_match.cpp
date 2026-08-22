#include "th03/app/new_classic_netplay_post_match.hpp"

#include "classic_mainl_win_internal.hpp"
#include "new_classic_netplay_post_match_internal.hpp"

#include <fstream>
#include <utility>
#include <vector>

#include "th03/present/source_surface.hpp"

namespace th03::app {
namespace {

[[nodiscard]] NewClassicNetplayPostMatchError post_match_error(
	NewClassicNetplayPostMatchErrorCode code
) noexcept
{
	return NewClassicNetplayPostMatchError{.code = code};
}

[[nodiscard]] bool valid_request(
	const NewClassicNetplayPostMatchRequest &request
) noexcept
{
	return request.process_start.game_mode == game_mode::vs_1p_2p &&
		request.terminal.game_mode == game_mode::vs_1p_2p &&
		request.terminal.route == 2U && request.terminal.winner >= 0 &&
		request.terminal.winner <= 1 &&
		request.transition.action == SessionAction::exit_to_operator &&
		!request.transition.high_score_registration &&
		request.process_start.paletted_character[0U] >= 1 &&
		request.process_start.paletted_character[0U] <= 18 &&
		request.process_start.paletted_character[1U] >= 1 &&
		request.process_start.paletted_character[1U] <= 18;
}

[[nodiscard]] std::expected<
	std::vector<std::byte>,
	NewClassicNetplayPostMatchError
> read_font_rom(const std::filesystem::path &path)
{
	std::error_code error{};
	const auto status = std::filesystem::status(path, error);
	if(error || !std::filesystem::exists(status)) {
		return std::unexpected(post_match_error(
			NewClassicNetplayPostMatchErrorCode::file_missing
		));
	}
	if(!std::filesystem::is_regular_file(status)) {
		return std::unexpected(post_match_error(
			NewClassicNetplayPostMatchErrorCode::file_not_regular
		));
	}
	const auto size = std::filesystem::file_size(path, error);
	if(error) {
		return std::unexpected(post_match_error(
			NewClassicNetplayPostMatchErrorCode::file_size_failed
		));
	}
	if(size != Pc98FontRomProvider::font_rom_size) {
		return std::unexpected(post_match_error(
			NewClassicNetplayPostMatchErrorCode::file_size_mismatch
		));
	}
	std::vector<std::byte> bytes(Pc98FontRomProvider::font_rom_size);
	std::ifstream input(path, std::ios::binary);
	if(!input.is_open()) {
		return std::unexpected(post_match_error(
			NewClassicNetplayPostMatchErrorCode::file_open_failed
		));
	}
	input.read(
		reinterpret_cast<char *>(bytes.data()),
		static_cast<std::streamsize>(bytes.size())
	);
	if(!input || input.gcount() != static_cast<std::streamsize>(bytes.size()) ||
		input.peek() != std::ifstream::traits_type::eof()) {
		return std::unexpected(post_match_error(
			NewClassicNetplayPostMatchErrorCode::file_read_failed
		));
	}
	return bytes;
}

[[nodiscard]] NewClassicNetplayPostMatchBoundary map_boundary(
	ClassicMainlWinBoundary boundary
) noexcept
{
	switch(boundary) {
	case ClassicMainlWinBoundary::immediate:
	case ClassicMainlWinBoundary::stage_preload:
		return NewClassicNetplayPostMatchBoundary::immediate;
	case ClassicMainlWinBoundary::vsync:
		return NewClassicNetplayPostMatchBoundary::vsync;
	case ClassicMainlWinBoundary::audio_measure:
		return NewClassicNetplayPostMatchBoundary::audio_measure;
	}
	return NewClassicNetplayPostMatchBoundary::immediate;
}

} // namespace

struct NewClassicNetplayPostMatch::Impl final {
	std::vector<std::byte> font_rom{};
	std::optional<Pc98FontRomProvider> font_provider{};
	std::optional<NewClassicGameTextCatalog> game_text{};
	const present::AnkGlyphProvider *font = nullptr;
	std::unique_ptr<ClassicMainlWinScene> scene{};
	std::optional<ClassicMainlWinCompletion> completion{};
	present::Tram tram{};
	NewClassicNetplayPostMatchStatus status =
		NewClassicNetplayPostMatchStatus::running;
};

std::expected<
	std::unique_ptr<NewClassicNetplayPostMatch>,
	NewClassicNetplayPostMatchError
> NewClassicNetplayPostMatch::create(NewClassicNetplayPostMatchRequest request)
{
	if(!valid_request(request) || request.content.data_root.empty() ||
		request.content.font_rom.empty()) {
		return std::unexpected(post_match_error(
			NewClassicNetplayPostMatchErrorCode::invalid_request
		));
	}
	const auto winner = static_cast<std::size_t>(request.terminal.winner);
	const auto selection = main_player_selection_from_paletted(
		request.process_start.paletted_character[winner]
	);
	if(!selection) {
		return std::unexpected(post_match_error(
			NewClassicNetplayPostMatchErrorCode::invalid_request
		));
	}
	MainResourceLoader loader(request.content.data_root);
	std::optional<NewClassicGameTextCatalog> custom_game_text{};
	const bool english = !request.content.language_overlay.empty();
	const NewClassicGameTextCatalog *fallback = english ?
		&new_classic_builtin_english_game_text() :
		&new_classic_builtin_japanese_game_text();
	if(!request.content.game_text_pack.empty()) {
		auto loaded = load_new_classic_game_text_pack(
			request.content.game_text_pack, fallback
		);
		if(!loaded) {
			return std::unexpected(NewClassicNetplayPostMatchError{
				.code = NewClassicNetplayPostMatchErrorCode::game_text_pack_failed,
				.game_text_pack_error = std::move(loaded.error()),
			});
		}
		if((loaded->locale() == "ja-JP") == english) {
			return std::unexpected(NewClassicNetplayPostMatchError{
				.code = NewClassicNetplayPostMatchErrorCode::game_text_pack_failed,
				.game_text_pack_error = NewClassicGameTextPackError{
					.code = NewClassicGameTextPackErrorCode::locale_shape,
				},
			});
		}
		custom_game_text.emplace(std::move(*loaded));
	}
	if(english) {
		auto overlay = load_new_classic_language_overlay(
			request.content.language_overlay
		);
		if(!overlay) {
			return std::unexpected(NewClassicNetplayPostMatchError{
				.code = NewClassicNetplayPostMatchErrorCode::language_overlay_failed,
				.language_overlay_error = std::move(overlay.error()),
			});
		}
		if(!loader.install_authenticated_overrides(
			std::move(*overlay).release_members()
		)) {
			return std::unexpected(post_match_error(
				NewClassicNetplayPostMatchErrorCode::language_overlay_failed
			));
		}
	}
	auto resources = loader.load_mainl_win(*selection);
	if(!resources) {
		return std::unexpected(NewClassicNetplayPostMatchError{
			.code = NewClassicNetplayPostMatchErrorCode::resource_load_failed,
			.resource_error = std::move(resources.error()),
		});
	}
	auto bytes = read_font_rom(request.content.font_rom);
	if(!bytes) {
		return std::unexpected(std::move(bytes.error()));
	}
	auto font = Pc98FontRomProvider::from_exact_font_rom(*bytes);
	if(!font) {
		return std::unexpected(NewClassicNetplayPostMatchError{
			.code = NewClassicNetplayPostMatchErrorCode::font_invalid,
			.font_error = std::move(font.error()),
		});
	}
	return create_prepared(
		std::move(request), *resources, *font, true, std::move(*bytes),
		std::move(custom_game_text), english
	);
}

std::expected<
	std::unique_ptr<NewClassicNetplayPostMatch>,
	NewClassicNetplayPostMatchError
> NewClassicNetplayPostMatch::create_prepared(
	NewClassicNetplayPostMatchRequest request,
	std::span<const LoadedMainResource> resources,
	const present::AnkGlyphProvider &font,
	bool authenticate_resources,
	std::vector<std::byte> owned_font_rom,
	std::optional<NewClassicGameTextCatalog> owned_game_text,
	bool use_builtin_english_text
)
{
	if(!valid_request(request)) {
		return std::unexpected(post_match_error(
			NewClassicNetplayPostMatchErrorCode::invalid_request
		));
	}
	const auto winner = static_cast<std::size_t>(request.terminal.winner);
	const auto loser = 1U - winner;
	const auto context = ClassicMainlWinContext{
		.winner_paletted_character =
			request.process_start.paletted_character[winner],
		.loser_paletted_character =
			request.process_start.paletted_character[loser],
		.message_block = static_cast<std::uint8_t>(
			(static_cast<std::uint8_t>(
				request.process_start.paletted_character[loser]
			) - 1U) / 2U
		),
		.preload_stage_splash = false,
	};
	auto impl = std::make_unique<Impl>();
	impl->game_text = std::move(owned_game_text);
	const auto *localized_text = impl->game_text ? &*impl->game_text :
		(use_builtin_english_text ?
			&new_classic_builtin_english_game_text() :
			&new_classic_builtin_japanese_game_text());
	auto scene = authenticate_resources ?
		ClassicMainlWinScene::create(
			context, resources, font, localized_text
		) :
		ClassicMainlWinTestAccess::create_with_synthetic_resources(
			context, resources, font, localized_text
		);
	if(!scene) {
		return std::unexpected(post_match_error(
			NewClassicNetplayPostMatchErrorCode::scene_failed
		));
	}
	impl->font_rom = std::move(owned_font_rom);
	if(!impl->font_rom.empty()) {
		auto owned_font = Pc98FontRomProvider::from_exact_font_rom(
			impl->font_rom
		);
		if(!owned_font) {
			return std::unexpected(NewClassicNetplayPostMatchError{
				.code = NewClassicNetplayPostMatchErrorCode::font_invalid,
				.font_error = std::move(owned_font.error()),
			});
		}
		impl->font_provider.emplace(std::move(*owned_font));
		impl->font = &*impl->font_provider;
	} else {
		impl->font = &font;
	}
	impl->scene = std::move(*scene);
	impl->tram.fill({
		' ', static_cast<present::TramAttribute>(present::tram_black)
	});
	return std::unique_ptr<NewClassicNetplayPostMatch>(
		new NewClassicNetplayPostMatch(std::move(impl))
	);
}

NewClassicNetplayPostMatch::NewClassicNetplayPostMatch(
	std::unique_ptr<Impl> impl
) noexcept :
	impl_(std::move(impl))
{}

NewClassicNetplayPostMatch::~NewClassicNetplayPostMatch() = default;

std::expected<
	NewClassicNetplayPostMatchTick,
	NewClassicNetplayPostMatchError
> NewClassicNetplayPostMatch::advance(
	ClassicAudioStatus audio,
	bool dismiss_pressed,
	bool app_quit_requested
) noexcept
{
	if(impl_->status != NewClassicNetplayPostMatchStatus::running ||
		!impl_->scene) {
		return std::unexpected(post_match_error(
			NewClassicNetplayPostMatchErrorCode::not_running
		));
	}
	if(app_quit_requested) {
		impl_->status = NewClassicNetplayPostMatchStatus::quit_requested;
		return NewClassicNetplayPostMatchTick{.status = impl_->status};
	}
	if(impl_->scene->phase() == ClassicMainlWinPhase::ready) {
		impl_->completion = impl_->scene->take_completion();
		if(!impl_->completion ||
			impl_->completion->stage_splash_preloaded()) {
			return std::unexpected(post_match_error(
				NewClassicNetplayPostMatchErrorCode::scene_failed
			));
		}
		impl_->status = NewClassicNetplayPostMatchStatus::complete;
		return NewClassicNetplayPostMatchTick{.status = impl_->status};
	}
	auto tick = impl_->scene->advance(audio, dismiss_pressed);
	if(!tick || tick->stage_preload_requested) {
		return std::unexpected(post_match_error(
			NewClassicNetplayPostMatchErrorCode::scene_failed
		));
	}
	return NewClassicNetplayPostMatchTick{
		.status = impl_->status,
		.boundary = map_boundary(tick->boundary),
		.surface_changed = tick->surface_changed,
		.input_sampled = tick->input_sampled,
	};
}

std::optional<ClassicAudioCommand>
NewClassicNetplayPostMatch::take_audio_command() noexcept
{
	return impl_->scene ? impl_->scene->take_audio_command() : std::nullopt;
}

std::expected<present::SourceSurface, NewClassicNetplayPostMatchError>
NewClassicNetplayPostMatch::compose_shown_surface(
	std::span<present::ComposedPixel, present::graphics_page_pixels> output
) const
{
	const present::Raster *raster = nullptr;
	if(impl_->completion) {
		raster = &impl_->completion->raster();
	} else if(impl_->scene) {
		raster = &impl_->scene->raster();
	}
	if(!raster || !impl_->font) {
		return std::unexpected(post_match_error(
			NewClassicNetplayPostMatchErrorCode::surface_failed
		));
	}
	auto surface = present::compose_source_surface(
		output, *raster, impl_->tram, *impl_->font
	);
	if(!surface) {
		return std::unexpected(NewClassicNetplayPostMatchError{
			.code = NewClassicNetplayPostMatchErrorCode::surface_failed,
			.surface_error = std::move(surface.error()),
		});
	}
	return *surface;
}

NewClassicNetplayPostMatchStatus
NewClassicNetplayPostMatch::status() const noexcept
{
	return impl_->status;
}

} // namespace th03::app

#include "th03/app/new_classic_netplay_match_intro.hpp"

#include "classic_mainl_stage_internal.hpp"
#include "mainl_compiled_text_internal.hpp"
#include "new_classic_language_internal.hpp"

#include <fstream>
#include <limits>
#include <utility>
#include <vector>

namespace th03::app {
namespace {

[[nodiscard]] NewClassicNetplayMatchIntroError intro_error(
	NewClassicNetplayMatchIntroErrorCode code
) noexcept
{
	return NewClassicNetplayMatchIntroError{.code = code};
}

[[nodiscard]] std::expected<
	std::vector<std::byte>,
	NewClassicNetplayMatchIntroError
> read_file(
	const std::filesystem::path &path,
	std::optional<std::uintmax_t> exact_size = std::nullopt
)
{
	std::error_code error{};
	const auto status = std::filesystem::status(path, error);
	if(error || !std::filesystem::exists(status)) {
		return std::unexpected(intro_error(
			NewClassicNetplayMatchIntroErrorCode::file_missing
		));
	}
	if(!std::filesystem::is_regular_file(status)) {
		return std::unexpected(intro_error(
			NewClassicNetplayMatchIntroErrorCode::file_not_regular
		));
	}
	const auto size = std::filesystem::file_size(path, error);
	if(error || size > static_cast<std::uintmax_t>(
		std::numeric_limits<std::size_t>::max()
	) || size > static_cast<std::uintmax_t>(
		std::numeric_limits<std::streamsize>::max()
	)) {
		return std::unexpected(intro_error(
			NewClassicNetplayMatchIntroErrorCode::file_size_failed
		));
	}
	if(exact_size && size != *exact_size) {
		return std::unexpected(intro_error(
			NewClassicNetplayMatchIntroErrorCode::file_size_mismatch
		));
	}
	std::ifstream input(path, std::ios::binary);
	if(!input) {
		return std::unexpected(intro_error(
			NewClassicNetplayMatchIntroErrorCode::file_open_failed
		));
	}
	std::vector<std::byte> bytes(static_cast<std::size_t>(size));
	if(!bytes.empty()) {
		input.read(
			reinterpret_cast<char *>(bytes.data()),
			static_cast<std::streamsize>(bytes.size())
		);
	}
	if(!input || input.peek() != std::ifstream::traits_type::eof()) {
		return std::unexpected(intro_error(
			NewClassicNetplayMatchIntroErrorCode::file_read_failed
		));
	}
	return bytes;
}

} // namespace

struct NewClassicNetplayMatchIntro::Impl final {
	std::vector<std::byte> font_rom{};
	std::optional<Pc98FontRomProvider> font{};
	std::unique_ptr<ClassicMainlStageTransition> scene{};
	NewClassicNetplayMatchIntroStatus status =
		NewClassicNetplayMatchIntroStatus::running;
};

std::expected<
	std::unique_ptr<NewClassicNetplayMatchIntro>,
	NewClassicNetplayMatchIntroError
> NewClassicNetplayMatchIntro::create(
	NewClassicNetplayMatchIntroRequest request
)
{
	if(request.content.data_root.empty() ||
		request.content.mainl_executable.empty() ||
		request.content.font_rom.empty() ||
		!valid_main_direct_vs_config(request.process)) {
		return std::unexpected(intro_error(
			NewClassicNetplayMatchIntroErrorCode::invalid_request
		));
	}
	auto mainl = read_file(request.content.mainl_executable);
	if(!mainl) {
		return std::unexpected(std::move(mainl.error()));
	}
	auto imported = import_mainl_stage_text(*mainl);
	if(!imported) {
		return std::unexpected(intro_error(
			NewClassicNetplayMatchIntroErrorCode::mainl_text_failed
		));
	}
	auto font_rom = read_file(
		request.content.font_rom, Pc98FontRomProvider::font_rom_size
	);
	if(!font_rom) {
		return std::unexpected(std::move(font_rom.error()));
	}

	MainResourceLoader loader(request.content.data_root);
	std::optional<NewClassicGameTextCatalog> custom_game_text{};
	const NewClassicGameTextCatalog *game_text =
		&new_classic_builtin_english_game_text();
	if(!request.content.game_text_pack.empty()) {
		auto loaded = load_new_classic_game_text_pack(
			request.content.game_text_pack, game_text
		);
		if(!loaded) {
			return std::unexpected(NewClassicNetplayMatchIntroError{
				.code = NewClassicNetplayMatchIntroErrorCode::game_text_pack_failed,
				.game_text_pack_error = std::move(loaded.error()),
			});
		}
		custom_game_text.emplace(std::move(*loaded));
		game_text = &*custom_game_text;
	}
	if(request.language == NewClassicLanguageAssetProfile::english) {
		if(request.content.language_overlay.empty()) {
			return std::unexpected(intro_error(
				NewClassicNetplayMatchIntroErrorCode::invalid_request
			));
		}
		auto overlay = load_new_classic_language_overlay(
			request.content.language_overlay
		);
		if(!overlay) {
			return std::unexpected(NewClassicNetplayMatchIntroError{
				.code = NewClassicNetplayMatchIntroErrorCode::
					language_overlay_failed,
				.language_overlay_error = std::move(overlay.error()),
			});
		}
		if(!loader.install_authenticated_overrides(
			std::move(*overlay).release_members()
		)) {
			return std::unexpected(intro_error(
				NewClassicNetplayMatchIntroErrorCode::language_overlay_failed
			));
		}
		apply_new_classic_english_stage_text(
			imported->assets, *game_text
		);
	}

	const auto init = main_direct_vs_game_init(request.process);
	auto resources = loader.load_mainl_stage({
		.player_one = request.process.player_one,
		.player_two = request.process.player_two,
		.game_mode = init.game_mode,
		.story_stage = init.story_stage,
	});
	if(!resources) {
		return std::unexpected(NewClassicNetplayMatchIntroError{
			.code = NewClassicNetplayMatchIntroErrorCode::resource_load_failed,
			.resource_error = std::move(resources.error()),
		});
	}

	auto impl = std::make_unique<Impl>();
	impl->font_rom = std::move(*font_rom);
	auto font = Pc98FontRomProvider::from_exact_font_rom(impl->font_rom);
	if(!font) {
		return std::unexpected(NewClassicNetplayMatchIntroError{
			.code = NewClassicNetplayMatchIntroErrorCode::font_invalid,
			.font_error = std::move(font.error()),
		});
	}
	impl->font.emplace(std::move(*font));
	auto scene = ClassicMainlStageTransition::create_from_restart(
		init, request.process.kotohime_compatibility, *resources,
		std::move(imported->assets), *impl->font, request.process.game_options
	);
	if(!scene) {
		return std::unexpected(intro_error(
			NewClassicNetplayMatchIntroErrorCode::scene_failed
		));
	}
	impl->scene = std::move(*scene);
	return std::unique_ptr<NewClassicNetplayMatchIntro>(
		new NewClassicNetplayMatchIntro(std::move(impl))
	);
}

NewClassicNetplayMatchIntro::NewClassicNetplayMatchIntro(
	std::unique_ptr<Impl> impl
) noexcept :
	impl_(std::move(impl))
{}

NewClassicNetplayMatchIntro::~NewClassicNetplayMatchIntro() = default;

std::expected<
	NewClassicNetplayMatchIntroTick,
	NewClassicNetplayMatchIntroError
> NewClassicNetplayMatchIntro::advance(bool dismiss_pressed) noexcept
{
	if(impl_->status != NewClassicNetplayMatchIntroStatus::running ||
		!impl_->scene) {
		return std::unexpected(intro_error(
			NewClassicNetplayMatchIntroErrorCode::not_running
		));
	}
	auto advanced = impl_->scene->advance(dismiss_pressed);
	if(!advanced) {
		return std::unexpected(intro_error(
			NewClassicNetplayMatchIntroErrorCode::scene_failed
		));
	}
	if(advanced->handoff_ready) {
		impl_->status = NewClassicNetplayMatchIntroStatus::ready;
	}
	return NewClassicNetplayMatchIntroTick{
		.status = impl_->status,
		.surface_changed = true,
		.input_sampled = advanced->input_accepted,
	};
}

std::optional<ClassicAudioCommand>
NewClassicNetplayMatchIntro::take_audio_command() noexcept
{
	return impl_->scene ? impl_->scene->take_audio_command() : std::nullopt;
}

std::expected<present::SourceSurface, NewClassicNetplayMatchIntroError>
NewClassicNetplayMatchIntro::compose_shown_surface(
	std::span<present::ComposedPixel, present::graphics_page_pixels> output
) const
{
	if(!impl_->scene || !impl_->font) {
		return std::unexpected(intro_error(
			NewClassicNetplayMatchIntroErrorCode::surface_failed
		));
	}
	auto surface = present::compose_source_surface(
		output, impl_->scene->raster(), impl_->scene->tram(), *impl_->font
	);
	if(!surface) {
		return std::unexpected(NewClassicNetplayMatchIntroError{
			.code = NewClassicNetplayMatchIntroErrorCode::surface_failed,
			.surface_error = std::move(surface.error()),
		});
	}
	return *surface;
}

NewClassicNetplayMatchIntroStatus
NewClassicNetplayMatchIntro::status() const noexcept
{
	return impl_->status;
}

} // namespace th03::app

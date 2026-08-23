#include "classic_cutscene_scene_internal.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "th03/assets/decode.hpp"

namespace th03::app {
namespace {

constexpr std::size_t operation_limit = 65536U;
constexpr std::size_t cutscene_picture_count = 24U;

[[nodiscard]] ClassicCutsceneError cutscene_error(
	ClassicCutsceneErrorCode code,
	std::optional<MainResourceKey> resource = std::nullopt
) noexcept
{
	return {.code = code, .resource = resource};
}

[[nodiscard]] ClassicCutsceneError script_error(
	present::CutsceneScriptError error
) noexcept
{
	auto result = cutscene_error(ClassicCutsceneErrorCode::script_failed);
	result.script_error.emplace(error);
	return result;
}

[[nodiscard]] ClassicCutsceneError raster_error(
	present::CutsceneRasterError error
) noexcept
{
	auto result = cutscene_error(ClassicCutsceneErrorCode::raster_failed);
	result.raster_error.emplace(std::move(error));
	return result;
}

[[nodiscard]] ClassicCutsceneError decode_error(
	MainResourceKey key,
	assets::AssetError error
)
{
	auto result = cutscene_error(
		ClassicCutsceneErrorCode::asset_decode_failed, key
	);
	result.asset_error.emplace(std::move(error));
	return result;
}

[[nodiscard]] bool same_resource_spec(
	const MainResourceSpec &actual,
	const MainResourceSpec &canonical
) noexcept
{
	return actual.key == canonical.key && actual.scene == canonical.scene &&
		actual.archive == canonical.archive &&
		actual.decoder == canonical.decoder &&
		actual.logical_role == canonical.logical_role &&
		actual.filename == canonical.filename &&
		actual.expected_bytes == canonical.expected_bytes &&
		actual.expected_fnv1a64 == canonical.expected_fnv1a64;
}

[[nodiscard]] std::expected<void, ClassicCutsceneError>
validate_resource_set(
	std::span<const LoadedMainResource> resources,
	std::span<const MainResourceKey> expected,
	bool authenticate_resource_bytes
)
{
	if(resources.size() != expected.size()) {
		return std::unexpected(cutscene_error(
			ClassicCutsceneErrorCode::resource_set_mismatch
		));
	}
	for(std::size_t index = 0U; index < expected.size(); ++index) {
		const auto key = expected[index];
		const auto &resource = resources[index];
		if(resource.spec.key != key) {
			return std::unexpected(cutscene_error(
				ClassicCutsceneErrorCode::resource_set_mismatch, key
			));
		}
		if(!resource.bytes) {
			return std::unexpected(cutscene_error(
				ClassicCutsceneErrorCode::resource_bytes_missing, key
			));
		}
		const auto canonical = main_resource_spec(key);
		if(!canonical || !same_resource_spec(resource.spec, *canonical)) {
			return std::unexpected(cutscene_error(
				ClassicCutsceneErrorCode::resource_set_mismatch, key
			));
		}
		if(!resource.authenticated_override &&
			(resource.bytes->size() != canonical->expected_bytes ||
			(authenticate_resource_bytes && main_resource_fnv1a64(
				*resource.bytes
			) != canonical->expected_fnv1a64))) {
			return std::unexpected(cutscene_error(
				ClassicCutsceneErrorCode::resource_integrity_failed, key
			));
		}
	}
	return {};
}

[[nodiscard]] const LoadedMainResource *find_resource(
	std::span<const LoadedMainResource> resources,
	MainResourceKey key
) noexcept
{
	const auto found = std::find_if(
		resources.begin(), resources.end(), [&](const auto &resource) {
			return resource.spec.key == key;
		}
	);
	return (found == resources.end()) ? nullptr : &*found;
}

[[nodiscard]] bool valid_picture(const assets::PiImage &image) noexcept
{
	if(image.image.width != present::graphics_width ||
		image.image.height != present::graphics_height || !image.palette ||
		image.image.indices.size() != present::graphics_page_pixels) {
		return false;
	}
	return std::all_of(
		image.image.indices.begin(), image.image.indices.end(),
		[](std::uint8_t color) { return color <= 0x0FU; }
	);
}

[[nodiscard]] char ascii_fold(char value) noexcept
{
	if(value >= 'a' && value <= 'z') {
		return static_cast<char>(value - 'a' + 'A');
	}
	return value;
}

[[nodiscard]] bool equal_name(
	std::string_view left,
	std::string_view right
) noexcept
{
	return left.size() == right.size() && std::equal(
		left.begin(), left.end(), right.begin(),
		[](char a, char b) { return ascii_fold(a) == ascii_fold(b); }
	);
}

[[nodiscard]] std::string_view filename_view(
	const present::CutsceneFilename &filename
) noexcept
{
	return {filename.bytes.data(), filename.length};
}

[[nodiscard]] bool applied_operation_changes_surface(
	const present::CutsceneOperation &operation
) noexcept
{
	return std::visit([](const auto &typed) {
		using T = std::remove_cvref_t<decltype(typed)>;
		return std::is_same_v<T, present::CutsceneDrawGlyph> ||
			std::is_same_v<T, present::CutsceneDrawGaiji> ||
			std::is_same_v<T, present::CutsceneRestoreBox> ||
			std::is_same_v<T, present::CutsceneShowPage> ||
			std::is_same_v<T, present::CutsceneSetTone> ||
			std::is_same_v<T, present::CutsceneClearPages> ||
			std::is_same_v<T, present::CutscenePresentBackground> ||
			std::is_same_v<T, present::CutsceneApplyBackgroundPalette> ||
			std::is_same_v<T, present::CutsceneShowPictureQuarter> ||
			std::is_same_v<T, present::CutsceneClearPicture>;
	}, operation);
}

[[nodiscard]] ClassicCutsceneTick tick(
	ClassicCutscenePhase phase,
	ClassicCutsceneBoundary boundary,
	bool surface_changed,
	bool input_sampled = false
) noexcept
{
	return {
		.phase = phase,
		.boundary = boundary,
		.surface_changed = surface_changed,
		.input_sampled = input_sampled,
		.ready = phase == ClassicCutscenePhase::ready,
	};
}

} // namespace

ClassicCutsceneCompletion::ClassicCutsceneCompletion(
	present::Raster raster
) noexcept
	: raster_(std::move(raster))
{
}

const present::Raster &ClassicCutsceneCompletion::raster() const noexcept
{
	return raster_;
}

ClassicCutsceneScene::ClassicCutsceneScene(
	std::unique_ptr<Prepared> prepared,
	const present::AnkGlyphProvider &font,
	present::Raster raster
) noexcept
	: prepared_(std::move(prepared)),
	  font_(&font),
	  raster_(std::move(raster))
{
}

ClassicCutsceneScene::~ClassicCutsceneScene() = default;

std::expected<std::unique_ptr<ClassicCutsceneScene>, ClassicCutsceneError>
ClassicCutsceneScene::create(
	MainlCutsceneResourceRequest request,
	std::span<const LoadedMainResource> resources,
	const present::AnkGlyphProvider &font,
	present::Raster initial_raster,
	const NewClassicCutsceneTextCatalog *localized_text,
	const NewClassicPicturePatchCatalog *localized_pictures
)
{
	return create_impl(
		request, resources, font, std::move(initial_raster), true,
		localized_text, localized_pictures
	);
}

std::expected<std::unique_ptr<ClassicCutsceneScene>, ClassicCutsceneError>
ClassicCutsceneScene::create_impl(
	MainlCutsceneResourceRequest request,
	std::span<const LoadedMainResource> resources,
	const present::AnkGlyphProvider &font,
	present::Raster initial_raster,
	bool authenticate_resource_bytes,
	const NewClassicCutsceneTextCatalog *localized_text,
	const NewClassicPicturePatchCatalog *localized_pictures
)
{
	if(initial_raster.raster_mode() != present::RasterMode::lines_400) {
		return std::unexpected(cutscene_error(
			ClassicCutsceneErrorCode::invalid_request
		));
	}
	const auto expected = mainl_cutscene_resource_keys(request);
	if(!expected) {
		return std::unexpected(cutscene_error(
			ClassicCutsceneErrorCode::invalid_request
		));
	}
	if(const auto valid = validate_resource_set(
		resources, *expected, authenticate_resource_bytes
	); !valid) {
		return std::unexpected(valid.error());
	}

	auto prepared = std::make_unique<Prepared>();
	prepared->request = request;
	const MainResourceKey gaiji_key{
		.role = MainResourceRole::mainl_gaiji_font,
	};
	const auto *gaiji_resource = find_resource(resources, gaiji_key);
	if(!gaiji_resource || !gaiji_resource->bytes) {
		return std::unexpected(cutscene_error(
			ClassicCutsceneErrorCode::resource_set_mismatch, gaiji_key
		));
	}
	auto sheet = assets::decode_bfnt(
		*gaiji_resource->bytes, assets::BfntConsumerPolicy::gaiji
	);
	if(!sheet) {
		return std::unexpected(decode_error(
			gaiji_key, std::move(sheet.error())
		));
	}
	auto gaiji = PreparedGameftGaiji::prepare(*sheet);
	if(!gaiji) {
		return std::unexpected(cutscene_error(
			ClassicCutsceneErrorCode::font_prepare_failed, gaiji_key
		));
	}
	prepared->gaiji.emplace(std::move(*gaiji));

	const auto script_key = (*expected)[1U];
	const auto *script_resource = find_resource(resources, script_key);
	if(!script_resource || !script_resource->bytes) {
		return std::unexpected(cutscene_error(
			ClassicCutsceneErrorCode::resource_set_mismatch, script_key
		));
	}
	const auto script_bytes = std::span<const std::byte>{*script_resource->bytes};
	const auto script = std::span<const std::uint8_t>{
		reinterpret_cast<const std::uint8_t *>(script_bytes.data()),
		script_bytes.size(),
	};
	if(localized_text) {
		const auto script_value = static_cast<std::size_t>(
			script_key.cutscene_script
		);
		if(script_value == 0U ||
			script_value > new_classic_cutscene_script_count) {
			return std::unexpected(cutscene_error(
				ClassicCutsceneErrorCode::invalid_request, script_key
			));
		}
		auto replacements = localized_text->replacement_glyphs(
			script_value - 1U, script
		);
		if(!replacements) {
			auto result = cutscene_error(
				ClassicCutsceneErrorCode::localized_script_failed, script_key
			);
			result.cutscene_text_error.emplace(std::move(replacements.error()));
			return std::unexpected(std::move(result));
		}
		prepared->localized_glyphs = std::move(*replacements);
		prepared->localized_script_index = script_value - 1U;
	}
	auto vm = present::CutsceneScriptVm::create(script);
	if(!vm) {
		return std::unexpected(script_error(vm.error()));
	}
	prepared->vm.emplace(std::move(*vm));

	for(std::size_t index = 0U; index < cutscene_picture_count; ++index) {
		const auto key = (*expected)[index + 2U];
		const auto *resource = find_resource(resources, key);
		if(!resource || !resource->bytes) {
			return std::unexpected(cutscene_error(
				ClassicCutsceneErrorCode::resource_set_mismatch, key
			));
		}
		auto image = assets::decode_pi(*resource->bytes);
		if(!image) {
			return std::unexpected(decode_error(
				key, std::move(image.error())
			));
		}
		if(!valid_picture(*image)) {
			return std::unexpected(cutscene_error(
				ClassicCutsceneErrorCode::invalid_asset_geometry, key
			));
		}
		if(localized_pictures) {
			const auto spec = main_resource_spec(key);
			if(!spec) {
				return std::unexpected(cutscene_error(
					ClassicCutsceneErrorCode::resource_set_mismatch, key
				));
			}
			auto applied = localized_pictures->apply(spec->filename, *image);
			if(!applied) {
				auto result = cutscene_error(
					ClassicCutsceneErrorCode::localized_picture_failed, key
				);
				result.picture_patch_error.emplace(std::move(applied.error()));
				return std::unexpected(std::move(result));
			}
		}
		prepared->pictures[index] = std::move(*image);
	}

	return std::unique_ptr<ClassicCutsceneScene>(
		new ClassicCutsceneScene(
			std::move(prepared), font, std::move(initial_raster)
		)
	);
}

std::expected<void, ClassicCutsceneError>
ClassicCutsceneScene::begin_prelude(bool &surface_changed)
{
	if(!prepared_ || !prepared_->vm || !font_ || began_vm_) {
		return std::unexpected(cutscene_error(
			ClassicCutsceneErrorCode::invalid_state
		));
	}
	if(!prelude_started_) {
		prelude_started_ = true;
		if(prepared_->request.kind == MainlCutsceneKind::ending) {
			raster_.set_palette_tone(0);
			raster_.apply_pending_palette();
			surface_changed = true;
			pending_ = PendingKind::frames;
			wait_remaining_ = 96U;
			return {};
		}
	}
	if(pending_ != PendingKind::none) {
		return {};
	}
	const present::Rect page{
		0, 0, present::graphics_width, present::graphics_height,
	};
	if(prepared_->request.kind == MainlCutsceneKind::ending) {
		if(!raster_.set_access_page(present::Page::zero) ||
			!raster_.set_shown_page(present::Page::zero) ||
			!raster_.clear(page, 0U)) {
			return std::unexpected(cutscene_error(
				ClassicCutsceneErrorCode::raster_failed
			));
		}
		surface_changed = true;
	} else if(prepared_->request.kind == MainlCutsceneKind::extra_ending) {
		if(!raster_.set_access_page(present::Page::one) ||
			!raster_.clear(page, 0U) ||
			!raster_.set_access_page(present::Page::zero) ||
			!raster_.set_shown_page(present::Page::zero) ||
			!raster_.clear(page, 0U)) {
			return std::unexpected(cutscene_error(
				ClassicCutsceneErrorCode::raster_failed
			));
		}
		surface_changed = true;
	} else {
		// MAINL's interstage entry selects and clears page 0 immediately before
		// cutscene_animate(). Page 1 remains the black backing page inherited
		// from win_animate_and_wait(); clearing only page 0 removes the winner
		// logo without inventing a second page mutation.
		if(!raster_.set_access_page(present::Page::zero) ||
			!raster_.set_shown_page(present::Page::zero) ||
			!raster_.clear(page, 0U)) {
			return std::unexpected(cutscene_error(
				ClassicCutsceneErrorCode::raster_failed
			));
		}
		surface_changed = true;
	}
	if(prepared_->gaiji) {
		prepared_->gaiji->install(tram_);
	}
	auto presenter = present::CutsceneRasterPresenter::create(
		raster_, *font_, tram_
	);
	if(!presenter) {
		return std::unexpected(raster_error(std::move(presenter.error())));
	}
	presenter_.emplace(std::move(*presenter));
	auto prologue = prepared_->vm->begin();
	if(!prologue) {
		return std::unexpected(script_error(prologue.error()));
	}
	for(const auto &operation : prologue->operations()) {
		auto dispatch = presenter_->execute(operation);
		if(!dispatch) {
			return std::unexpected(raster_error(std::move(dispatch.error())));
		}
		if(*dispatch != present::CutsceneRasterDispatch::applied) {
			return std::unexpected(cutscene_error(
				ClassicCutsceneErrorCode::invalid_state
			));
		}
		surface_changed = surface_changed ||
			applied_operation_changes_surface(operation);
	}
	began_vm_ = true;
	phase_ = ClassicCutscenePhase::running;
	return {};
}

void ClassicCutsceneScene::begin_wait(
	const present::CutsceneWait &operation,
	const ClassicCutsceneInput &input
)
{
	wait_remaining_ = 0U;
	wait_target_ = 0U;
	wait_audio_active_ = false;
	wait_poll_due_ = false;
	switch(operation.kind) {
	case present::CutsceneWaitKind::frames:
		pending_ = PendingKind::frames;
		wait_remaining_ = operation.first;
		break;
	case present::CutsceneWaitKind::input_change:
		pending_ = PendingKind::input_change_release;
		wait_remaining_ = operation.first;
		wait_target_ = operation.first;
		break;
	case present::CutsceneWaitKind::input_ok:
		pending_ = PendingKind::input_ok;
		wait_remaining_ = operation.first;
		wait_poll_due_ = true;
		break;
	case present::CutsceneWaitKind::measure:
		pending_ = PendingKind::measure;
		wait_target_ = operation.first;
		wait_remaining_ = operation.second;
		wait_audio_active_ = input.audio.bgm_active;
		break;
	case present::CutsceneWaitKind::input_ok_or_measure:
		pending_ = PendingKind::input_ok_or_measure;
		wait_target_ = operation.first;
		wait_remaining_ = operation.second;
		wait_audio_active_ = input.audio.bgm_active;
		wait_poll_due_ = true;
		break;
	}
}

std::expected<std::optional<ClassicCutsceneTick>, ClassicCutsceneError>
ClassicCutsceneScene::advance_pending(
	const ClassicCutsceneInput &input,
	bool &surface_changed,
	bool &input_sampled
)
{
	const bool any_key = input.script.any_key_held ||
		input.script.cancel_held || input.shot_or_ok_held;
	while(true) {
		switch(pending_) {
		case PendingKind::none:
			return std::nullopt;

		case PendingKind::frames:
			if(wait_remaining_ == 0U) {
				pending_ = PendingKind::none;
				return std::nullopt;
			}
			--wait_remaining_;
			return tick(
				phase_, ClassicCutsceneBoundary::vsync, surface_changed
			);

		case PendingKind::input_change_release:
			input_sampled = true;
			if(any_key) {
				return tick(
					phase_, ClassicCutsceneBoundary::vsync,
					surface_changed, true
				);
			}
			pending_ = PendingKind::input_change_press;
			return tick(
				phase_, ClassicCutsceneBoundary::immediate,
				surface_changed, true
			);

		case PendingKind::input_change_press:
			if(wait_target_ != 0U && wait_remaining_ == 0U) {
				pending_ = PendingKind::none;
				return std::nullopt;
			}
			input_sampled = true;
			if(any_key) {
				pending_ = PendingKind::none;
				return std::nullopt;
			}
			if(wait_target_ != 0U) {
				--wait_remaining_;
			}
			return tick(
				phase_, ClassicCutsceneBoundary::vsync,
				surface_changed, true
			);

		case PendingKind::input_ok:
			if(wait_remaining_ == 0U && !wait_poll_due_) {
				pending_ = PendingKind::none;
				return std::nullopt;
			}
			input_sampled = true;
			wait_poll_due_ = false;
			if(input.shot_or_ok_held) {
				pending_ = PendingKind::none;
				return std::nullopt;
			}
			if(wait_remaining_ == 0U) {
				pending_ = PendingKind::none;
				return std::nullopt;
			}
			--wait_remaining_;
			return tick(
				phase_, ClassicCutsceneBoundary::vsync,
				surface_changed, true
			);

		case PendingKind::measure:
			if(wait_audio_active_) {
				if(input.audio.measure >= wait_target_) {
					pending_ = PendingKind::none;
					return std::nullopt;
				}
				return tick(
					phase_, ClassicCutsceneBoundary::audio_measure,
					surface_changed
				);
			}
			if(wait_remaining_ == 0U) {
				pending_ = PendingKind::none;
				return std::nullopt;
			}
			--wait_remaining_;
			return tick(
				phase_, ClassicCutsceneBoundary::vsync, surface_changed
			);

		case PendingKind::input_ok_or_measure:
			if(wait_audio_active_) {
				input_sampled = true;
				wait_poll_due_ = false;
				if(input.audio.measure >= wait_target_ ||
					input.shot_or_ok_held) {
					pending_ = PendingKind::none;
					return std::nullopt;
				}
				return tick(
					phase_, ClassicCutsceneBoundary::audio_measure,
					surface_changed, true
				);
			}
			if(wait_remaining_ == 0U && !wait_poll_due_) {
				pending_ = PendingKind::none;
				return std::nullopt;
			}
			input_sampled = true;
			wait_poll_due_ = false;
			if(input.shot_or_ok_held) {
				pending_ = PendingKind::none;
				return std::nullopt;
			}
			if(wait_remaining_ == 0U) {
				pending_ = PendingKind::none;
				return std::nullopt;
			}
			--wait_remaining_;
			return tick(
				phase_, ClassicCutsceneBoundary::vsync,
				surface_changed, true
			);

		case PendingKind::palette_fade_wait:
			if(wait_remaining_ > 0U) {
				--wait_remaining_;
				return tick(
					phase_, ClassicCutsceneBoundary::vsync,
					surface_changed
				);
			}
			pending_ = PendingKind::palette_fade_initial;
			break;

		case PendingKind::palette_fade_initial:
			if(!palette_fade_ || !presenter_) {
				return std::unexpected(cutscene_error(
					ClassicCutsceneErrorCode::invalid_state
				));
			}
			if(palette_fade_sample_ >=
				present::cutscene_palette_fade_sample_count) {
				palette_fade_.reset();
				pending_ = PendingKind::none;
				return std::nullopt;
			}
			{
				auto sampled = presenter_->palette_fade_sample(
					*palette_fade_, palette_fade_sample_
				);
				if(!sampled) {
					return std::unexpected(raster_error(
						std::move(sampled.error())
					));
				}
			}
			surface_changed = true;
			++palette_fade_sample_;
			if(palette_fade_sample_ >=
				present::cutscene_palette_fade_sample_count) {
				palette_fade_.reset();
				pending_ = PendingKind::none;
				return std::nullopt;
			}
			if(palette_fade_->speed == 0U) {
				break;
			}
			pending_ = PendingKind::palette_fade_wait;
			wait_remaining_ = palette_fade_->speed;
			--wait_remaining_;
			return tick(
				phase_, ClassicCutsceneBoundary::vsync, surface_changed
			);

		case PendingKind::crossfade_wait:
			if(!crossfade_ || !presenter_) {
				return std::unexpected(cutscene_error(
					ClassicCutsceneErrorCode::invalid_state
				));
			}
			if(wait_remaining_ > 0U) {
				--wait_remaining_;
				return tick(
					phase_, ClassicCutsceneBoundary::vsync,
					surface_changed
				);
			}
			if(crossfade_step_ >= present::cutscene_crossfade_step_count) {
				crossfade_.reset();
				pending_ = PendingKind::none;
				return std::nullopt;
			}
			{
				auto stepped = presenter_->crossfade_step(
					*crossfade_,
					static_cast<present::CutsceneCrossfadeStep>(
						crossfade_step_
					)
				);
				if(!stepped) {
					return std::unexpected(raster_error(
						std::move(stepped.error())
					));
				}
			}
			surface_changed = true;
			++crossfade_step_;
			if(crossfade_step_ >= present::cutscene_crossfade_step_count) {
				crossfade_.reset();
				pending_ = PendingKind::none;
				return std::nullopt;
			}
			if(crossfade_->frames_per_mask == 0U ||
				!crossfade_->animate_masks) {
				break;
			}
			wait_remaining_ = crossfade_->frames_per_mask;
			--wait_remaining_;
			return tick(
				phase_, ClassicCutsceneBoundary::vsync, surface_changed
			);

		case PendingKind::shake_wait:
			if(!shake_ || !presenter_) {
				return std::unexpected(cutscene_error(
					ClassicCutsceneErrorCode::invalid_state
				));
			}
			if(wait_remaining_ > 0U) {
				--wait_remaining_;
				return tick(
					phase_, ClassicCutsceneBoundary::vsync,
					surface_changed
				);
			}
			if(shake_position_ > shake_->parameter) {
				presenter_->finish_shake();
				surface_changed = true;
				shake_.reset();
				pending_ = PendingKind::none;
				return std::nullopt;
			}
			{
				auto stepped = presenter_->shake_step(
					*shake_, static_cast<std::uint16_t>(shake_position_)
				);
				if(!stepped) {
					return std::unexpected(raster_error(
						std::move(stepped.error())
					));
				}
			}
			surface_changed = true;
			++shake_position_;
			if(!shake_->delay_each_position) {
				break;
			}
			wait_remaining_ = 1U;
			--wait_remaining_;
			return tick(
				phase_, ClassicCutsceneBoundary::vsync, surface_changed
			);
		}
	}
}

std::expected<void, ClassicCutsceneError>
ClassicCutsceneScene::load_next_batch(
	const ClassicCutsceneInput &input,
	bool &input_sampled
)
{
	if(!prepared_ || !prepared_->vm || !began_vm_) {
		return std::unexpected(cutscene_error(
			ClassicCutsceneErrorCode::invalid_state
		));
	}
	auto batch = prepared_->vm->step(input.script);
	input_sampled = true;
	if(!batch) {
		return std::unexpected(script_error(batch.error()));
	}
	const auto localized_error = [&]() {
		auto result = cutscene_error(
			ClassicCutsceneErrorCode::localized_script_failed
		);
		result.cutscene_text_error.emplace(NewClassicCutsceneTextPackError{
			.code = NewClassicCutsceneTextPackErrorCode::run_mismatch,
			.offset = prepared_->vm->byte_offset(),
			.script_index = prepared_->localized_script_index,
			.run_index = localized_glyph_index_,
		});
		return result;
	};
	for(std::size_t index = 0U; index < batch->count; ++index) {
		if(auto *glyph = std::get_if<present::CutsceneDrawGlyph>(
			&batch->storage[index]
		)) {
			if(!prepared_->localized_glyphs.empty()) {
				if(localized_glyph_index_ >= prepared_->localized_glyphs.size()) {
					return std::unexpected(localized_error());
				}
				glyph->shift_jis =
					prepared_->localized_glyphs[localized_glyph_index_++];
			}
		}
		if(std::holds_alternative<present::CutsceneFinish>(
			batch->storage[index]
		) && !prepared_->localized_glyphs.empty() &&
			localized_glyph_index_ != prepared_->localized_glyphs.size()) {
			return std::unexpected(localized_error());
		}
	}
	batch_ = std::move(*batch);
	batch_index_ = 0U;
	batch_active_ = batch_.count != 0U;
	return {};
}

std::expected<void, ClassicCutsceneError>
ClassicCutsceneScene::load_picture(
	const present::CutsceneFilename &filename
)
{
	if(!prepared_ || !presenter_) {
		return std::unexpected(cutscene_error(
			ClassicCutsceneErrorCode::invalid_state
		));
	}
	const auto name = filename_view(filename);
	for(std::uint8_t raw = 1U;
		raw <= static_cast<std::uint8_t>(MainCutscenePicture::dm4a);
		++raw) {
		const MainResourceKey key{
			.role = MainResourceRole::cutscene_picture,
			.cutscene_picture = static_cast<MainCutscenePicture>(raw),
		};
		const auto spec = main_resource_spec(key);
		if(spec && equal_name(name, spec->filename)) {
			auto replaced = presenter_->replace_background(
				prepared_->pictures[static_cast<std::size_t>(raw - 1U)]
			);
			if(!replaced) {
				return std::unexpected(raster_error(
					std::move(replaced.error())
				));
			}
			return {};
		}
	}
	return std::unexpected(cutscene_error(
		ClassicCutsceneErrorCode::unknown_picture
	));
}

std::optional<ClassicMusicCue> ClassicCutsceneScene::music_cue(
	const present::CutsceneFilename &filename
) const noexcept
{
	constexpr std::array cues{
		ClassicMusicCue::cutscene_one,
		ClassicMusicCue::cutscene_two,
		ClassicMusicCue::cutscene_three,
		ClassicMusicCue::cutscene_four,
		ClassicMusicCue::cutscene_five,
	};
	const auto name = filename_view(filename);
	for(std::uint8_t raw = 1U;
		raw <= static_cast<std::uint8_t>(MainCutsceneMusic::demo5);
		++raw) {
		const MainResourceKey key{
			.role = MainResourceRole::cutscene_music,
			.cutscene_music = static_cast<MainCutsceneMusic>(raw),
		};
		const auto spec = main_resource_spec(key);
		if(spec && equal_name(name, spec->filename)) {
			return cues[static_cast<std::size_t>(raw - 1U)];
		}
	}
	return std::nullopt;
}

bool ClassicCutsceneScene::push_audio_command(
	ClassicAudioCommand command
) noexcept
{
	if(audio_count_ >= audio_commands_.size()) {
		return false;
	}
	const auto tail = (audio_head_ + audio_count_) % audio_commands_.size();
	audio_commands_[tail] = command;
	++audio_count_;
	return true;
}

std::expected<std::optional<ClassicCutsceneTick>, ClassicCutsceneError>
ClassicCutsceneScene::execute_operation(
	const present::CutsceneOperation &operation,
	const ClassicCutsceneInput &input,
	bool &surface_changed,
	bool &input_sampled
)
{
	if(!presenter_) {
		return std::unexpected(cutscene_error(
			ClassicCutsceneErrorCode::invalid_state
		));
	}
	auto dispatch = presenter_->execute(operation);
	if(!dispatch) {
		return std::unexpected(raster_error(std::move(dispatch.error())));
	}
	switch(*dispatch) {
	case present::CutsceneRasterDispatch::applied:
		surface_changed = surface_changed ||
			applied_operation_changes_surface(operation);
		return std::nullopt;

	case present::CutsceneRasterDispatch::background_required: {
		const auto *load = std::get_if<present::CutsceneLoadBackground>(
			&operation
		);
		if(!load) {
			return std::unexpected(cutscene_error(
				ClassicCutsceneErrorCode::invalid_state
			));
		}
		auto loaded = load_picture(load->filename);
		if(!loaded) {
			return std::unexpected(std::move(loaded.error()));
		}
		return std::nullopt;
	}

	case present::CutsceneRasterDispatch::palette_fade_required: {
		const auto *fade = std::get_if<present::CutscenePaletteFade>(&operation);
		if(!fade) {
			return std::unexpected(cutscene_error(
				ClassicCutsceneErrorCode::invalid_state
			));
		}
		palette_fade_.emplace(*fade);
		palette_fade_sample_ = 0U;
		pending_ = PendingKind::palette_fade_initial;
		return tick(
			phase_, ClassicCutsceneBoundary::vsync, surface_changed
		);
	}

	case present::CutsceneRasterDispatch::crossfade_required: {
		const auto *crossfade =
			std::get_if<present::CutsceneCrossfadePicture>(&operation);
		if(!crossfade) {
			return std::unexpected(cutscene_error(
				ClassicCutsceneErrorCode::invalid_state
			));
		}
		crossfade_.emplace(*crossfade);
		crossfade_step_ = 0U;
		wait_remaining_ = 0U;
		pending_ = PendingKind::crossfade_wait;
		return advance_pending(input, surface_changed, input_sampled);
	}

	case present::CutsceneRasterDispatch::shake_required: {
		const auto *shake = std::get_if<present::CutsceneShake>(&operation);
		if(!shake) {
			return std::unexpected(cutscene_error(
				ClassicCutsceneErrorCode::invalid_state
			));
		}
		shake_.emplace(*shake);
		shake_position_ = 0U;
		wait_remaining_ = 0U;
		pending_ = PendingKind::shake_wait;
		return advance_pending(input, surface_changed, input_sampled);
	}

	case present::CutsceneRasterDispatch::external:
		if(const auto *wait = std::get_if<present::CutsceneWait>(&operation)) {
			begin_wait(*wait, input);
			if(wait->kind == present::CutsceneWaitKind::input_change ||
				wait->kind == present::CutsceneWaitKind::input_ok ||
				wait->kind == present::CutsceneWaitKind::input_ok_or_measure) {
				return tick(
					phase_, ClassicCutsceneBoundary::immediate,
					surface_changed, input_sampled
				);
			}
			return advance_pending(
				input, surface_changed, input_sampled
			);
		}
		if(const auto *fade =
			std::get_if<present::CutsceneAudioFade>(&operation)) {
			if(!push_audio_command({
				.type = ClassicAudioCommandType::fade_song,
				.cue = loaded_music_,
				.parameter = fade->parameter,
			})) {
				return std::unexpected(cutscene_error(
					ClassicCutsceneErrorCode::invalid_state
				));
			}
			return tick(
				phase_, ClassicCutsceneBoundary::immediate, surface_changed
			);
		}
		if(const auto *effect =
			std::get_if<present::CutscenePlaySoundEffect>(&operation)) {
			if(!push_audio_command({
				.type = ClassicAudioCommandType::play_effect,
				.parameter = effect->id,
			})) {
				return std::unexpected(cutscene_error(
					ClassicCutsceneErrorCode::invalid_state
				));
			}
			return tick(
				phase_, ClassicCutsceneBoundary::immediate, surface_changed
			);
		}
		if(const auto *bgm = std::get_if<present::CutsceneBgm>(&operation)) {
			if(bgm->action == present::CutsceneBgmAction::stop) {
				if(!push_audio_command({
					.type = ClassicAudioCommandType::stop_song,
					.cue = loaded_music_,
				})) {
					return std::unexpected(cutscene_error(
						ClassicCutsceneErrorCode::invalid_state
					));
				}
			} else if(bgm->action == present::CutsceneBgmAction::play) {
				if(!push_audio_command({
					.type = ClassicAudioCommandType::play_song,
					.cue = loaded_music_,
				})) {
					return std::unexpected(cutscene_error(
						ClassicCutsceneErrorCode::invalid_state
					));
				}
			} else {
				const auto cue = music_cue(bgm->filename);
				if(!cue) {
					return std::unexpected(cutscene_error(
						ClassicCutsceneErrorCode::unknown_music
					));
				}
				if(!push_audio_command({
					.type = ClassicAudioCommandType::stop_song,
				}) || !push_audio_command({
					.type = ClassicAudioCommandType::load_song,
					.cue = *cue,
				}) || !push_audio_command({
					.type = ClassicAudioCommandType::play_song,
					.cue = *cue,
				})) {
					return std::unexpected(cutscene_error(
						ClassicCutsceneErrorCode::invalid_state
					));
				}
				loaded_music_ = *cue;
			}
			return tick(
				phase_, ClassicCutsceneBoundary::immediate, surface_changed
			);
		}
		if(std::holds_alternative<present::CutsceneFinish>(operation)) {
			presenter_.reset();
			completion_.emplace(
				ClassicCutsceneCompletion(std::move(raster_))
			);
			phase_ = ClassicCutscenePhase::ready;
			return tick(
				phase_, ClassicCutsceneBoundary::immediate, surface_changed
			);
		}
		return std::unexpected(cutscene_error(
			ClassicCutsceneErrorCode::invalid_state
		));
	}
	return std::unexpected(cutscene_error(
		ClassicCutsceneErrorCode::invalid_state
	));
}

std::expected<ClassicCutsceneTick, ClassicCutsceneError>
ClassicCutsceneScene::advance(const ClassicCutsceneInput &input)
{
	if(phase_ == ClassicCutscenePhase::ready ||
		phase_ == ClassicCutscenePhase::consumed ||
		phase_ == ClassicCutscenePhase::failed) {
		return std::unexpected(cutscene_error(
			ClassicCutsceneErrorCode::invalid_state
		));
	}
	if(audio_count_ != 0U) {
		return std::unexpected(cutscene_error(
			ClassicCutsceneErrorCode::audio_command_pending
		));
	}

	bool surface_changed = false;
	bool input_sampled = false;
	if(phase_ == ClassicCutscenePhase::prelude) {
		auto begun = begin_prelude(surface_changed);
		if(!begun) {
			fail();
			return std::unexpected(std::move(begun.error()));
		}
	}

	for(std::size_t count = 0U; count < operation_limit; ++count) {
		if(pending_ != PendingKind::none) {
			auto advanced = advance_pending(
				input, surface_changed, input_sampled
			);
			if(!advanced) {
				fail();
				return std::unexpected(std::move(advanced.error()));
			}
			if(*advanced) {
				auto result = **advanced;
				result.input_sampled = result.input_sampled || input_sampled;
				return result;
			}
		}
		if(phase_ == ClassicCutscenePhase::prelude) {
			auto begun = begin_prelude(surface_changed);
			if(!begun) {
				fail();
				return std::unexpected(std::move(begun.error()));
			}
			continue;
		}

		if(batch_active_) {
			if(batch_index_ >= batch_.count) {
				batch_active_ = false;
				continue;
			}
			const auto &operation = batch_.storage[batch_index_];
			auto executed = execute_operation(
				operation, input, surface_changed, input_sampled
			);
			if(!executed) {
				fail();
				return std::unexpected(std::move(executed.error()));
			}
			++batch_index_;
			if(*executed) {
				auto result = **executed;
				result.input_sampled = result.input_sampled || input_sampled;
				return result;
			}
			continue;
		}

		if(input_sampled) {
			return tick(
				phase_, ClassicCutsceneBoundary::immediate,
				surface_changed, true
			);
		}
		auto loaded = load_next_batch(input, input_sampled);
		if(!loaded) {
			fail();
			return std::unexpected(std::move(loaded.error()));
		}
		// Empty batches are valid source iterations (separators and state-only
		// opcodes). Never execute their default-initialized storage.
	}

	fail();
	return std::unexpected(cutscene_error(
		ClassicCutsceneErrorCode::invalid_state
	));
}

std::optional<ClassicAudioCommand>
ClassicCutsceneScene::take_audio_command() noexcept
{
	if(audio_count_ == 0U) {
		return std::nullopt;
	}
	const auto command = audio_commands_[audio_head_];
	audio_head_ = (audio_head_ + 1U) % audio_commands_.size();
	--audio_count_;
	return command;
}

bool ClassicCutsceneScene::audio_command_pending() const noexcept
{
	return audio_count_ != 0U;
}

std::optional<ClassicCutsceneCompletion>
ClassicCutsceneScene::take_completion() noexcept
{
	if(phase_ != ClassicCutscenePhase::ready || !completion_) {
		return std::nullopt;
	}
	phase_ = ClassicCutscenePhase::consumed;
	auto result = std::move(completion_);
	completion_.reset();
	return result;
}

ClassicCutscenePhase ClassicCutsceneScene::phase() const noexcept
{
	return phase_;
}

const present::Raster &ClassicCutsceneScene::raster() const noexcept
{
	return completion_ ? completion_->raster() : raster_;
}

const present::Tram &ClassicCutsceneScene::tram() const noexcept
{
	return tram_;
}

std::size_t ClassicCutsceneScene::localized_glyphs_consumed() const noexcept
{
	return localized_glyph_index_;
}

std::expected<std::unique_ptr<ClassicCutsceneScene>, ClassicCutsceneError>
ClassicCutsceneSceneTestAccess::create_with_synthetic_script(
	std::span<const std::uint8_t> script,
	const present::AnkGlyphProvider &font,
	present::Raster initial_raster,
	MainlCutsceneKind kind
)
{
	if(initial_raster.raster_mode() != present::RasterMode::lines_400) {
		return std::unexpected(cutscene_error(
			ClassicCutsceneErrorCode::invalid_request
		));
	}
	auto vm = present::CutsceneScriptVm::create(script);
	if(!vm) {
		return std::unexpected(script_error(vm.error()));
	}
	auto prepared = std::make_unique<ClassicCutsceneScene::Prepared>();
	prepared->request = {
		.kind = kind,
		.character = MainCharacter::reimu,
	};
	prepared->vm.emplace(std::move(*vm));
	for(auto &picture : prepared->pictures) {
		picture.palette.emplace();
		picture.image.width = present::graphics_width;
		picture.image.height = present::graphics_height;
		picture.image.indices.assign(present::graphics_page_pixels, 0U);
	}
	return std::unique_ptr<ClassicCutsceneScene>(
		new ClassicCutsceneScene(
			std::move(prepared), font, std::move(initial_raster)
		)
	);
}

void ClassicCutsceneScene::fail() noexcept
{
	phase_ = ClassicCutscenePhase::failed;
}

} // namespace th03::app

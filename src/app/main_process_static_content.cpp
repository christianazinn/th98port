#include "main_process_static_content_internal.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <utility>

#include "th03/assets/decode.hpp"
#include "th03/exact/trig.hpp"
#include "th03/formats/enedat.hpp"

namespace th03::app {
namespace {

[[nodiscard]] MainProcessError error(
	MainProcessErrorCode code,
	std::optional<MainResourceKey> resource = std::nullopt
)
{
	return {.code = code, .resource = resource};
}

[[nodiscard]] MainProcessError asset_error(
	MainResourceKey resource,
	assets::AssetError asset
)
{
	auto result = error(MainProcessErrorCode::asset_decode_failed, resource);
	result.asset_error.emplace(std::move(asset));
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

[[nodiscard]] std::expected<
	const LoadedMainResource *,
	MainProcessError
> find_static_resource(
	std::span<const LoadedMainResource> resources,
	MainResourceKey key,
	bool authenticate_resource_bytes
)
{
	const LoadedMainResource *found = nullptr;
	for(const auto &resource : resources) {
		if(resource.spec.key == key) {
			if(found != nullptr) {
				return std::unexpected(error(
					MainProcessErrorCode::resource_set_mismatch,
					key
				));
			}
			found = &resource;
		}
	}
	if(found == nullptr) {
		return std::unexpected(error(
			MainProcessErrorCode::resource_set_mismatch,
			key
		));
	}
	if(!found->bytes) {
		return std::unexpected(error(
			MainProcessErrorCode::resource_bytes_missing,
			key
		));
	}
	const auto canonical = main_resource_spec(key);
	if(!canonical || !same_resource_spec(found->spec, *canonical)) {
		return std::unexpected(error(
			MainProcessErrorCode::resource_set_mismatch,
			key
		));
	}
	if(found->bytes->size() != canonical->expected_bytes ||
		(authenticate_resource_bytes && main_resource_fnv1a64(
			*found->bytes
		) != canonical->expected_fnv1a64)) {
		return std::unexpected(error(
			MainProcessErrorCode::resource_integrity_failed,
			key
		));
	}
	return found;
}

} // namespace

MainProcessStaticContent::MainProcessStaticContent(
	MainCompiledContent compiled,
	std::span<const std::byte> font_rom
) :
	compiled_(std::move(compiled)),
	font_rom_(font_rom.begin(), font_rom.end())
{}

const MainCompiledContent &MainProcessStaticContent::compiled() const noexcept
{
	return compiled_;
}

const Pc98FontRomProvider &MainProcessStaticContent::font() const noexcept
{
	return *font_provider_;
}

const PreparedGameftGaiji &
MainProcessStaticContent::gameft_gaiji() const noexcept
{
	return *gameft_gaiji_;
}

const GameContent &MainProcessStaticContent::game_content() const noexcept
{
	return game_content_;
}

const assets::BfntSheet &MainProcessStaticContent::zikicw_sheet() const noexcept
{
	return zikicw_sheet_;
}

const present::PreparedSuperimposeSheet &
MainProcessStaticContent::lose() const noexcept
{
	return *prepared_lose_;
}

const present::PreparedSuperimposeSheet &
MainProcessStaticContent::round() const noexcept
{
	return *prepared_round_;
}

const present::PreparedSuperimposeSheet &
MainProcessStaticContent::zikicw() const noexcept
{
	return *prepared_zikicw_;
}

present::StaffFlakeTrigTables MainProcessStaticContent::trig() const noexcept
{
	return {sine_q8_, cosine_q8_};
}

std::expected<
	std::shared_ptr<const MainProcessStaticContent>,
	MainProcessError
> MainProcessStaticContentAccess::create(
	std::span<const LoadedMainResource> resources,
	MainCompiledContent compiled,
	std::span<const std::byte> font_rom,
	bool authenticate_resource_bytes
)
{
	constexpr std::array static_keys{
		MainResourceKey{.role = MainResourceRole::enemy_scripts},
		MainResourceKey{.role = MainResourceRole::game_font},
		MainResourceKey{.role = MainResourceRole::hud_lose_sheet},
		MainResourceKey{.role = MainResourceRole::hud_round_sheet},
		MainResourceKey{.role = MainResourceRole::hud_result_sheet},
	};
	std::array<const LoadedMainResource *, static_keys.size()> selected{};
	for(std::size_t index = 0U; index < static_keys.size(); ++index) {
		const auto found = find_static_resource(
			resources,
			static_keys[index],
			authenticate_resource_bytes
		);
		if(!found) {
			return std::unexpected(found.error());
		}
		selected[index] = *found;
	}

	auto candidate = std::shared_ptr<MainProcessStaticContent>(
		new MainProcessStaticContent(std::move(compiled), font_rom)
	);
	const auto font = Pc98FontRomProvider::from_exact_font_rom(
		candidate->font_rom_
	);
	if(!font) {
		return std::unexpected(error(MainProcessErrorCode::font_rom_failed));
	}
	candidate->font_provider_.emplace(*font);

	auto game_content = formats::parse_enedat(*selected[0U]->bytes);
	if(!game_content) {
		auto result = error(
			MainProcessErrorCode::enedat_decode_failed,
			static_keys[0U]
		);
		result.core_error.emplace(std::move(game_content.error()));
		return std::unexpected(std::move(result));
	}
	candidate->game_content_ = std::move(*game_content);

	auto gameft = assets::decode_bfnt(
		*selected[1U]->bytes,
		assets::BfntConsumerPolicy::gaiji
	);
	if(!gameft) {
		return std::unexpected(asset_error(
			static_keys[1U],
			std::move(gameft.error())
		));
	}
	const auto gaiji = PreparedGameftGaiji::prepare(*gameft);
	if(!gaiji) {
		return std::unexpected(error(
			MainProcessErrorCode::gameft_prepare_failed,
			static_keys[1U]
		));
	}
	candidate->gameft_gaiji_.emplace(*gaiji);

	std::array<assets::BfntSheet, 3U> sheets{};
	for(std::size_t index = 0U; index < sheets.size(); ++index) {
		auto decoded = assets::decode_bfnt(
			*selected[index + 2U]->bytes,
			assets::BfntConsumerPolicy::superimpose
		);
		if(!decoded) {
			return std::unexpected(asset_error(
				static_keys[index + 2U],
				std::move(decoded.error())
			));
		}
		sheets[index] = std::move(*decoded);
	}
	candidate->prepared_lose_ = present::prepare_superimpose_sheet(sheets[0U]);
	candidate->prepared_round_ = present::prepare_superimpose_sheet(sheets[1U]);
	candidate->zikicw_sheet_ = std::move(sheets[2U]);
	candidate->prepared_zikicw_ = present::prepare_superimpose_sheet(
		candidate->zikicw_sheet_
	);
	if(!candidate->prepared_lose_ || !candidate->prepared_round_ ||
		!candidate->prepared_zikicw_) {
		return std::unexpected(error(
			MainProcessErrorCode::presenter_setup_failed
		));
	}

	for(std::size_t index = 0U; index < candidate->sine_q8_.size(); ++index) {
		const auto angle = static_cast<std::uint8_t>(index);
		candidate->sine_q8_[index] = exact::sine8(angle);
		candidate->cosine_q8_[index] = exact::cosine8(angle);
	}
	return std::shared_ptr<const MainProcessStaticContent>{std::move(candidate)};
}

} // namespace th03::app

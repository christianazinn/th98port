#include "th03/app/new_classic_cutscene_text.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <new>
#include <ranges>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "th03/app/main_compiled_content.hpp"
#include "th03/present/cutscene_script.hpp"

namespace th03::app {

struct NewClassicCutsceneTextCatalogAccess final {
	static void assign(
		NewClassicCutsceneTextCatalog &catalog,
		std::string locale,
		std::uint64_t source_profile_fnv1a64,
		std::vector<NewClassicCutsceneTextCatalog::Script> scripts
	)
	{
		catalog.locale_ = std::move(locale);
		catalog.source_profile_fnv1a64_ = source_profile_fnv1a64;
		catalog.scripts_ = std::move(scripts);
	}
};

namespace {

constexpr std::size_t header_bytes = 128U;
constexpr std::size_t script_entry_bytes = 24U;
constexpr std::size_t run_entry_bytes = 24U;
constexpr std::size_t maximum_pack_bytes = 2U << 20U;
constexpr std::uint64_t fnv_offset = 14695981039346656037ULL;
constexpr std::uint64_t fnv_prime = 1099511628211ULL;
constexpr std::array<std::byte, 8U> magic{
	std::byte{'T'}, std::byte{'3'}, std::byte{'C'}, std::byte{'T'},
	std::byte{'P'}, std::byte{'0'}, std::byte{'0'}, std::byte{'1'},
};

[[nodiscard]] NewClassicCutsceneTextPackError error(
	NewClassicCutsceneTextPackErrorCode code,
	std::size_t offset = 0U,
	std::size_t script_index = new_classic_cutscene_script_count,
	std::size_t run_index = 0U,
	std::error_code system = {}
) noexcept
{
	return {
		.code = code,
		.offset = offset,
		.script_index = script_index,
		.run_index = run_index,
		.system = system,
	};
}

[[nodiscard]] std::uint16_t u16(
	std::span<const std::byte> bytes,
	std::size_t offset
) noexcept
{
	return static_cast<std::uint16_t>(
		std::to_integer<std::uint8_t>(bytes[offset]) |
		(static_cast<std::uint16_t>(
			std::to_integer<std::uint8_t>(bytes[offset + 1U])
		) << 8U)
	);
}

[[nodiscard]] std::uint32_t u32(
	std::span<const std::byte> bytes,
	std::size_t offset
) noexcept
{
	return std::to_integer<std::uint8_t>(bytes[offset]) |
		(static_cast<std::uint32_t>(
			std::to_integer<std::uint8_t>(bytes[offset + 1U])
		) << 8U) |
		(static_cast<std::uint32_t>(
			std::to_integer<std::uint8_t>(bytes[offset + 2U])
		) << 16U) |
		(static_cast<std::uint32_t>(
			std::to_integer<std::uint8_t>(bytes[offset + 3U])
		) << 24U);
}

[[nodiscard]] std::uint64_t u64(
	std::span<const std::byte> bytes,
	std::size_t offset
) noexcept
{
	std::uint64_t result = 0U;
	for(std::size_t index = 0U; index < 8U; ++index) {
		result |= static_cast<std::uint64_t>(
			std::to_integer<std::uint8_t>(bytes[offset + index])
		) << (index * 8U);
	}
	return result;
}

[[nodiscard]] bool zero_range(
	std::span<const std::byte> bytes,
	std::size_t first,
	std::size_t last
) noexcept
{
	return std::ranges::all_of(
		bytes.subspan(first, last - first),
		[](std::byte value) { return value == std::byte{0U}; }
	);
}

[[nodiscard]] bool valid_locale(std::string_view locale) noexcept
{
	if(locale.empty() || locale.size() > 31U || locale.front() == '-' ||
		locale.back() == '-') {
		return false;
	}
	std::size_t component = 0U;
	bool first_component = true;
	for(const auto character : locale) {
		if(character == '-') {
			if(component > 8U ||
				(first_component ? component < 2U : component < 1U)) {
				return false;
			}
			component = 0U;
			first_component = false;
			continue;
		}
		const auto value = static_cast<unsigned char>(character);
		if(!((value >= 'A' && value <= 'Z') ||
			(value >= 'a' && value <= 'z') ||
			(value >= '0' && value <= '9'))) {
			return false;
		}
		++component;
	}
	return component <= 8U &&
		(first_component ? component >= 2U : component >= 1U);
}

[[nodiscard]] bool valid_glyph_pairs(
	std::span<const std::uint8_t> bytes,
	std::size_t &invalid_offset
) noexcept
{
	if((bytes.size() & 1U) != 0U) {
		invalid_offset = bytes.size();
		return false;
	}
	for(std::size_t offset = 0U; offset < bytes.size(); offset += 2U) {
		const auto first = bytes[offset];
		const auto second = bytes[offset + 1U];
		const auto single = [](std::uint8_t value) noexcept {
			return (value >= 0x20U && value <= 0x7EU) ||
				(value >= 0xA1U && value <= 0xDFU);
		};
		const bool double_byte =
			((first >= 0x81U && first <= 0x9FU) ||
				(first >= 0xE0U && first <= 0xFCU)) &&
			((second >= 0x40U && second <= 0x7EU) ||
				(second >= 0x80U && second <= 0xFCU));
		if(!double_byte && !(single(first) && single(second))) {
			invalid_offset = offset;
			return false;
		}
	}
	return true;
}

[[nodiscard]] std::uint64_t fnv1a64(
	std::span<const std::uint8_t> bytes
) noexcept
{
	auto result = fnv_offset;
	for(const auto value : bytes) {
		result ^= value;
		result *= fnv_prime;
	}
	return result;
}

} // namespace

std::string_view NewClassicCutsceneTextCatalog::locale() const noexcept
{
	return locale_;
}

std::uint64_t
NewClassicCutsceneTextCatalog::source_profile_fnv1a64() const noexcept
{
	return source_profile_fnv1a64_;
}

std::expected<
	std::vector<std::array<std::uint8_t, 2U>>,
	NewClassicCutsceneTextPackError
> NewClassicCutsceneTextCatalog::replacement_glyphs(
	std::size_t script_index,
	std::span<const std::uint8_t> authenticated_script
) const
{
	if(script_index >= scripts_.size()) {
		return std::unexpected(error(
			NewClassicCutsceneTextPackErrorCode::script_mismatch,
			0U, script_index
		));
	}
	const auto &script = scripts_[script_index];
	if(authenticated_script.size() != script.source_bytes ||
		fnv1a64(authenticated_script) != script.source_fnv1a64) {
		return std::unexpected(error(
			NewClassicCutsceneTextPackErrorCode::script_mismatch,
			0U, script_index
		));
	}
	try {
		auto extracted = present::extract_cutscene_text_runs(authenticated_script);
		if(!extracted || extracted->size() != script.runs.size()) {
			return std::unexpected(error(
				NewClassicCutsceneTextPackErrorCode::run_mismatch,
				0U, script_index
			));
		}
		std::vector<std::array<std::uint8_t, 2U>> result{};
		std::size_t glyph_count = 0U;
		for(const auto &run : script.runs) {
			glyph_count += run.glyphs;
		}
		result.reserve(glyph_count);
		for(std::size_t run_index = 0U; run_index < script.runs.size(); ++run_index) {
			const auto &source = (*extracted)[run_index];
			const auto &replacement = script.runs[run_index];
			if(source.ordinal != run_index ||
				source.source_offset != replacement.source_offset ||
				source.shift_jis.size() != replacement.replacement.size() ||
				source.shift_jis.size() !=
					static_cast<std::size_t>(replacement.glyphs) * 2U ||
				fnv1a64(source.shift_jis) != replacement.source_fnv1a64 ||
				replacement.replacement.size() > authenticated_script.size() ||
				replacement.source_offset > authenticated_script.size() -
					replacement.replacement.size()) {
				return std::unexpected(error(
					NewClassicCutsceneTextPackErrorCode::run_mismatch,
					replacement.source_offset, script_index, run_index
				));
			}
			for(std::size_t offset = 0U;
				offset < replacement.replacement.size(); offset += 2U) {
				result.push_back({
					replacement.replacement[offset],
					replacement.replacement[offset + 1U],
				});
			}
		}
		return result;
	} catch(const std::bad_alloc &) {
		return std::unexpected(error(
			NewClassicCutsceneTextPackErrorCode::allocation_failed,
			0U, script_index
		));
	}
}

std::expected<NewClassicCutsceneTextCatalog, NewClassicCutsceneTextPackError>
load_new_classic_cutscene_text_pack(const std::filesystem::path &path)
{
	if(path.empty()) {
		return std::unexpected(error(
			NewClassicCutsceneTextPackErrorCode::invalid_path
		));
	}
	std::error_code system;
	const auto status = std::filesystem::status(path, system);
	if(system) {
		return std::unexpected(error(
			NewClassicCutsceneTextPackErrorCode::source_status, 0U,
			new_classic_cutscene_script_count, 0U, system
		));
	}
	if(!std::filesystem::is_regular_file(status)) {
		return std::unexpected(error(
			NewClassicCutsceneTextPackErrorCode::source_not_regular
		));
	}
	const auto size = std::filesystem::file_size(path, system);
	if(system) {
		return std::unexpected(error(
			NewClassicCutsceneTextPackErrorCode::source_size, 0U,
			new_classic_cutscene_script_count, 0U, system
		));
	}
	if(size < header_bytes || size > maximum_pack_bytes) {
		return std::unexpected(error(
			NewClassicCutsceneTextPackErrorCode::source_size
		));
	}
	try {
		std::ifstream input(path, std::ios::binary);
		if(!input) {
			return std::unexpected(error(
				NewClassicCutsceneTextPackErrorCode::source_open
			));
		}
		std::vector<std::byte> bytes(static_cast<std::size_t>(size));
		input.read(reinterpret_cast<char *>(bytes.data()),
			static_cast<std::streamsize>(bytes.size()));
		if(!input || input.gcount() != static_cast<std::streamsize>(bytes.size())) {
			return std::unexpected(error(
				NewClassicCutsceneTextPackErrorCode::source_read
			));
		}
		const auto view = std::span<const std::byte>{bytes};
		if(!std::ranges::equal(magic, view.first(8U)) ||
			u16(view, 8U) != 1U || u16(view, 10U) != header_bytes ||
			u16(view, 12U) != new_classic_cutscene_script_count ||
			std::to_integer<std::uint8_t>(view[14U]) != 1U ||
			!zero_range(view, 16U, 20U) || u32(view, 36U) != 0U ||
			!zero_range(view, 80U, header_bytes)) {
			return std::unexpected(error(
				NewClassicCutsceneTextPackErrorCode::header_shape
			));
		}
		const auto locale_bytes = std::to_integer<std::uint8_t>(view[15U]);
		const auto run_count = static_cast<std::size_t>(u32(view, 20U));
		const auto scripts_bytes = static_cast<std::size_t>(u32(view, 24U));
		const auto runs_bytes = static_cast<std::size_t>(u32(view, 28U));
		const auto payload_bytes = static_cast<std::size_t>(u32(view, 32U));
		if(locale_bytes == 0U || locale_bytes > 31U || run_count == 0U ||
			run_count > maximum_pack_bytes / run_entry_bytes ||
			scripts_bytes != new_classic_cutscene_script_count *
				script_entry_bytes ||
			runs_bytes != run_count * run_entry_bytes ||
			header_bytes + locale_bytes + scripts_bytes + runs_bytes +
				payload_bytes != view.size()) {
			return std::unexpected(error(
				NewClassicCutsceneTextPackErrorCode::directory_shape
			));
		}
		const auto body = view.subspan(header_bytes);
		const auto body_digest = app_sha256(body);
		if(!std::ranges::equal(
			body_digest, view.subspan(48U, 32U), {},
			[](std::uint8_t value) { return static_cast<std::byte>(value); },
			[](std::byte value) { return value; }
		)) {
			return std::unexpected(error(
				NewClassicCutsceneTextPackErrorCode::body_digest_mismatch, 48U
			));
		}
		const std::string locale{
			reinterpret_cast<const char *>(bytes.data() + header_bytes), locale_bytes
		};
		if(!valid_locale(locale)) {
			return std::unexpected(error(
				NewClassicCutsceneTextPackErrorCode::locale_shape, header_bytes
			));
		}
		const auto script_directory = header_bytes + locale_bytes;
		const auto run_directory = script_directory + scripts_bytes;
		const auto payload = run_directory + runs_bytes;
		std::vector<NewClassicCutsceneTextCatalog::Script> scripts{};
		scripts.reserve(new_classic_cutscene_script_count);
		std::size_t expected_first_run = 0U;
		std::size_t expected_payload = 0U;
		for(std::size_t script_index = 0U;
			script_index < new_classic_cutscene_script_count; ++script_index) {
			const auto entry = script_directory + script_index * script_entry_bytes;
			const auto source_bytes = static_cast<std::size_t>(u32(view, entry));
			const auto script_runs = static_cast<std::size_t>(u32(view, entry + 4U));
			const auto first_run = static_cast<std::size_t>(u32(view, entry + 8U));
			if(source_bytes == 0U || script_runs == 0U ||
				first_run != expected_first_run || u32(view, entry + 12U) != 0U ||
				first_run > run_count ||
				script_runs > run_count - first_run) {
				return std::unexpected(error(
					NewClassicCutsceneTextPackErrorCode::directory_shape,
					entry, script_index
				));
			}
			NewClassicCutsceneTextCatalog::Script script{
				.source_bytes = source_bytes,
				.source_fnv1a64 = u64(view, entry + 16U),
			};
			script.runs.reserve(script_runs);
			for(std::size_t run_index = 0U; run_index < script_runs; ++run_index) {
				const auto directory_index = first_run + run_index;
				const auto run_entry = run_directory +
					directory_index * run_entry_bytes;
				const auto source_offset =
					static_cast<std::size_t>(u32(view, run_entry));
				const auto glyphs = u16(view, run_entry + 4U);
				const auto replacement_offset =
					static_cast<std::size_t>(u32(view, run_entry + 16U));
				const auto replacement_bytes = u16(view, run_entry + 20U);
				if(glyphs == 0U || u16(view, run_entry + 6U) != 0U ||
					u16(view, run_entry + 22U) != 0U ||
					replacement_bytes != static_cast<std::size_t>(glyphs) * 2U ||
					replacement_offset != expected_payload ||
					replacement_offset > payload_bytes ||
					replacement_bytes > payload_bytes - replacement_offset ||
					replacement_bytes > source_bytes ||
					source_offset > source_bytes - replacement_bytes) {
					return std::unexpected(error(
						NewClassicCutsceneTextPackErrorCode::message_shape,
						run_entry, script_index, run_index
					));
				}
				const auto encoded = view.subspan(
					payload + replacement_offset, replacement_bytes
				);
				std::vector<std::uint8_t> replacement{};
				replacement.reserve(replacement_bytes);
				std::ranges::transform(
					encoded, std::back_inserter(replacement),
					[](std::byte value) { return std::to_integer<std::uint8_t>(value); }
				);
				std::size_t invalid_offset = 0U;
				if(!valid_glyph_pairs(replacement, invalid_offset)) {
					return std::unexpected(error(
						NewClassicCutsceneTextPackErrorCode::malformed_shift_jis,
						payload + replacement_offset + invalid_offset,
						script_index, run_index
					));
				}
				script.runs.push_back({
					.source_offset = source_offset,
					.glyphs = glyphs,
					.source_fnv1a64 = u64(view, run_entry + 8U),
					.replacement = std::move(replacement),
				});
				expected_payload += replacement_bytes;
			}
			expected_first_run += script_runs;
			scripts.push_back(std::move(script));
		}
		if(expected_first_run != run_count || expected_payload != payload_bytes) {
			return std::unexpected(error(
				NewClassicCutsceneTextPackErrorCode::directory_shape,
				run_directory
			));
		}
		NewClassicCutsceneTextCatalog result{};
		NewClassicCutsceneTextCatalogAccess::assign(
			result, locale, u64(view, 40U), std::move(scripts)
		);
		return result;
	} catch(const std::bad_alloc &) {
		return std::unexpected(error(
			NewClassicCutsceneTextPackErrorCode::allocation_failed
		));
	}
}

} // namespace th03::app

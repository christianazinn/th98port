#include "th03/formats/enedat.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

#include "bytes.hpp"

namespace th03::formats {
namespace {

constexpr std::size_t header_size = 4U;
constexpr std::size_t formation_max = 24U;
constexpr std::size_t enemies_per_formation_max = 16U;

Error error(std::string message, std::size_t offset = 0U)
{
	return Error{ErrorCode::invalid_value, std::move(message), offset};
}

} // namespace

std::expected<GameContent, Error> parse_enedat(std::span<const std::byte> bytes)
{
	if(bytes.size() < header_size) {
		return std::unexpected(Error{
			ErrorCode::truncated,
			"ENEDAT is shorter than its four-byte header",
		});
	}
	const auto payload_size = detail::u16(bytes, 0U);
	if(detail::u16(bytes, 2U) != 0U) {
		return std::unexpected(error("ENEDAT header reserved word is nonzero", 2U));
	}
	if(static_cast<std::size_t>(payload_size) + header_size != bytes.size()) {
		return std::unexpected(Error{
			ErrorCode::count_mismatch,
			"ENEDAT header size does not equal the file payload",
		});
	}

	GameContent content{};
	std::size_t cursor = header_size;
	while(true) {
		if(cursor + 2U > bytes.size()) {
			return std::unexpected(Error{
				ErrorCode::truncated,
				"ENEDAT has no terminating zero formation count",
				cursor,
			});
		}
		const auto enemy_count = detail::u16(bytes, cursor);
		cursor += 2U;
		if(enemy_count == 0U) {
			break;
		}
		if(content.enemy_formations.size() >= formation_max) {
			return std::unexpected(error("ENEDAT exceeds 24 formations", cursor - 2U));
		}
		if(enemy_count > enemies_per_formation_max) {
			return std::unexpected(error(
				"ENEDAT formation exceeds 16 enemy scripts", cursor - 2U
			));
		}

		EnemyFormation formation{};
		formation.enemy_scripts.reserve(enemy_count);
		for(std::uint16_t enemy = 0U; enemy < enemy_count; ++enemy) {
			if(cursor >= bytes.size()) {
				return std::unexpected(Error{
					ErrorCode::truncated,
					"ENEDAT is missing an enemy script size",
					cursor,
				});
			}
			const auto script_size = detail::u8(bytes, cursor++);
			if(script_size == 0U) {
				return std::unexpected(error("ENEDAT contains an empty enemy script", cursor - 1U));
			}
			if(cursor + script_size > bytes.size()) {
				return std::unexpected(Error{
					ErrorCode::truncated,
					"ENEDAT enemy script extends past the payload",
					cursor,
				});
			}
			auto &script = formation.enemy_scripts.emplace_back();
			script.reserve(script_size);
			for(std::size_t index = 0U; index < script_size; ++index) {
				script.push_back(detail::u8(bytes, cursor + index));
			}
			cursor += script_size;
		}
		content.enemy_formations.push_back(std::move(formation));
	}
	if(cursor != bytes.size()) {
		return std::unexpected(Error{
			ErrorCode::trailing_data,
			"ENEDAT contains bytes after its zero formation count",
			cursor,
		});
	}
	if(content.enemy_formations.empty()) {
		return std::unexpected(error("ENEDAT contains no formations", header_size));
	}
	return content;
}

} // namespace th03::formats

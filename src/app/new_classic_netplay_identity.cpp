#include "th03/app/new_classic_netplay_identity.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace th03::app {
namespace {

constexpr std::array magic{
	std::byte{'T'}, std::byte{'3'}, std::byte{'N'}, std::byte{'I'},
	std::byte{'D'}, std::byte{'0'}, std::byte{'0'}, std::byte{'1'},
};
constexpr std::size_t body_bytes = 192U;

[[nodiscard]] int hexadecimal(char value) noexcept
{
	if(value >= '0' && value <= '9') {
		return value - '0';
	}
	if(value >= 'a' && value <= 'f') {
		return value - 'a' + 10;
	}
	return -1;
}

[[nodiscard]] bool parse_uuid(
	std::string_view source,
	std::span<std::byte, 16U> destination
) noexcept
{
	if(source.size() != 36U) {
		return false;
	}
	std::size_t output = 0U;
	for(std::size_t input = 0U; input < source.size();) {
		if(input == 8U || input == 13U || input == 18U || input == 23U) {
			if(source[input++] != '-') {
				return false;
			}
			continue;
		}
		if(input + 1U >= source.size() || output >= destination.size()) {
			return false;
		}
		const auto high = hexadecimal(source[input++]);
		const auto low = hexadecimal(source[input++]);
		if(high < 0 || low < 0) {
			return false;
		}
		destination[output++] = std::byte{
			static_cast<std::uint8_t>((high << 4U) | low)
		};
	}
	return output == destination.size();
}

[[nodiscard]] bool valid_profile(
	const NewClassicNetplayRoomIdentity &profile
) noexcept
{
	const auto layout = layout_new_classic_nametag(profile.nametag.source);
	return layout && *layout == profile.nametag;
}

} // namespace

std::expected<
	std::array<std::byte, new_classic_netplay_identity_bytes>,
	NewClassicNetplayIdentityError
> encode_new_classic_netplay_identity(
	const NewClassicNetplayIdentitySnapshot &snapshot
) noexcept
{
	std::array<std::byte, new_classic_netplay_identity_bytes> result{};
	std::ranges::copy(magic, result.begin());
	result[8U] = std::byte{1U}; // authenticated account identities
	if(!parse_uuid(snapshot.room_id,
		std::span<std::byte, 16U>(result.data() + 12U, 16U))) {
		return std::unexpected(NewClassicNetplayIdentityError::invalid_room);
	}
	if(snapshot.profiles.player_one.user_id ==
		snapshot.profiles.player_two.user_id ||
		!parse_uuid(snapshot.profiles.player_one.user_id,
			std::span<std::byte, 16U>(result.data() + 28U, 16U)) ||
		!parse_uuid(snapshot.profiles.player_two.user_id,
			std::span<std::byte, 16U>(result.data() + 44U, 16U)) ||
		!valid_profile(snapshot.profiles.player_one) ||
		!valid_profile(snapshot.profiles.player_two)) {
		return std::unexpected(NewClassicNetplayIdentityError::invalid_profile);
	}
	const auto &player_one = snapshot.profiles.player_one.nametag.source;
	const auto &player_two = snapshot.profiles.player_two.nametag.source;
	result[9U] = std::byte{static_cast<std::uint8_t>(player_one.size())};
	result[10U] = std::byte{static_cast<std::uint8_t>(player_two.size())};
	std::transform(player_one.begin(), player_one.end(), result.begin() + 60U,
		[](char value) { return std::byte{static_cast<std::uint8_t>(value)}; });
	std::transform(player_two.begin(), player_two.end(), result.begin() + 117U,
		[](char value) { return std::byte{static_cast<std::uint8_t>(value)}; });
	const auto digest = app_sha256(std::span(result).first(body_bytes));
	std::transform(digest.begin(), digest.end(), result.begin() + body_bytes,
		[](std::uint8_t value) { return std::byte{value}; });
	return result;
}

std::expected<AppSha256, NewClassicNetplayIdentityError>
new_classic_netplay_identity_digest(
	const NewClassicNetplayIdentitySnapshot &snapshot
) noexcept
{
	const auto encoded = encode_new_classic_netplay_identity(snapshot);
	if(!encoded) {
		return std::unexpected(encoded.error());
	}
	AppSha256 digest{};
	std::transform(encoded->begin() + body_bytes, encoded->end(), digest.begin(),
		[](std::byte value) { return std::to_integer<std::uint8_t>(value); });
	return digest;
}

} // namespace th03::app

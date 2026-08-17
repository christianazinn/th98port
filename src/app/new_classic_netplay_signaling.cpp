#include "th03/app/new_classic_netplay_signaling.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace th03::app {
namespace {

struct JsonValue final {
	std::variant<std::string, std::uint64_t> value{};
};

struct JsonField final {
	std::string key{};
	JsonValue value{};
};

[[nodiscard]] NewClassicNetplayRoomMessageError error(
	NewClassicNetplayRoomMessageErrorCode code
) noexcept
{
	return {.code = code};
}

void skip_space(std::string_view input, std::size_t &at) noexcept
{
	while(at < input.size() &&
		(input[at] == ' ' || input[at] == '\t' || input[at] == '\r' ||
		 input[at] == '\n')) {
		++at;
	}
}

[[nodiscard]] std::optional<std::uint8_t> hex_value(char character) noexcept
{
	if(character >= '0' && character <= '9') {
		return static_cast<std::uint8_t>(character - '0');
	}
	if(character >= 'A' && character <= 'F') {
		return static_cast<std::uint8_t>(character - 'A' + 10);
	}
	if(character >= 'a' && character <= 'f') {
		return static_cast<std::uint8_t>(character - 'a' + 10);
	}
	return std::nullopt;
}

void append_utf8(std::string &output, std::uint16_t codepoint)
{
	if(codepoint <= 0x7FU) {
		output.push_back(static_cast<char>(codepoint));
	} else if(codepoint <= 0x7FFU) {
		output.push_back(static_cast<char>(0xC0U | (codepoint >> 6U)));
		output.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
	} else {
		output.push_back(static_cast<char>(0xE0U | (codepoint >> 12U)));
		output.push_back(static_cast<char>(
			0x80U | ((codepoint >> 6U) & 0x3FU)
		));
		output.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
	}
}

[[nodiscard]] std::optional<std::string> json_string(
	std::string_view input,
	std::size_t &at
)
{
	if(at >= input.size() || input[at++] != '"') {
		return std::nullopt;
	}
	std::string result;
	while(at < input.size()) {
		const auto character = input[at++];
		if(character == '"') {
			return result;
		}
		if(static_cast<unsigned char>(character) < 0x20U) {
			return std::nullopt;
		}
		if(character != '\\') {
			result.push_back(character);
			continue;
		}
		if(at >= input.size()) {
			return std::nullopt;
		}
		switch(input[at++]) {
		case '"': result.push_back('"'); break;
		case '\\': result.push_back('\\'); break;
		case '/': result.push_back('/'); break;
		case 'b': result.push_back('\b'); break;
		case 'f': result.push_back('\f'); break;
		case 'n': result.push_back('\n'); break;
		case 'r': result.push_back('\r'); break;
		case 't': result.push_back('\t'); break;
		case 'u': {
			if(at + 4U > input.size()) {
				return std::nullopt;
			}
			std::uint16_t codepoint = 0U;
			for(std::size_t digit = 0U; digit < 4U; ++digit) {
				const auto value = hex_value(input[at++]);
				if(!value) {
					return std::nullopt;
				}
				codepoint = static_cast<std::uint16_t>(
					(codepoint << 4U) | *value
				);
			}
			if(codepoint >= 0xD800U && codepoint <= 0xDFFFU) {
				return std::nullopt;
			}
			append_utf8(result, codepoint);
			break;
		}
		default: return std::nullopt;
		}
	}
	return std::nullopt;
}

[[nodiscard]] std::optional<std::uint64_t> json_unsigned(
	std::string_view input,
	std::size_t &at
) noexcept
{
	const auto begin = at;
	while(at < input.size() && input[at] >= '0' && input[at] <= '9') {
		++at;
	}
	if(begin == at || (at - begin > 1U && input[begin] == '0')) {
		return std::nullopt;
	}
	std::uint64_t value = 0U;
	const auto parsed = std::from_chars(
		input.data() + begin, input.data() + at, value
	);
	return parsed.ec == std::errc{} && parsed.ptr == input.data() + at ?
		std::optional{value} : std::nullopt;
}

[[nodiscard]] std::optional<std::vector<JsonField>> flat_json(
	std::string_view input
)
{
	std::size_t at = 0U;
	skip_space(input, at);
	if(at >= input.size() || input[at++] != '{') {
		return std::nullopt;
	}
	std::vector<JsonField> result;
	skip_space(input, at);
	if(at < input.size() && input[at] == '}') {
		++at;
	} else {
		while(at < input.size()) {
			auto key = json_string(input, at);
			skip_space(input, at);
			if(!key || at >= input.size() || input[at++] != ':') {
				return std::nullopt;
			}
			skip_space(input, at);
			JsonValue value{};
			if(at < input.size() && input[at] == '"') {
				auto parsed = json_string(input, at);
				if(!parsed) {
					return std::nullopt;
				}
				value.value = std::move(*parsed);
			} else {
				auto parsed = json_unsigned(input, at);
				if(!parsed) {
					return std::nullopt;
				}
				value.value = *parsed;
			}
			if(std::ranges::any_of(result, [&](const JsonField &field) {
				return field.key == *key;
			})) {
				return std::nullopt;
			}
			result.push_back({std::move(*key), std::move(value)});
			skip_space(input, at);
			if(at >= input.size()) {
				return std::nullopt;
			}
			if(input[at++] == '}') {
				break;
			}
			if(input[at - 1U] != ',') {
				return std::nullopt;
			}
			skip_space(input, at);
		}
	}
	skip_space(input, at);
	return at == input.size() ? std::optional{std::move(result)} :
		std::nullopt;
}

[[nodiscard]] const JsonValue *field(
	const std::vector<JsonField> &fields,
	std::string_view key
) noexcept
{
	const auto found = std::ranges::find(fields, key, &JsonField::key);
	return found == fields.end() ? nullptr : &found->value;
}

[[nodiscard]] const std::string *string_value(const JsonValue *value) noexcept
{
	return value ? std::get_if<std::string>(&value->value) : nullptr;
}

[[nodiscard]] const std::uint64_t *unsigned_value(
	const JsonValue *value
) noexcept
{
	return value ? std::get_if<std::uint64_t>(&value->value) : nullptr;
}

[[nodiscard]] bool uuid_shape(std::string_view value) noexcept
{
	if(value.size() != 36U) {
		return false;
	}
	for(std::size_t index = 0U; index < value.size(); ++index) {
		if(index == 8U || index == 13U || index == 18U || index == 23U) {
			if(value[index] != '-') {
				return false;
			}
		} else if(!((value[index] >= '0' && value[index] <= '9') ||
			(value[index] >= 'a' && value[index] <= 'f'))) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] std::optional<NewClassicNetplayRoomSignalType> signal_type(
	std::string_view value
) noexcept
{
	if(value == "offer") return NewClassicNetplayRoomSignalType::offer;
	if(value == "answer") return NewClassicNetplayRoomSignalType::answer;
	if(value == "ice") return NewClassicNetplayRoomSignalType::ice;
	if(value == "control") return NewClassicNetplayRoomSignalType::control;
	return std::nullopt;
}

[[nodiscard]] std::string_view signal_type_text(
	NewClassicNetplayRoomSignalType type
) noexcept
{
	switch(type) {
	case NewClassicNetplayRoomSignalType::offer: return "offer";
	case NewClassicNetplayRoomSignalType::answer: return "answer";
	case NewClassicNetplayRoomSignalType::ice: return "ice";
	case NewClassicNetplayRoomSignalType::control: return "control";
	}
	return {};
}

void append_json_string(std::string &output, std::string_view value)
{
	constexpr std::array<char, 16U> hex{
		'0', '1', '2', '3', '4', '5', '6', '7',
		'8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
	};
	output.push_back('"');
	for(const auto character : value) {
		const auto byte = static_cast<unsigned char>(character);
		switch(character) {
		case '"': output += "\\\""; break;
		case '\\': output += "\\\\"; break;
		case '\b': output += "\\b"; break;
		case '\f': output += "\\f"; break;
		case '\n': output += "\\n"; break;
		case '\r': output += "\\r"; break;
		case '\t': output += "\\t"; break;
		default:
			if(byte < 0x20U) {
				output += "\\u00";
				output.push_back(hex[byte >> 4U]);
				output.push_back(hex[byte & 0x0FU]);
			} else {
				output.push_back(character);
			}
			break;
		}
	}
	output.push_back('"');
}

} // namespace

bool valid_new_classic_netplay_room_code(std::string_view code) noexcept
{
	constexpr std::string_view alphabet = "23456789ABCDEFGHJKLMNPQRSTUVWXYZ";
	return code.size() == 6U && std::ranges::all_of(code, [alphabet](char character) {
		return alphabet.find(character) != std::string_view::npos;
	});
}

std::expected<std::string, NewClassicNetplayRoomMessageError>
encode_new_classic_netplay_room_signal(
	const NewClassicNetplayRoomSignal &signal
)
{
	const auto type = signal_type_text(signal.type);
	if(type.empty()) {
		return std::unexpected(error(
			NewClassicNetplayRoomMessageErrorCode::invalid_type
		));
	}
	if(signal.payload.size() > new_classic_netplay_room_payload_max_bytes) {
		return std::unexpected(error(
			NewClassicNetplayRoomMessageErrorCode::invalid_payload
		));
	}
	std::string result{"{\"protocol\":\"T3ROOM1\",\"type\":"};
	append_json_string(result, type);
	result += ",\"sequence\":" + std::to_string(signal.sequence) +
		",\"payload\":";
	append_json_string(result, signal.payload);
	result.push_back('}');
	if(result.size() > new_classic_netplay_room_signal_max_bytes) {
		return std::unexpected(error(
			NewClassicNetplayRoomMessageErrorCode::invalid_size
		));
	}
	return result;
}

std::expected<NewClassicNetplayRoomMessage, NewClassicNetplayRoomMessageError>
decode_new_classic_netplay_room_message(std::string_view wire)
{
	if(wire.empty() || wire.size() > new_classic_netplay_room_signal_max_bytes) {
		return std::unexpected(error(
			NewClassicNetplayRoomMessageErrorCode::invalid_size
		));
	}
	const auto fields = flat_json(wire);
	if(!fields) {
		return std::unexpected(error(
			NewClassicNetplayRoomMessageErrorCode::invalid_json
		));
	}
	const auto *protocol = string_value(field(*fields, "protocol"));
	const auto *type = string_value(field(*fields, "type"));
	if(protocol == nullptr || *protocol != new_classic_netplay_room_protocol) {
		return std::unexpected(error(
			NewClassicNetplayRoomMessageErrorCode::unsupported_protocol
		));
	}
	if(type == nullptr) {
		return std::unexpected(error(
			NewClassicNetplayRoomMessageErrorCode::invalid_type
		));
	}
	if(*type == "paired") {
		if(fields->size() != 2U) {
			return std::unexpected(error(
				NewClassicNetplayRoomMessageErrorCode::invalid_shape
			));
		}
		return NewClassicNetplayRoomPaired{};
	}
	if(*type == "peer-left") {
		const auto *player = unsigned_value(field(*fields, "player"));
		if(fields->size() != 3U || player == nullptr ||
			(*player != 1U && *player != 2U)) {
			return std::unexpected(error(
				NewClassicNetplayRoomMessageErrorCode::invalid_player
			));
		}
		return NewClassicNetplayRoomPeerLeft{
			.player = static_cast<std::uint8_t>(*player),
		};
	}
	if(*type == "welcome") {
		const auto *code = string_value(field(*fields, "code"));
		const auto *room_id = string_value(field(*fields, "roomId"));
		const auto *player = unsigned_value(field(*fields, "player"));
		const auto *expires = unsigned_value(field(*fields, "expiresAtUnixMs"));
		if(fields->size() != 6U || code == nullptr || room_id == nullptr ||
			!valid_new_classic_netplay_room_code(*code) ||
			!uuid_shape(*room_id)) {
			return std::unexpected(error(
				NewClassicNetplayRoomMessageErrorCode::invalid_room
			));
		}
		if(player == nullptr || (*player != 1U && *player != 2U)) {
			return std::unexpected(error(
				NewClassicNetplayRoomMessageErrorCode::invalid_player
			));
		}
		if(expires == nullptr || *expires == 0U) {
			return std::unexpected(error(
				NewClassicNetplayRoomMessageErrorCode::invalid_expiration
			));
		}
		return NewClassicNetplayRoomWelcome{
			.code = *code,
			.room_id = *room_id,
			.player = static_cast<std::uint8_t>(*player),
			.expires_at_unix_ms = *expires,
		};
	}
	const auto parsed_type = signal_type(*type);
	if(!parsed_type) {
		return std::unexpected(error(
			NewClassicNetplayRoomMessageErrorCode::invalid_type
		));
	}
	const auto *sequence = unsigned_value(field(*fields, "sequence"));
	const auto *payload = string_value(field(*fields, "payload"));
	if(fields->size() != 4U) {
		return std::unexpected(error(
			NewClassicNetplayRoomMessageErrorCode::invalid_shape
		));
	}
	if(sequence == nullptr || *sequence >
		std::numeric_limits<std::uint32_t>::max()) {
		return std::unexpected(error(
			NewClassicNetplayRoomMessageErrorCode::invalid_sequence
		));
	}
	if(payload == nullptr || payload->size() >
		new_classic_netplay_room_payload_max_bytes) {
		return std::unexpected(error(
			NewClassicNetplayRoomMessageErrorCode::invalid_payload
		));
	}
	return NewClassicNetplayRoomSignal{
		.type = *parsed_type,
		.sequence = static_cast<std::uint32_t>(*sequence),
		.payload = *payload,
	};
}

} // namespace th03::app

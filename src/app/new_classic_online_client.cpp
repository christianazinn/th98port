#include "th03/app/new_classic_online_client.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "th03/app/main_compiled_content.hpp"
#include "th03/app/new_classic_nametag.hpp"
#include "th03/formats/v14.hpp"

namespace th03::app {
namespace {

constexpr std::string_view cache_content_type =
	"application/vnd.th03.online-cache";
constexpr std::string_view replay_content_type =
	"application/vnd.th03.v14-replay";
constexpr std::string_view json_content_type = "application/json";
constexpr std::size_t json_response_limit = 16U * 1024U;

[[nodiscard]] NewClassicOnlineClientError client_error(
	NewClassicOnlineClientErrorCode code,
	std::uint16_t status = 0U
) noexcept
{
	NewClassicOnlineClientError result{};
	result.code = code;
	result.http_status = status;
	return result;
}

[[nodiscard]] bool story_category(
	const NewClassicOnlineCategory &category
) noexcept
{
	return category.mode == NewClassicOnlineMode::story &&
		category.rank < 4U &&
		category.character <= new_classic_online_overall_character &&
		category.start_stage == 0U && category.start_round == 0U;
}

[[nodiscard]] std::string category_key(
	const NewClassicOnlineCategory &category
)
{
	return "story:r" + std::to_string(category.rank) +
		(category.character == new_classic_online_overall_character ?
			":overall" : ":c" + std::to_string(category.character));
}

[[nodiscard]] char hex_digit(std::uint8_t nibble) noexcept
{
	return static_cast<char>(nibble < 10U ? ('0' + nibble) : ('A' + nibble - 10U));
}

template<typename Tag>
[[nodiscard]] std::string identity_hex(
	const NewClassicOnlineIdentity<Tag> &identity
)
{
	std::string result;
	result.reserve(identity.bytes.size() * 2U);
	for(const auto byte : identity.bytes) {
		result.push_back(hex_digit(static_cast<std::uint8_t>(byte >> 4U)));
		result.push_back(hex_digit(static_cast<std::uint8_t>(byte & 0x0FU)));
	}
	return result;
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

[[nodiscard]] bool token_shape(std::string_view value) noexcept
{
	return value.size() >= 32U && value.size() <= 128U &&
		std::ranges::all_of(value, [](char character) {
			return (character >= 'A' && character <= 'Z') ||
				(character >= 'a' && character <= 'z') ||
				(character >= '0' && character <= '9') ||
				character == '-' || character == '_';
		});
}

[[nodiscard]] bool credentials_valid(
	const NewClassicOnlineCredentials &credentials
) noexcept
{
	return uuid_shape(credentials.user_id) && token_shape(credentials.token) &&
		(!credentials.cached_nametag ||
		 layout_new_classic_nametag(*credentials.cached_nametag).has_value());
}

struct JsonField final {
	std::string key{};
	std::string value{};
};

enum class JsonScalarKind : std::uint8_t {
	string,
	null,
	other,
};

struct JsonScalarField final {
	std::string key{};
	JsonScalarKind kind = JsonScalarKind::other;
	std::string value{};
};

void skip_space(std::string_view input, std::size_t &at) noexcept
{
	while(at < input.size() && std::isspace(
		static_cast<unsigned char>(input[at])
	) != 0) {
		++at;
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
		if(character == '\\') {
			if(at >= input.size() || (input[at] != '"' &&
				input[at] != '\\' && input[at] != '/')) {
				return std::nullopt;
			}
			result.push_back(input[at++]);
			continue;
		}
		result.push_back(character);
	}
	return std::nullopt;
}

[[nodiscard]] std::optional<std::vector<JsonField>> flat_json(
	std::span<const std::byte> bytes
)
{
	const std::string_view input{
		reinterpret_cast<const char *>(bytes.data()), bytes.size()
	};
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
			auto value = json_string(input, at);
			if(!value || std::ranges::any_of(result, [&](const JsonField &field) {
				return field.key == *key;
			})) {
				return std::nullopt;
			}
			result.push_back({std::move(*key), std::move(*value)});
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
	return at == input.size() ? std::optional{std::move(result)} : std::nullopt;
}

[[nodiscard]] const std::string *field(
	const std::vector<JsonField> &fields,
	std::string_view key
) noexcept
{
	const auto found = std::ranges::find(fields, key, &JsonField::key);
	return found == fields.end() ? nullptr : &found->value;
}

[[nodiscard]] bool skip_json_value(
	std::string_view input,
	std::size_t &at,
	std::uint8_t depth = 0U
);

[[nodiscard]] bool skip_json_sequence(
	std::string_view input,
	std::size_t &at,
	char close,
	std::uint8_t depth
)
{
	if(depth > 8U) {
		return false;
	}
	skip_space(input, at);
	if(at < input.size() && input[at] == close) {
		++at;
		return true;
	}
	for(;;) {
		if(close == '}' && !json_string(input, at)) {
			return false;
		}
		if(close == '}') {
			skip_space(input, at);
			if(at >= input.size() || input[at++] != ':') {
				return false;
			}
		}
		if(!skip_json_value(input, at, static_cast<std::uint8_t>(depth + 1U))) {
			return false;
		}
		skip_space(input, at);
		if(at >= input.size()) {
			return false;
		}
		if(input[at++] == close) {
			return true;
		}
		if(input[at - 1U] != ',') {
			return false;
		}
		skip_space(input, at);
	}
}

[[nodiscard]] bool skip_json_value(
	std::string_view input,
	std::size_t &at,
	std::uint8_t depth
)
{
	skip_space(input, at);
	if(at >= input.size()) {
		return false;
	}
	if(input[at] == '"') {
		return json_string(input, at).has_value();
	}
	if(input[at] == '[' || input[at] == '{') {
		const auto close = input[at++] == '[' ? ']' : '}';
		return skip_json_sequence(input, at, close, depth);
	}
	for(const auto literal : {std::string_view{"true"},
		std::string_view{"false"}, std::string_view{"null"}}) {
		if(input.substr(at, literal.size()) == literal) {
			at += literal.size();
			return true;
		}
	}
	const auto begin = at;
	if(input[at] == '-') {
		++at;
	}
	while(at < input.size() && input[at] >= '0' && input[at] <= '9') {
		++at;
	}
	if(at < input.size() && input[at] == '.') {
		++at;
		while(at < input.size() && input[at] >= '0' && input[at] <= '9') {
			++at;
		}
	}
	return at != begin && !(at == begin + 1U && input[begin] == '-');
}

[[nodiscard]] std::optional<std::vector<JsonScalarField>> scalar_json(
	std::span<const std::byte> bytes
)
{
	const std::string_view input{
		reinterpret_cast<const char *>(bytes.data()), bytes.size()
	};
	std::size_t at = 0U;
	skip_space(input, at);
	if(at >= input.size() || input[at++] != '{') {
		return std::nullopt;
	}
	std::vector<JsonScalarField> result;
	skip_space(input, at);
	if(at < input.size() && input[at] == '}') {
		++at;
	} else {
		for(;;) {
			auto key = json_string(input, at);
			skip_space(input, at);
			if(!key || at >= input.size() || input[at++] != ':' ||
				std::ranges::any_of(result, [&](const JsonScalarField &entry) {
					return entry.key == *key;
				})) {
				return std::nullopt;
			}
			skip_space(input, at);
			JsonScalarField entry{.key = std::move(*key)};
			if(at < input.size() && input[at] == '"') {
				auto value = json_string(input, at);
				if(!value) {
					return std::nullopt;
				}
				entry.kind = JsonScalarKind::string;
				entry.value = std::move(*value);
			} else if(input.substr(at, 4U) == "null") {
				at += 4U;
				entry.kind = JsonScalarKind::null;
			} else if(!skip_json_value(input, at)) {
				return std::nullopt;
			}
			result.push_back(std::move(entry));
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
	return at == input.size() ? std::optional{std::move(result)} : std::nullopt;
}

[[nodiscard]] const JsonScalarField *scalar_field(
	const std::vector<JsonScalarField> &fields,
	std::string_view key
) noexcept
{
	const auto found = std::ranges::find(fields, key, &JsonScalarField::key);
	return found == fields.end() ? nullptr : &*found;
}

[[nodiscard]] std::vector<NewClassicOnlineHttpHeader> auth_headers(
	const NewClassicOnlineCredentials &credentials,
	bool json_body = false
)
{
	std::vector<NewClassicOnlineHttpHeader> result{
		{"authorization", "Bearer " + credentials.token},
		{"x-th03-user-id", credentials.user_id},
	};
	if(json_body) {
		result.push_back({"content-type", std::string(json_content_type)});
	}
	return result;
}

[[nodiscard]] std::string json_string_literal(std::string_view value)
{
	std::string result{"\""};
	result.reserve(value.size() + 2U);
	for(const auto character : value) {
		if(character == '\\' || character == '"') {
			result.push_back('\\');
		}
		result.push_back(character);
	}
	result.push_back('"');
	return result;
}

[[nodiscard]] std::expected<
	NewClassicOnlineHttpResponse,
	NewClassicOnlineClientError
> perform(
	const NewClassicOnlineTransport &transport,
	const NewClassicOnlineHttpRequest &request
)
{
	if(transport.perform == nullptr || request.maximum_response_bytes == 0U) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::transport
		));
	}
	auto response = transport.perform(transport.context, request);
	if(!response) {
		auto result = client_error(NewClassicOnlineClientErrorCode::transport);
		result.transport = response.error();
		return std::unexpected(std::move(result));
	}
	if(response->body.size() > request.maximum_response_bytes) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::response_limit,
			response->status
		));
	}
	return std::move(*response);
}

[[nodiscard]] std::uint64_t packed_score(
	const std::array<std::uint8_t, 4U> &packed
) noexcept
{
	std::uint64_t value = 0U;
	for(auto byte = packed.rbegin(); byte != packed.rend(); ++byte) {
		value = (value * 10U) + ((*byte >> 4U) & 0x0FU);
		value = (value * 10U) + (*byte & 0x0FU);
	}
	return value * 10U;
}

[[nodiscard]] std::string replay_name(
	const std::array<char, 8U> &name
)
{
	std::size_t length = name.size();
	while(length != 0U && (name[length - 1U] == ' ' || name[length - 1U] == '\0')) {
		--length;
	}
	return std::string(name.data(), length);
}

[[nodiscard]] bool replay_metadata_matches(
	const formats::ReplayDocument &document,
	const NewClassicOnlineReplayRecord &record
) noexcept
{
	if(!document.metadata || !document.slowdown || document.practice) {
		return false;
	}
	const auto &metadata = *document.metadata;
	const auto player = metadata.paletted_character[0U];
	if(metadata.status != formats::ReplayStatus::finalized ||
		metadata.game_mode != game_mode::story || player < 1 || player > 18 ||
		metadata.rank != record.category.rank ||
		static_cast<std::uint8_t>((player - 1) / 2) != record.category.character ||
		metadata.dos_date != record.dos_date || metadata.autofire != record.autofire ||
		document.session.init.autofire_mask != record.autofire ||
		document.slowdown->timed_frames != record.timed_frames ||
		document.slowdown->slow_frames != record.slow_frames ||
		packed_score(metadata.final_score) != record.result.score ||
		replay_name(metadata.name) != record.title ||
		record.result.continues_used != 0U || !metadata.final_route ||
		*metadata.final_route != 2U || !metadata.final_story_stage ||
		*metadata.final_story_stage != record.result.end_stage ||
		!metadata.final_winner) {
		return false;
	}
	if(record.result.kind == NewClassicOnlineResultKind::clear) {
		return metadata.end_reason == formats::ReplayEndReason::complete &&
			record.result.end_stage == 8U && *metadata.final_winner == 0;
	}
	return record.result.kind == NewClassicOnlineResultKind::game_over &&
		metadata.end_reason == formats::ReplayEndReason::game_over &&
		*metadata.final_winner == 1;
}

[[nodiscard]] std::expected<formats::ReplayDocument,
	NewClassicOnlineClientError>
validate_replay_payload(
	std::span<const std::byte> bytes,
	const NewClassicOnlineReplayRecord &record,
	std::uint16_t status = 0U
)
{
	if(bytes.size() != record.payload_bytes) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::replay_metadata, status
		));
	}
	const auto digest = app_sha256(bytes);
	if(!std::ranges::equal(digest, record.replay_id.bytes)) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::replay_digest, status
		));
	}
	auto parsed = formats::parse_v14_replay(bytes);
	if(!parsed) {
		auto result = client_error(
			NewClassicOnlineClientErrorCode::replay_format, status
		);
		result.replay_format = parsed.error();
		return std::unexpected(std::move(result));
	}
	if(!replay_metadata_matches(*parsed, record)) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::replay_metadata, status
		));
	}
	return std::move(*parsed);
}

[[nodiscard]] NewClassicOnlineClientError replay_file_error(
	ReplaySlotStoreErrorCode code,
	std::error_code system = {}
)
{
	auto result = client_error(NewClassicOnlineClientErrorCode::replay_store);
	result.replay_store = ReplaySlotStoreError{
		.code = code,
		.system = system,
	};
	return result;
}

[[nodiscard]] std::expected<std::optional<std::vector<std::byte>>,
	NewClassicOnlineClientError>
read_local_replay_payload(
	const NewClassicOnlineReplayRecord &record,
	const std::filesystem::path &replay_root
)
{
	if(replay_root.empty() || record.payload_bytes == 0U ||
		record.payload_bytes > new_classic_online_replay_max_bytes) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::invalid_document
		));
	}
	const auto path = replay_root / "ONLINE" /
		new_classic_online_replay_filename(record.replay_id);
	std::error_code system{};
	const auto status = std::filesystem::status(path, system);
	if(system == std::errc::no_such_file_or_directory) {
		return std::optional<std::vector<std::byte>>{};
	}
	if(system) {
		return std::unexpected(replay_file_error(
			ReplaySlotStoreErrorCode::source_status, system
		));
	}
	if(!std::filesystem::exists(status)) {
		return std::optional<std::vector<std::byte>>{};
	}
	if(!std::filesystem::is_regular_file(status)) {
		return std::unexpected(replay_file_error(
			ReplaySlotStoreErrorCode::source_not_regular
		));
	}
	const auto size = std::filesystem::file_size(path, system);
	if(system || size != record.payload_bytes ||
		size > new_classic_online_replay_max_bytes ||
		size > static_cast<std::uintmax_t>(
			std::numeric_limits<std::streamsize>::max()
		)) {
		return std::unexpected(replay_file_error(
			ReplaySlotStoreErrorCode::source_size, system
		));
	}
	std::ifstream input(path, std::ios::binary);
	if(!input) {
		return std::unexpected(replay_file_error(
			ReplaySlotStoreErrorCode::source_open
		));
	}
	std::vector<std::byte> bytes(static_cast<std::size_t>(size));
	input.read(
		reinterpret_cast<char *>(bytes.data()),
		static_cast<std::streamsize>(bytes.size())
	);
	if(!input || input.peek() != std::ifstream::traits_type::eof()) {
		return std::unexpected(replay_file_error(
			ReplaySlotStoreErrorCode::source_read
		));
	}
	return std::optional<std::vector<std::byte>>(std::move(bytes));
}

} // namespace

bool new_classic_online_credentials_rejected(
	const NewClassicOnlineClientError &error
) noexcept
{
	return error.code == NewClassicOnlineClientErrorCode::http_status &&
		error.http_status == 401U;
}

std::filesystem::path new_classic_online_board_cache_path(
	const std::filesystem::path &cache_root,
	const NewClassicOnlineCategory &category
)
{
	if(!story_category(category) || cache_root.empty()) {
		return {};
	}
	const auto character = category.character ==
		new_classic_online_overall_character ? std::string{"OVERALL"} :
		std::to_string(category.character);
	return cache_root / ("STORY-R" + std::to_string(category.rank) + "-C" +
		character + ".T3ONL");
}

bool validate_new_classic_online_credentials(
	const NewClassicOnlineCredentials &credentials
) noexcept
{
	return credentials_valid(credentials);
}

std::expected<NewClassicOnlineCacheDocument, NewClassicOnlineClientError>
refresh_new_classic_online_board(
	const NewClassicOnlineCategory &category,
	const std::filesystem::path &cache_root,
	const NewClassicOnlineTransport &transport
)
{
	const auto path = new_classic_online_board_cache_path(cache_root, category);
	if(path.empty()) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::invalid_category
		));
	}
	const std::string target = "/api/th03/leaderboard?category=" +
		category_key(category) + "&format=t3onl2";
	auto response = perform(transport, {
		.method = NewClassicOnlineHttpMethod::get,
		.target = target,
		.maximum_response_bytes = new_classic_online_cache_max_bytes,
	});
	if(!response) {
		return std::unexpected(std::move(response.error()));
	}
	if(response->status != 200U) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::http_status, response->status
		));
	}
	if(response->content_type != cache_content_type) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::content_type, response->status
		));
	}
	auto decoded = decode_new_classic_online_cache(response->body);
	if(!decoded) {
		auto result = client_error(NewClassicOnlineClientErrorCode::cache_codec);
		result.cache_codec = decoded.error();
		return std::unexpected(std::move(result));
	}
	if(decoded->generation != 0U || decoded->board_category != category) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::cache_mismatch
		));
	}
	auto existing = load_new_classic_online_store(path);
	if(existing) {
		decoded->generation = existing->document.generation;
	} else if(existing.error().code !=
		NewClassicOnlineStoreErrorCode::no_valid_candidate) {
		auto result = client_error(NewClassicOnlineClientErrorCode::cache_store);
		result.cache_store = existing.error();
		return std::unexpected(std::move(result));
	}
	if(auto committed = commit_new_classic_online_store(path, *decoded); !committed) {
		auto result = client_error(NewClassicOnlineClientErrorCode::cache_store);
		result.cache_store = committed.error();
		return std::unexpected(std::move(result));
	}
	return std::move(*decoded);
}

std::expected<NewClassicOnlineCacheDocument, NewClassicOnlineClientError>
fetch_new_classic_online_records(
	const NewClassicOnlineCategory &category,
	const NewClassicOnlineCredentials &credentials,
	const NewClassicOnlineTransport &transport
)
{
	if(!story_category(category)) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::invalid_category
		));
	}
	if(!credentials_valid(credentials)) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::invalid_credentials
		));
	}
	const std::string target = "/api/th03/users/" + credentials.user_id +
		"/records?category=" + category_key(category) + "&format=t3onl2";
	auto response = perform(transport, {
		.method = NewClassicOnlineHttpMethod::get,
		.target = target,
		.headers = {
			{"authorization", "Bearer " + credentials.token},
			{"x-th03-user-id", credentials.user_id},
		},
		.maximum_response_bytes = new_classic_online_cache_max_bytes,
	});
	if(!response) {
		return std::unexpected(std::move(response.error()));
	}
	if(response->status != 200U) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::http_status, response->status
		));
	}
	if(response->content_type != cache_content_type) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::content_type, response->status
		));
	}
	auto decoded = decode_new_classic_online_cache(response->body);
	if(!decoded) {
		auto result = client_error(NewClassicOnlineClientErrorCode::cache_codec);
		result.cache_codec = decoded.error();
		return std::unexpected(std::move(result));
	}
	if(decoded->generation != 0U || decoded->board_category != category) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::cache_mismatch
		));
	}
	return std::move(*decoded);
}

std::expected<NewClassicOnlineCredentials, NewClassicOnlineClientError>
enroll_new_classic_online_user(const NewClassicOnlineTransport &transport)
{
	auto response = perform(transport, {
		.method = NewClassicOnlineHttpMethod::post,
		.target = "/api/th03/users",
		.headers = {{"content-type", "application/octet-stream"}},
		.maximum_response_bytes = json_response_limit,
	});
	if(!response) {
		return std::unexpected(std::move(response.error()));
	}
	if(response->status != 201U) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::http_status, response->status
		));
	}
	if(response->content_type != json_content_type) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::content_type, response->status
		));
	}
	auto fields = flat_json(response->body);
	if(!fields || fields->size() != 2U || field(*fields, "userId") == nullptr ||
		field(*fields, "token") == nullptr) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::response_shape, response->status
		));
	}
	NewClassicOnlineCredentials credentials{
		*field(*fields, "userId"), *field(*fields, "token")
	};
	if(!credentials_valid(credentials)) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::invalid_credentials, response->status
		));
	}
	return credentials;
}

std::expected<NewClassicOnlineProfile, NewClassicOnlineClientError>
fetch_new_classic_online_profile(
	const NewClassicOnlineCredentials &credentials,
	const NewClassicOnlineTransport &transport
)
{
	if(!credentials_valid(credentials)) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::invalid_credentials
		));
	}
	auto response = perform(transport, {
		.method = NewClassicOnlineHttpMethod::get,
		.target = "/api/th03/users/" + credentials.user_id + "/profile",
		.headers = auth_headers(credentials),
		.maximum_response_bytes = json_response_limit,
	});
	if(!response) {
		return std::unexpected(std::move(response.error()));
	}
	if(response->status != 200U) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::http_status, response->status
		));
	}
	if(response->content_type != json_content_type) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::content_type, response->status
		));
	}
	auto fields = scalar_json(response->body);
	const auto *status = fields ? scalar_field(*fields, "status") : nullptr;
	const auto *user_id = fields ? scalar_field(*fields, "userId") : nullptr;
	const auto *nametag = fields ? scalar_field(*fields, "nametag") : nullptr;
	if(!fields || fields->size() != 5U || status == nullptr ||
		status->kind != JsonScalarKind::string || status->value != "accepted" ||
		user_id == nullptr || user_id->kind != JsonScalarKind::string ||
		user_id->value != credentials.user_id || nametag == nullptr ||
		(nametag->kind != JsonScalarKind::string &&
		 nametag->kind != JsonScalarKind::null)) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::response_shape, response->status
		));
	}
	NewClassicOnlineProfile result{.user_id = user_id->value};
	if(nametag->kind == JsonScalarKind::string) {
		if(!layout_new_classic_nametag(nametag->value)) {
			return std::unexpected(client_error(
				NewClassicOnlineClientErrorCode::response_shape, response->status
			));
		}
		result.nametag = nametag->value;
	}
	return result;
}

std::expected<NewClassicOnlineProfile, NewClassicOnlineClientError>
update_new_classic_online_profile(
	std::string_view nametag,
	const NewClassicOnlineCredentials &credentials,
	const NewClassicOnlineTransport &transport
)
{
	if(!credentials_valid(credentials)) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::invalid_credentials
		));
	}
	const auto layout = layout_new_classic_nametag(nametag);
	if(!layout) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::invalid_document
		));
	}
	const auto body = std::string{"{\"nametag\":"} +
		json_string_literal(layout->source) + "}";
	auto response = perform(transport, {
		.method = NewClassicOnlineHttpMethod::put,
		.target = "/api/th03/users/" + credentials.user_id + "/profile",
		.headers = auth_headers(credentials, true),
		.body = std::as_bytes(std::span{body}),
		.maximum_response_bytes = json_response_limit,
	});
	if(!response) {
		return std::unexpected(std::move(response.error()));
	}
	if(response->status != 200U) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::http_status, response->status
		));
	}
	if(response->content_type != json_content_type) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::content_type, response->status
		));
	}
	auto fields = scalar_json(response->body);
	const auto *status = fields ? scalar_field(*fields, "status") : nullptr;
	const auto *user_id = fields ? scalar_field(*fields, "userId") : nullptr;
	const auto *updated = fields ? scalar_field(*fields, "nametag") : nullptr;
	if(!fields || fields->size() != 5U || status == nullptr ||
		status->kind != JsonScalarKind::string || status->value != "accepted" ||
		user_id == nullptr || user_id->kind != JsonScalarKind::string ||
		user_id->value != credentials.user_id || updated == nullptr ||
		updated->kind != JsonScalarKind::string || updated->value != layout->source) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::response_shape, response->status
		));
	}
	return NewClassicOnlineProfile{
		.user_id = user_id->value,
		.nametag = updated->value,
	};
}

std::expected<NewClassicOnlineLinkCode, NewClassicOnlineClientError>
create_new_classic_online_link_code(
	const NewClassicOnlineCredentials &credentials,
	const NewClassicOnlineTransport &transport
)
{
	if(!credentials_valid(credentials)) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::invalid_credentials
		));
	}
	auto response = perform(transport, {
		.method = NewClassicOnlineHttpMethod::post,
		.target = "/api/th03/users/" + credentials.user_id + "/link-codes",
		.headers = auth_headers(credentials),
		.maximum_response_bytes = json_response_limit,
	});
	if(!response) {
		return std::unexpected(std::move(response.error()));
	}
	if(response->status != 201U) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::http_status, response->status
		));
	}
	if(response->content_type != json_content_type) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::content_type, response->status
		));
	}
	auto fields = scalar_json(response->body);
	const auto *status = fields ? scalar_field(*fields, "status") : nullptr;
	const auto *code = fields ? scalar_field(*fields, "code") : nullptr;
	if(!fields || fields->size() != 3U || status == nullptr ||
		status->kind != JsonScalarKind::string || status->value != "accepted" ||
		code == nullptr || code->kind != JsonScalarKind::string ||
		code->value.size() != 10U || !std::ranges::all_of(code->value,
			[](const char character) {
				return (character >= '2' && character <= '9') ||
					(character >= 'A' && character <= 'H') ||
					(character >= 'J' && character <= 'N') ||
					(character >= 'P' && character <= 'Z');
			})) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::response_shape, response->status
		));
	}
	return NewClassicOnlineLinkCode{.code = code->value};
}

std::expected<NewClassicOnlineCredentials, NewClassicOnlineClientError>
redeem_new_classic_online_link_code(
	std::string_view code,
	const NewClassicOnlineTransport &transport
)
{
	if(code.size() != 10U) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::invalid_document
		));
	}
	const auto body = std::string{"{\"code\":"} +
		json_string_literal(code) + "}";
	auto response = perform(transport, {
		.method = NewClassicOnlineHttpMethod::post,
		.target = "/api/th03/users/link",
		.headers = {{"content-type", std::string(json_content_type)}},
		.body = std::as_bytes(std::span{body}),
		.maximum_response_bytes = json_response_limit,
	});
	if(!response) {
		return std::unexpected(std::move(response.error()));
	}
	if(response->status != 201U) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::http_status, response->status
		));
	}
	if(response->content_type != json_content_type) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::content_type, response->status
		));
	}
	auto fields = flat_json(response->body);
	if(!fields || fields->size() != 3U || field(*fields, "userId") == nullptr ||
		field(*fields, "token") == nullptr ||
		field(*fields, "credentialId") == nullptr) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::response_shape, response->status
		));
	}
	NewClassicOnlineCredentials result{
		.user_id = *field(*fields, "userId"),
		.token = *field(*fields, "token"),
	};
	if(!credentials_valid(result) || !uuid_shape(*field(*fields, "credentialId"))) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::response_shape, response->status
		));
	}
	return result;
}

std::expected<NewClassicOnlineUploadResult, NewClassicOnlineClientError>
upload_new_classic_online_replay(
	const formats::ReplayDocument &document,
	const NewClassicOnlineCredentials &credentials,
	const NewClassicOnlineTransport &transport
)
{
	if(!credentials_valid(credentials)) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::invalid_credentials
		));
	}
	if(!document.metadata || !document.slowdown || document.practice ||
		document.metadata->status != formats::ReplayStatus::finalized ||
		document.metadata->game_mode != game_mode::story ||
		document.slowdown->timed_frames == 0U ||
		document.slowdown->slow_frames > document.slowdown->timed_frames ||
		(static_cast<std::uint64_t>(document.slowdown->slow_frames) * 100U) >
			(static_cast<std::uint64_t>(document.slowdown->timed_frames) * 5U)) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::invalid_document
		));
	}
	auto bytes = formats::write_v14_replay(document);
	if(!bytes || bytes->empty() ||
		bytes->size() > new_classic_online_replay_max_bytes) {
		auto result = client_error(NewClassicOnlineClientErrorCode::invalid_document);
		if(!bytes) {
			result.replay_format = bytes.error();
		}
		return std::unexpected(std::move(result));
	}
	auto response = perform(transport, {
		.method = NewClassicOnlineHttpMethod::post,
		.target = "/api/th03/replays",
		.headers = {
			{"content-type", std::string(replay_content_type)},
			{"x-th03-user-id", credentials.user_id},
			{"authorization", "Bearer " + credentials.token},
		},
		.body = *bytes,
		.maximum_response_bytes = json_response_limit,
	});
	if(!response) {
		return std::unexpected(std::move(response.error()));
	}
	if(response->content_type != json_content_type) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::content_type, response->status
		));
	}
	auto fields = flat_json(response->body);
	const auto *status = fields ? field(*fields, "status") : nullptr;
	if(status != nullptr && *status == "rejected" &&
		(response->status == 401U || response->status == 409U ||
			response->status == 422U)) {
		const auto *reason = field(*fields, "reason");
		if(reason == nullptr || fields->size() != 2U) {
			return std::unexpected(client_error(
				NewClassicOnlineClientErrorCode::response_shape, response->status
			));
		}
		return NewClassicOnlineUploadResult{
			.status = NewClassicOnlineUploadStatus::rejected,
			.reason = *reason,
		};
	}
	if(response->status != 201U) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::http_status, response->status
		));
	}
	const auto *replay_id = fields ? field(*fields, "replayId") : nullptr;
	const auto *entry_id = fields ? field(*fields, "entryId") : nullptr;
	NewClassicOnlineReplayId local_replay_id{};
	local_replay_id.bytes = app_sha256(*bytes);
	if(status == nullptr || *status != "accepted" || replay_id == nullptr ||
		entry_id == nullptr || fields->size() != 3U || replay_id->size() != 64U ||
		!std::ranges::all_of(*replay_id, [](char character) {
			return (character >= '0' && character <= '9') ||
				(character >= 'A' && character <= 'F');
		}) || *replay_id != identity_hex(local_replay_id) ||
		!uuid_shape(*entry_id)) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::response_shape, response->status
		));
	}
	return NewClassicOnlineUploadResult{
		.status = NewClassicOnlineUploadStatus::accepted,
		.replay_id = *replay_id,
		.entry_id = *entry_id,
	};
}

std::expected<std::filesystem::path, NewClassicOnlineClientError>
download_new_classic_online_replay(
	const NewClassicOnlineReplayRecord &record,
	const std::filesystem::path &replay_root,
	const NewClassicOnlineTransport &transport
)
{
	if(replay_root.empty() || record.payload_bytes == 0U ||
		record.payload_bytes > new_classic_online_replay_max_bytes) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::invalid_document
		));
	}
	const auto replay_id = identity_hex(record.replay_id);
	auto response = perform(transport, {
		.method = NewClassicOnlineHttpMethod::get,
		.target = "/api/th03/replays/" + replay_id,
		.maximum_response_bytes = new_classic_online_replay_max_bytes,
	});
	if(!response) {
		return std::unexpected(std::move(response.error()));
	}
	if(response->status != 200U) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::http_status, response->status
		));
	}
	if(response->content_type != replay_content_type) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::content_type, response->status
		));
	}
	if(auto validated = validate_replay_payload(
		response->body, record, response->status
	); !validated) {
		return std::unexpected(std::move(validated.error()));
	}
	const auto path = replay_root / "ONLINE" /
		new_classic_online_replay_filename(record.replay_id);
	if(auto committed = commit_exact_replay_file(path, response->body); !committed) {
		auto result = client_error(NewClassicOnlineClientErrorCode::replay_store);
		result.replay_store = committed.error();
		return std::unexpected(std::move(result));
	}
	return path;
}

std::expected<bool, NewClassicOnlineClientError>
probe_new_classic_online_replay_download(
	const NewClassicOnlineReplayRecord &record,
	const std::filesystem::path &replay_root
)
{
	auto bytes = read_local_replay_payload(record, replay_root);
	if(!bytes) {
		return std::unexpected(std::move(bytes.error()));
	}
	if(!*bytes) {
		return false;
	}
	if(auto validated = validate_replay_payload(**bytes, record); !validated) {
		return std::unexpected(std::move(validated.error()));
	}
	return true;
}

std::expected<formats::ReplayDocument, NewClassicOnlineClientError>
load_new_classic_online_replay(
	const NewClassicOnlineReplayRecord &record,
	const std::filesystem::path &replay_root
)
{
	auto bytes = read_local_replay_payload(record, replay_root);
	if(!bytes) {
		return std::unexpected(std::move(bytes.error()));
	}
	if(!*bytes) {
		return std::unexpected(replay_file_error(
			ReplaySlotStoreErrorCode::source_status,
			std::make_error_code(std::errc::no_such_file_or_directory)
		));
	}
	return validate_replay_payload(**bytes, record);
}

} // namespace th03::app

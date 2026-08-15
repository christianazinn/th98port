#include "th03/app/new_classic_online_client.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "th03/app/main_compiled_content.hpp"
#include "th03/formats/v13.hpp"

namespace th03::app {
namespace {

constexpr std::string_view cache_content_type =
	"application/vnd.th03.online-cache";
constexpr std::string_view replay_content_type =
	"application/vnd.th03.v13-replay";
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
		category.rank < 4U && category.character < 9U &&
		category.start_stage == 0U && category.start_round == 0U;
}

[[nodiscard]] std::string category_key(
	const NewClassicOnlineCategory &category
)
{
	return "story:r" + std::to_string(category.rank) + ":c" +
		std::to_string(category.character);
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
	return uuid_shape(credentials.user_id) && token_shape(credentials.token);
}

struct JsonField final {
	std::string key{};
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
		if(static_cast<unsigned char>(character) < 0x20U || character == '\\') {
			// All current protocol values use an escape-free ASCII vocabulary.
			return std::nullopt;
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

} // namespace

std::filesystem::path new_classic_online_board_cache_path(
	const std::filesystem::path &cache_root,
	const NewClassicOnlineCategory &category
)
{
	if(!story_category(category) || cache_root.empty()) {
		return {};
	}
	return cache_root / (
		"STORY-R" + std::to_string(category.rank) + "-C" +
		std::to_string(category.character) + ".T3ONL"
	);
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
	auto bytes = formats::write_v13_replay(document);
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
		(response->status == 401U || response->status == 422U)) {
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
	if(response->body.size() != record.payload_bytes) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::replay_metadata, response->status
		));
	}
	const auto digest = app_sha256(response->body);
	if(!std::ranges::equal(digest, record.replay_id.bytes)) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::replay_digest, response->status
		));
	}
	auto parsed = formats::parse_v13_replay(response->body);
	if(!parsed) {
		auto result = client_error(
			NewClassicOnlineClientErrorCode::replay_format, response->status
		);
		result.replay_format = parsed.error();
		return std::unexpected(std::move(result));
	}
	if(!replay_metadata_matches(*parsed, record)) {
		return std::unexpected(client_error(
			NewClassicOnlineClientErrorCode::replay_metadata, response->status
		));
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

} // namespace th03::app

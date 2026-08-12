#include "th03/app/new_classic_score_store.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <fstream>
#include <limits>
#include <optional>
#include <span>
#include <system_error>
#include <utility>

namespace th03::app {
namespace {

[[nodiscard]] NewClassicScoreStoreError store_error(
	NewClassicScoreStoreErrorCode code,
	std::error_code system = {},
	std::optional<NewClassicScoreError> codec = std::nullopt,
	std::optional<ClassicScoreError> legacy_codec = std::nullopt
) noexcept
{
	return {code, system, std::move(codec), std::move(legacy_codec)};
}

[[nodiscard]] std::filesystem::path sibling(
	const std::filesystem::path &path,
	const std::filesystem::path &suffix
)
{
	auto result = path;
	result += suffix.native();
	return result;
}

template<std::size_t Size>
[[nodiscard]] std::expected<std::array<std::byte, Size>, NewClassicScoreStoreError>
read_exact(const std::filesystem::path &path)
{
	std::error_code system;
	const auto status = std::filesystem::status(path, system);
	if(system) {
		return std::unexpected(store_error(
			NewClassicScoreStoreErrorCode::source_status, system
		));
	}
	if(!std::filesystem::is_regular_file(status)) {
		return std::unexpected(store_error(
			NewClassicScoreStoreErrorCode::source_not_regular
		));
	}
	if(std::filesystem::file_size(path, system) != Size || system) {
		return std::unexpected(store_error(
			NewClassicScoreStoreErrorCode::source_size, system
		));
	}
	std::ifstream input(path, std::ios::binary);
	if(!input) {
		return std::unexpected(store_error(
			NewClassicScoreStoreErrorCode::source_open
		));
	}
	std::array<std::byte, Size> bytes{};
	input.read(
		reinterpret_cast<char *>(bytes.data()),
		static_cast<std::streamsize>(bytes.size())
	);
	if(!input || input.peek() != std::ifstream::traits_type::eof()) {
		return std::unexpected(store_error(
			NewClassicScoreStoreErrorCode::source_read
		));
	}
	return bytes;
}

[[nodiscard]] bool exists_no_error(const std::filesystem::path &path) noexcept
{
	std::error_code ignored;
	return std::filesystem::exists(path, ignored);
}

[[nodiscard]] std::expected<void, NewClassicScoreStoreError> write_validated(
	const std::filesystem::path &path,
	const NewClassicScoreDocument &document
)
{
	auto encoded = encode_new_classic_score_file(document);
	if(!encoded) {
		return std::unexpected(store_error(
			NewClassicScoreStoreErrorCode::codec, {}, encoded.error()
		));
	}
	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	if(!output) {
		return std::unexpected(store_error(
			NewClassicScoreStoreErrorCode::temporary_open
		));
	}
	output.write(
		reinterpret_cast<const char *>(encoded->data()),
		static_cast<std::streamsize>(encoded->size())
	);
	output.flush();
	if(!output) {
		return std::unexpected(store_error(
			NewClassicScoreStoreErrorCode::temporary_write
		));
	}
	output.close();
	auto readback = read_exact<new_classic_score_file_bytes>(path);
	if(!readback) {
		return std::unexpected(store_error(
			NewClassicScoreStoreErrorCode::temporary_validation,
			readback.error().system
		));
	}
	auto decoded = decode_new_classic_score_file(*readback);
	if(*readback != *encoded || !decoded || decoded->document != document) {
		return std::unexpected(store_error(
			NewClassicScoreStoreErrorCode::temporary_validation, {},
			decoded ? std::nullopt :
				std::optional<NewClassicScoreError>{decoded.error()}
		));
	}
	return {};
}

[[nodiscard]] std::expected<void, NewClassicScoreStoreError> publish(
	const std::filesystem::path &path,
	const NewClassicScoreDocument &document
)
{
	const auto temporary = sibling(path, ".tmp");
	const auto old = sibling(path, ".old");
	std::error_code system;
	std::filesystem::remove(temporary, system);
	if(system) {
		return std::unexpected(store_error(
			NewClassicScoreStoreErrorCode::temporary_open, system
		));
	}
	if(auto written = write_validated(temporary, document); !written) {
		return std::unexpected(std::move(written.error()));
	}
	std::filesystem::remove(old, system);
	if(system) {
		return std::unexpected(store_error(
			NewClassicScoreStoreErrorCode::replace_target, system
		));
	}
	const bool had_canonical = exists_no_error(path);
	if(had_canonical) {
		std::filesystem::rename(path, old, system);
		if(system) {
			return std::unexpected(store_error(
				NewClassicScoreStoreErrorCode::replace_target, system
			));
		}
	}
	std::filesystem::rename(temporary, path, system);
	if(system) {
		if(had_canonical) {
			std::error_code ignored;
			std::filesystem::rename(old, path, ignored);
		}
		return std::unexpected(store_error(
			NewClassicScoreStoreErrorCode::replace_target, system
		));
	}
	std::filesystem::remove(old, system);
	return {};
}

struct Candidate final {
	NewClassicScoreDecodeResult decoded{};
	std::size_t source = 0U;
};

[[nodiscard]] std::optional<Candidate> read_candidate(
	const std::filesystem::path &path,
	std::size_t source
)
{
	auto bytes = read_exact<new_classic_score_file_bytes>(path);
	if(!bytes) {
		return std::nullopt;
	}
	auto decoded = decode_new_classic_score_file(
		*bytes, NewClassicScoreBoardPolicy::replace_invalid
	);
	if(!decoded) {
		return std::nullopt;
	}
	return Candidate{std::move(*decoded), source};
}

[[nodiscard]] std::optional<ClassicScoreDocument> read_legacy(
	const std::filesystem::path &path
)
{
	if(path.empty()) {
		return std::nullopt;
	}
	auto bytes = read_exact<classic_score_file_bytes>(path);
	if(!bytes) {
		return std::nullopt;
	}
	auto decoded = decode_classic_score_file(*bytes);
	if(!decoded) {
		return std::nullopt;
	}
	return std::move(*decoded);
}

} // namespace

std::expected<void, NewClassicScoreStoreError> commit_new_classic_score_store(
	const std::filesystem::path &path,
	NewClassicScoreDocument &document
)
{
	if(path.empty() || !path.has_filename()) {
		return std::unexpected(store_error(
			NewClassicScoreStoreErrorCode::invalid_path
		));
	}
	auto next = document;
	if(next.generation != std::numeric_limits<std::uint32_t>::max()) {
		++next.generation;
	}
	if(auto result = publish(path, next); !result) {
		return std::unexpected(std::move(result.error()));
	}
	document = std::move(next);
	return {};
}

std::expected<NewClassicScoreStoreLoadResult, NewClassicScoreStoreError>
load_new_classic_score_store(
	const std::filesystem::path &path,
	const std::filesystem::path &legacy_path
)
{
	if(path.empty() || !path.has_filename()) {
		return std::unexpected(store_error(
			NewClassicScoreStoreErrorCode::invalid_path
		));
	}
	const std::array<std::filesystem::path, 3U> candidates{{
		path,
		sibling(path, ".tmp"),
		sibling(path, ".old"),
	}};
	std::optional<Candidate> selected{};
	for(std::size_t source = 0U; source < candidates.size(); ++source) {
		auto candidate = read_candidate(candidates[source], source);
		if(candidate && (!selected ||
			candidate->decoded.document.generation >
				selected->decoded.document.generation)) {
			selected = std::move(candidate);
		}
	}
	if(selected) {
		const auto repaired = std::ranges::count_if(
			selected->decoded.repaired_boards,
			[](const auto &rank) {
				return std::ranges::any_of(rank, [](bool value) { return value; });
			}
		);
		std::size_t repaired_boards = 0U;
		for(const auto &rank : selected->decoded.repaired_boards) {
			repaired_boards += static_cast<std::size_t>(
				std::ranges::count(rank, true)
			);
		}
		auto document = std::move(selected->decoded.document);
		if(selected->source != 0U || repaired != 0) {
			if(auto committed = commit_new_classic_score_store(path, document);
				!committed) {
				return std::unexpected(std::move(committed.error()));
			}
		} else {
			std::error_code ignored;
			std::filesystem::remove(candidates[1U], ignored);
			std::filesystem::remove(candidates[2U], ignored);
		}
		return NewClassicScoreStoreLoadResult{
			.document = std::move(document),
			.disposition = repaired_boards != 0U ?
				NewClassicScoreStoreDisposition::repaired_boards :
				(selected->source == 0U ?
				 NewClassicScoreStoreDisposition::loaded :
				 NewClassicScoreStoreDisposition::recovered_candidate),
			.repaired_board_count = repaired_boards,
		};
	}

	auto document = new_classic_score_empty();
	auto disposition = NewClassicScoreStoreDisposition::created_empty;
	if(auto legacy = read_legacy(legacy_path)) {
		auto migrated = migrate_classic_score_document(*legacy);
		if(!migrated) {
			return std::unexpected(store_error(
				NewClassicScoreStoreErrorCode::codec, {}, migrated.error()
			));
		}
		document = std::move(*migrated);
		disposition = NewClassicScoreStoreDisposition::migrated_classic;
	}
	if(auto committed = commit_new_classic_score_store(path, document);
		!committed) {
		return std::unexpected(std::move(committed.error()));
	}
	return NewClassicScoreStoreLoadResult{
		.document = std::move(document),
		.disposition = disposition,
	};
}

} // namespace th03::app

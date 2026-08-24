#include "th03/app/new_classic_settings_store.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <fstream>
#include <span>
#include <string>
#include <system_error>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace th03::app {
namespace {

[[nodiscard]] NewClassicSettingsStoreError store_error(
NewClassicSettingsStoreErrorCode code,
	std::error_code system = {},
	std::optional<NewClassicSettingsError> codec = std::nullopt
) noexcept
{
	return {
		.code = code,
		.system = system,
		.codec = std::move(codec),
	};
}

[[nodiscard]] std::filesystem::path temporary_path(
	const std::filesystem::path &target
)
{
	static std::atomic<std::uint64_t> sequence{0U};
	auto result = target;
	result += std::filesystem::path(
		".tmp-" + std::to_string(sequence.fetch_add(1U))
	).native();
	return result;
}

template <std::size_t Size>
[[nodiscard]] std::expected<
	std::array<std::byte, Size>,
	NewClassicSettingsStoreError
> read_exact(
	const std::filesystem::path &path,
	NewClassicSettingsStoreErrorCode open_code,
	NewClassicSettingsStoreErrorCode read_code
)
{
	std::ifstream input(path, std::ios::binary);
	if(!input) {
		return std::unexpected(store_error(
			open_code, std::make_error_code(std::errc::io_error)
		));
	}
	std::array<std::byte, Size> bytes{};
	input.read(
		reinterpret_cast<char *>(bytes.data()),
		static_cast<std::streamsize>(bytes.size())
	);
	if(input.gcount() != static_cast<std::streamsize>(bytes.size()) ||
		input.peek() != std::ifstream::traits_type::eof()) {
		return std::unexpected(store_error(
			read_code, std::make_error_code(std::errc::io_error)
		));
	}
	return bytes;
}

[[nodiscard]] std::expected<
	std::optional<ClassicConfigOptions>,
	NewClassicSettingsStoreError
> read_classic_migration(const std::filesystem::path &path)
{
	if(path.empty() || !path.has_filename()) {
		return std::optional<ClassicConfigOptions>{};
	}
	std::error_code system;
	const auto status = std::filesystem::status(path, system);
	if(system == std::errc::no_such_file_or_directory ||
		(!system && !std::filesystem::exists(status))) {
		return std::optional<ClassicConfigOptions>{};
	}
	if(system) {
		return std::unexpected(store_error(
			NewClassicSettingsStoreErrorCode::migration_status, system
		));
	}
	if(!std::filesystem::is_regular_file(status)) {
		return std::optional<ClassicConfigOptions>{};
	}
	const auto size = std::filesystem::file_size(path, system);
	if(system) {
		return std::unexpected(store_error(
			NewClassicSettingsStoreErrorCode::migration_status, system
		));
	}
	if(size != classic_config_file_bytes) {
		return std::optional<ClassicConfigOptions>{};
	}
	auto bytes = read_exact<classic_config_file_bytes>(
		path,
		NewClassicSettingsStoreErrorCode::migration_open,
		NewClassicSettingsStoreErrorCode::migration_read
	);
	if(!bytes) {
		return std::unexpected(std::move(bytes.error()));
	}
	auto decoded = decode_classic_config_file(*bytes);
	if(!decoded) {
		return std::optional<ClassicConfigOptions>{};
	}
	return std::optional<ClassicConfigOptions>{decoded->options};
}

void remove_ignoring_error(const std::filesystem::path &path) noexcept
{
	std::error_code ignored;
	std::filesystem::remove(path, ignored);
}

} // namespace

std::expected<void, NewClassicSettingsStoreError>
commit_new_classic_settings_store(
	const std::filesystem::path &path,
	const NewClassicSettings &settings
)
{
	if(path.empty() || !path.has_filename()) {
		return std::unexpected(store_error(
			NewClassicSettingsStoreErrorCode::invalid_path
		));
	}
	auto encoded = encode_new_classic_settings(settings);
	if(!encoded) {
		return std::unexpected(store_error(
			NewClassicSettingsStoreErrorCode::codec, {}, encoded.error()
		));
	}
	std::error_code system;
	if(!path.parent_path().empty()) {
		std::filesystem::create_directories(path.parent_path(), system);
	}
	if(system) {
		return std::unexpected(store_error(
			NewClassicSettingsStoreErrorCode::temporary_create, system
		));
	}
	const auto temporary = temporary_path(path);
	{
		std::ofstream output(
			temporary, std::ios::binary | std::ios::trunc
		);
		if(!output) {
			return std::unexpected(store_error(
				NewClassicSettingsStoreErrorCode::temporary_create,
				std::make_error_code(std::errc::io_error)
			));
		}
		output.write(
			reinterpret_cast<const char *>(encoded->data()),
			static_cast<std::streamsize>(encoded->size())
		);
		output.flush();
		if(!output) {
			remove_ignoring_error(temporary);
			return std::unexpected(store_error(
				NewClassicSettingsStoreErrorCode::temporary_write,
				std::make_error_code(std::errc::io_error)
			));
		}
	}
	auto readback = read_exact<new_classic_settings_file_bytes>(
		temporary,
		NewClassicSettingsStoreErrorCode::temporary_read,
		NewClassicSettingsStoreErrorCode::temporary_read
	);
	if(!readback) {
		remove_ignoring_error(temporary);
		return std::unexpected(std::move(readback.error()));
	}
	auto decoded = decode_new_classic_settings(*readback);
	if(*readback != *encoded || !decoded || *decoded != settings) {
		remove_ignoring_error(temporary);
		return std::unexpected(store_error(
			NewClassicSettingsStoreErrorCode::temporary_validation,
			{},
			decoded ? std::nullopt :
				std::optional<NewClassicSettingsError>{decoded.error()}
		));
	}
#if defined(_WIN32)
	if(MoveFileExW(
		temporary.c_str(), path.c_str(),
		MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH
	) == 0) {
		system = {
			static_cast<int>(GetLastError()), std::system_category()
		};
	}
#else
	std::filesystem::rename(temporary, path, system);
#endif
	if(system) {
		remove_ignoring_error(temporary);
		return std::unexpected(store_error(
			NewClassicSettingsStoreErrorCode::publish, system
		));
	}
	return {};
}

std::expected<
	NewClassicSettingsStoreLoadResult,
	NewClassicSettingsStoreError
> load_new_classic_settings_store(
	const std::filesystem::path &path,
	const std::filesystem::path &classic_config_path,
	NewClassicSettings defaults
)
{
	if(path.empty() || !path.has_filename()) {
		return std::unexpected(store_error(
			NewClassicSettingsStoreErrorCode::invalid_path
		));
	}
	if(auto valid = validate_new_classic_settings(defaults); !valid) {
		return std::unexpected(store_error(
			NewClassicSettingsStoreErrorCode::invalid_defaults,
			{}, valid.error()
		));
	}
	std::error_code system;
	const auto status = std::filesystem::status(path, system);
	if(system == std::errc::no_such_file_or_directory ||
		(!system && !std::filesystem::exists(status))) {
		auto migration = read_classic_migration(classic_config_path);
		if(!migration) {
			return std::unexpected(std::move(migration.error()));
		}
		const auto settings = migration->has_value() ?
			new_classic_settings_from_classic(**migration) : defaults;
		if(auto committed = commit_new_classic_settings_store(path, settings);
			!committed) {
			return std::unexpected(std::move(committed.error()));
		}
		return NewClassicSettingsStoreLoadResult{
			.settings = settings,
			.disposition = migration->has_value() ?
				NewClassicSettingsStoreDisposition::migrated_classic :
				NewClassicSettingsStoreDisposition::created_defaults,
		};
	}
	if(system) {
		return std::unexpected(store_error(
			NewClassicSettingsStoreErrorCode::source_status, system
		));
	}
	if(!std::filesystem::is_regular_file(status)) {
		return std::unexpected(store_error(
			NewClassicSettingsStoreErrorCode::source_not_regular
		));
	}
	const auto size = std::filesystem::file_size(path, system);
	if(system) {
		return std::unexpected(store_error(
			NewClassicSettingsStoreErrorCode::source_size, system
		));
	}
	std::optional<NewClassicSettingsError> malformed{};
	if(size == new_classic_settings_file_bytes) {
		auto bytes = read_exact<new_classic_settings_file_bytes>(
			path,
			NewClassicSettingsStoreErrorCode::source_open,
			NewClassicSettingsStoreErrorCode::source_read
		);
		if(!bytes) {
			return std::unexpected(std::move(bytes.error()));
		}
		auto decoded = decode_new_classic_settings(*bytes);
		if(decoded) {
			const bool legacy_v4 =
				std::to_integer<std::uint8_t>((*bytes)[7U]) ==
					static_cast<std::uint8_t>('4');
			const bool legacy_v5 =
				std::to_integer<std::uint8_t>((*bytes)[7U]) ==
					static_cast<std::uint8_t>('5');
			const bool legacy_v6 =
				std::to_integer<std::uint8_t>((*bytes)[7U]) ==
					static_cast<std::uint8_t>('6');
			const bool legacy_v7 =
				std::to_integer<std::uint8_t>((*bytes)[7U]) ==
					static_cast<std::uint8_t>('7');
			if(legacy_v4 || legacy_v5 || legacy_v6 || legacy_v7) {
				if(auto committed = commit_new_classic_settings_store(path, *decoded);
					!committed) {
					return std::unexpected(std::move(committed.error()));
				}
			}
			return NewClassicSettingsStoreLoadResult{
				.settings = *decoded,
				.disposition = legacy_v4 ?
					NewClassicSettingsStoreDisposition::upgraded_v4 :
					(legacy_v5 ?
						NewClassicSettingsStoreDisposition::upgraded_v5 :
						(legacy_v6 ?
							NewClassicSettingsStoreDisposition::upgraded_v6 :
							(legacy_v7 ?
								NewClassicSettingsStoreDisposition::upgraded_v7 :
								NewClassicSettingsStoreDisposition::loaded))),
			};
		}
		malformed = decoded.error();
	} else if(size == new_classic_settings_v2_v3_file_bytes) {
		auto bytes = read_exact<new_classic_settings_v2_v3_file_bytes>(
			path,
			NewClassicSettingsStoreErrorCode::source_open,
			NewClassicSettingsStoreErrorCode::source_read
		);
		if(!bytes) {
			return std::unexpected(std::move(bytes.error()));
		}
		auto decoded = decode_new_classic_settings(*bytes);
		if(decoded) {
			if(auto committed = commit_new_classic_settings_store(path, *decoded);
				!committed) {
				return std::unexpected(std::move(committed.error()));
			}
			const bool legacy_v2 =
				std::to_integer<std::uint8_t>((*bytes)[7U]) ==
				static_cast<std::uint8_t>('2');
			return NewClassicSettingsStoreLoadResult{
				.settings = *decoded,
				.disposition = legacy_v2 ?
					NewClassicSettingsStoreDisposition::upgraded_v2 :
					NewClassicSettingsStoreDisposition::upgraded_v3,
			};
		}
		malformed = decoded.error();
	} else if(size == new_classic_settings_v1_file_bytes) {
		auto bytes = read_exact<new_classic_settings_v1_file_bytes>(
			path,
			NewClassicSettingsStoreErrorCode::source_open,
			NewClassicSettingsStoreErrorCode::source_read
		);
		if(!bytes) {
			return std::unexpected(std::move(bytes.error()));
		}
		auto decoded = decode_new_classic_settings(*bytes);
		if(decoded) {
			if(auto committed = commit_new_classic_settings_store(path, *decoded);
				!committed) {
				return std::unexpected(std::move(committed.error()));
			}
			return NewClassicSettingsStoreLoadResult{
				.settings = *decoded,
				.disposition = NewClassicSettingsStoreDisposition::upgraded_v1,
			};
		}
		malformed = decoded.error();
	} else {
		malformed = NewClassicSettingsError{
			.code = NewClassicSettingsErrorCode::input_size,
			.offset = static_cast<std::size_t>(size),
		};
	}
	if(auto committed = commit_new_classic_settings_store(path, defaults);
		!committed) {
		return std::unexpected(std::move(committed.error()));
	}
	return NewClassicSettingsStoreLoadResult{
		.settings = defaults,
		.disposition = NewClassicSettingsStoreDisposition::recovered_defaults,
		.recovered_from = malformed,
	};
}

} // namespace th03::app

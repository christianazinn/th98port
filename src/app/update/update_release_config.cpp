#include "th03/app/update/update_release_config.hpp"

#include "th03/app/update/update_release_config_generated.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace th03::app {
namespace {

[[nodiscard]] bool valid_https_url(std::string_view url) noexcept
{
	constexpr std::string_view prefix = "https://";
	if(!url.starts_with(prefix) || url.size() == prefix.size() ||
		url.find_first_of("\r\n\t #@\\") != std::string_view::npos) {
		return false;
	}
	const auto authority_end = url.find('/', prefix.size());
	const auto authority = url.substr(
		prefix.size(), authority_end == std::string_view::npos ?
			std::string_view::npos : authority_end - prefix.size()
	);
	return !authority.empty() && authority.front() != '.' &&
		authority.back() != '.';
}

[[nodiscard]] bool zero_key_id(
	const std::array<std::uint8_t, 16U> &key_id
) noexcept
{
	return std::ranges::all_of(key_id, [](std::uint8_t value) {
		return value == 0U;
	});
}

} // namespace

bool valid_update_release_configuration(
	const UpdateReleaseConfiguration &configuration
) noexcept
{
	const bool platform_valid = [&configuration]() {
		switch(configuration.platform) {
		case UpdatePlatform::windows_standalone:
		case UpdatePlatform::windows_x64:
		case UpdatePlatform::linux_x64:
		case UpdatePlatform::macos_x64:
		case UpdatePlatform::macos_arm64: return true;
		}
		return false;
	}();
	const bool channel_valid = configuration.channel == UpdateChannel::stable ||
		configuration.channel == UpdateChannel::beta;
	if(!valid_https_url(configuration.manifest_url) ||
		!platform_valid || !channel_valid ||
		configuration.installed_sequence == 0U ||
		configuration.pinned_keys.empty()) {
		return false;
	}
	for(std::size_t index = 0U;
		index < configuration.pinned_keys.size(); ++index) {
		const auto &pin = configuration.pinned_keys[index];
		if(zero_key_id(pin.key_id) || pin.subject_public_key_info.empty()) {
			return false;
		}
		for(std::size_t prior = 0U; prior < index; ++prior) {
			if(configuration.pinned_keys[prior].key_id == pin.key_id) {
				return false;
			}
		}
	}
	return true;
}

const std::optional<UpdateReleaseConfiguration> &
compiled_update_release_configuration() noexcept
{
#if TH03_UPDATE_RELEASE_CONFIGURED
	static const std::array<UpdatePinnedKey, 1U> pins{{UpdatePinnedKey{
		generated::update_release_key_id,
		generated::update_release_spki,
	}}};
	static const std::optional<UpdateReleaseConfiguration> configuration{
		UpdateReleaseConfiguration{
			.manifest_url = generated::update_release_manifest_url,
			.platform = generated::update_release_platform,
			.channel = generated::update_release_channel,
			.installed_sequence = generated::update_release_sequence,
			.pinned_keys = pins,
		}
	};
#else
	static const std::optional<UpdateReleaseConfiguration> configuration{};
#endif
	return configuration;
}

} // namespace th03::app

#include "th03/app/new_classic_netplay_room.hpp"

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <system_error>
#include <utility>

namespace th03::app {
namespace {

constexpr std::int32_t center_x = 320;
constexpr std::int32_t title_top = 64;
constexpr std::int32_t rows_top = 128;
constexpr std::int32_t row_height = 32;
constexpr std::int32_t label_left = 104;
constexpr std::int32_t value_right = 536;
constexpr std::int32_t footer_top = 336;
constexpr present::Index4 title_color = 9U;
constexpr present::Index4 selected_color = 12U;
constexpr present::Index4 normal_color = 15U;
constexpr present::Index4 detail_color = 13U;
constexpr std::size_t endpoint_limit = 47U;

[[nodiscard]] bool raw_any(NewClassicUiInput input) noexcept
{
	return input.up || input.down || input.left || input.right ||
		input.confirm || input.bomb || input.cancel;
}

[[nodiscard]] std::string_view decimal(
	std::uint16_t value,
	std::array<char, 8U> &storage
) noexcept
{
	const auto converted = std::to_chars(
		storage.data(), storage.data() + storage.size(), value
	);
	if(converted.ec != std::errc{}) {
		return {};
	}
	return {storage.data(), static_cast<std::size_t>(
		converted.ptr - storage.data()
	)};
}

[[nodiscard]] bool allowed_endpoint_character(char value) noexcept
{
	return (value >= '0' && value <= '9') ||
		(value >= 'A' && value <= 'Z') ||
		(value >= 'a' && value <= 'z') || value == '.' || value == ':' ||
		value == '-';
}

} // namespace

NewClassicNetplayRoom::NewClassicNetplayRoom(
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
	NewClassicLanguageAssetProfile language
) :
	patch_assets_(std::move(patch_assets)),
	language_(language)
{
	if(!patch_assets_ || !patch_surface_.reset(
		patch_assets_->replay_list_background
	) || !draw()) {
		status_ = NewClassicNetplayRoomStatus::back;
		return;
	}
	valid_ = true;
}

bool NewClassicNetplayRoom::draw() noexcept
{
	if(!patch_assets_ || !patch_surface_.begin_hidden_frame(
		patch_assets_->replay_list_background
	)) {
		return false;
	}
	const auto &font = patch_assets_->menu_font;
	const auto &messages = patch_assets_->messages(language_);
	auto &raster = patch_surface_.raster();
	if(!font.put_centered(raster, center_x, title_top,
		messages.text(NewClassicMessageId::netplay_room_title), title_color)) {
		return false;
	}

	if(screen_ == Screen::menu) {
		constexpr std::array rows{
			NewClassicMessageId::netplay_room_create,
			NewClassicMessageId::netplay_room_join,
			NewClassicMessageId::netplay_room_settings,
			NewClassicMessageId::netplay_room_return,
		};
		for(std::size_t row = 0U; row < rows.size(); ++row) {
			if(!font.put_centered(
				raster, center_x,
				rows_top + static_cast<std::int32_t>(row) * row_height,
				messages.text(rows[row]), row == selection_ ?
					selected_color : normal_color
			)) {
				return false;
			}
		}
	} else if(screen_ == Screen::host_confirmation) {
		std::array<char, 8U> storage{};
		if(!font.put_centered(raster, center_x, 112,
			messages.text(NewClassicMessageId::netplay_room_create),
			selected_color) ||
			!font.put(raster, label_left, 160,
				messages.text(NewClassicMessageId::netplay_room_host_port),
				normal_color) ||
			!font.put_right(raster, value_right, 160,
				decimal(host_port_, storage), selected_color) ||
			!font.put_centered(raster, center_x, 224,
				messages.text(NewClassicMessageId::netplay_room_socket_warning),
				detail_color)) {
			return false;
		}
	} else if(screen_ == Screen::join_entry) {
		const auto endpoint = join_endpoint_.empty() ?
			messages.text(NewClassicMessageId::netplay_room_endpoint_empty) :
			std::string_view(join_endpoint_);
		if(!font.put_centered(raster, center_x, 112,
			messages.text(NewClassicMessageId::netplay_room_join),
			selected_color) ||
			!font.put(raster, label_left, 160,
				messages.text(NewClassicMessageId::netplay_room_endpoint),
				normal_color) ||
			!font.put_right(raster, value_right, 160, endpoint,
				endpoint_invalid_ ? 11U : selected_color) ||
			!font.put_centered(raster, center_x, 224,
				messages.text(endpoint_invalid_ ?
					NewClassicMessageId::netplay_room_endpoint_invalid :
					NewClassicMessageId::netplay_room_endpoint_hint),
				detail_color)) {
			return false;
		}
	} else {
		std::array<char, 8U> storage{};
		if(!font.put_centered(raster, center_x, 112,
			messages.text(NewClassicMessageId::netplay_room_settings),
			selected_color) ||
			!font.put(raster, label_left, 160,
				messages.text(NewClassicMessageId::netplay_room_transport),
				selection_ == 0U ? selected_color : normal_color) ||
			!font.put_right(raster, value_right, 160,
				messages.text(NewClassicMessageId::netplay_room_direct_udp),
				detail_color) ||
			!font.put(raster, label_left, 192,
				messages.text(NewClassicMessageId::netplay_room_host_port),
				selection_ == 1U ? selected_color : normal_color) ||
			!font.put_right(raster, value_right, 192,
				decimal(host_port_, storage), selected_color) ||
			!font.put_centered(raster, center_x, 256,
				messages.text(NewClassicMessageId::netplay_room_return),
				selection_ == 2U ? selected_color : normal_color)) {
			return false;
		}
	}

	return font.put_centered(raster, center_x, footer_top,
		messages.text(screen_ == Screen::join_entry ?
			NewClassicMessageId::netplay_room_footer_edit :
			NewClassicMessageId::netplay_room_footer), detail_color) &&
		patch_surface_.publish(raster.access_page());
}

bool NewClassicNetplayRoom::valid() const noexcept
{
	return valid_;
}

bool NewClassicNetplayRoom::valid_join_endpoint() const noexcept
{
	const auto separator = join_endpoint_.rfind(':');
	if(separator == std::string::npos || separator == 0U ||
		separator + 1U >= join_endpoint_.size()) {
		return false;
	}
	std::uint32_t port = 0U;
	const auto begin = join_endpoint_.data() + separator + 1U;
	const auto end = join_endpoint_.data() + join_endpoint_.size();
	const auto parsed = std::from_chars(begin, end, port);
	return parsed.ec == std::errc{} && parsed.ptr == end && port > 0U &&
		port <= 65'535U;
}

void NewClassicNetplayRoom::require_release() noexcept
{
	release_required_ = true;
}

NewClassicNetplayRoomResult NewClassicNetplayRoom::consume(
	NewClassicNetplayRoomInput input
) noexcept
{
	if(status_ != NewClassicNetplayRoomStatus::running) {
		return {.status = status_};
	}
	if(input.ui.app_quit_requested) {
		status_ = NewClassicNetplayRoomStatus::application_quit;
		return {.status = status_};
	}
	if(release_required_) {
		if(!raw_any(input.ui)) {
			release_required_ = false;
		}
		return {.status = status_};
	}
	if(screen_ == Screen::join_entry) {
		bool changed = false;
		if((input.erase || input.ui.bomb) && !join_endpoint_.empty()) {
			join_endpoint_.pop_back();
			endpoint_invalid_ = false;
			changed = true;
		}
		if(input.text_character &&
			allowed_endpoint_character(*input.text_character) &&
			join_endpoint_.size() < endpoint_limit) {
			join_endpoint_.push_back(*input.text_character);
			endpoint_invalid_ = false;
			changed = true;
		}
		if(changed && !draw()) {
			status_ = NewClassicNetplayRoomStatus::back;
			return {.status = status_};
		}
		if(input.ui.cancel) {
			screen_ = Screen::menu;
			selection_ = 1U;
			endpoint_invalid_ = false;
			require_release();
			const bool drawn = draw();
			return {.surface_changed = drawn, .status = status_};
		}
		if(input.ui.confirm) {
			if(valid_join_endpoint()) {
				launch_ = NewClassicNetplayRoomLaunch{
					.mode = NewClassicNetplayRoomLaunchMode::join,
					.host_port = host_port_,
					.join_endpoint = join_endpoint_,
				};
				status_ = NewClassicNetplayRoomStatus::launch_requested;
				return {.status = status_};
			}
			endpoint_invalid_ = true;
			require_release();
			const bool drawn = draw();
			return {.surface_changed = drawn, .status = status_};
		}
		return {.surface_changed = changed, .status = status_};
	}

	bool changed = false;
	if(screen_ == Screen::menu) {
		if(input.ui.up || input.ui.down) {
			selection_ = static_cast<std::uint8_t>(
				(selection_ + (input.ui.up ? 3U : 1U)) % 4U
			);
			changed = true;
			require_release();
		} else if(input.ui.cancel) {
			status_ = NewClassicNetplayRoomStatus::back;
			return {.status = status_};
		} else if(input.ui.confirm) {
			if(selection_ == 0U) {
				screen_ = Screen::host_confirmation;
			} else if(selection_ == 1U) {
				screen_ = Screen::join_entry;
			} else if(selection_ == 2U) {
				screen_ = Screen::settings;
				selection_ = 0U;
			} else {
				status_ = NewClassicNetplayRoomStatus::back;
				return {.status = status_};
			}
			changed = true;
			require_release();
		}
	} else if(screen_ == Screen::host_confirmation) {
		if(input.ui.cancel) {
			screen_ = Screen::menu;
			selection_ = 0U;
			changed = true;
			require_release();
		} else if(input.ui.confirm) {
			launch_ = NewClassicNetplayRoomLaunch{
				.mode = NewClassicNetplayRoomLaunchMode::host,
				.host_port = host_port_,
			};
			status_ = NewClassicNetplayRoomStatus::launch_requested;
			return {.status = status_};
		}
	} else {
		if(input.ui.up || input.ui.down) {
			selection_ = static_cast<std::uint8_t>(
				(selection_ + (input.ui.up ? 2U : 1U)) % 3U
			);
			changed = true;
			require_release();
		} else if(selection_ == 1U && (input.ui.left || input.ui.right)) {
			if(input.ui.left) {
				host_port_ = host_port_ > 1'024U ?
					static_cast<std::uint16_t>(host_port_ - 1U) : 65'535U;
			} else {
				host_port_ = host_port_ < 65'535U ?
					static_cast<std::uint16_t>(host_port_ + 1U) : 1'024U;
			}
			changed = true;
			require_release();
		} else if(input.ui.cancel ||
			(input.ui.confirm && selection_ == 2U)) {
			screen_ = Screen::menu;
			selection_ = 2U;
			changed = true;
			require_release();
		}
	}
	if(changed && !draw()) {
		status_ = NewClassicNetplayRoomStatus::back;
		return {.status = status_};
	}
	return {.surface_changed = changed, .status = status_};
}

present::SourceSurface NewClassicNetplayRoom::surface() const noexcept
{
	return patch_surface_.surface();
}

NewClassicNetplayRoomStatus NewClassicNetplayRoom::status() const noexcept
{
	return status_;
}

std::optional<NewClassicNetplayRoomLaunch>
NewClassicNetplayRoom::take_launch() noexcept
{
	return std::exchange(launch_, std::nullopt);
}

} // namespace th03::app

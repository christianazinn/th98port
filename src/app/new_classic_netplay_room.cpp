#include "th03/app/new_classic_netplay_room.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <span>
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
constexpr std::array character_messages{
	NewClassicMessageId::replay_character_reimu,
	NewClassicMessageId::replay_character_mima,
	NewClassicMessageId::replay_character_marisa,
	NewClassicMessageId::replay_character_ellen,
	NewClassicMessageId::replay_character_kotohime,
	NewClassicMessageId::replay_character_kana,
	NewClassicMessageId::replay_character_rikako,
	NewClassicMessageId::replay_character_chiyuri,
	NewClassicMessageId::replay_character_yumemi,
};
constexpr std::array rank_messages{
	NewClassicMessageId::value_easy,
	NewClassicMessageId::value_normal,
	NewClassicMessageId::value_hard,
	NewClassicMessageId::value_lunatic,
};

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
	NewClassicLanguageAssetProfile language,
	std::uint8_t initial_rank
) :
	patch_assets_(std::move(patch_assets)),
	language_(language),
	rank_(initial_rank)
{
	if(rank_ > 3U || !patch_assets_ || !patch_surface_.reset(
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
	} else if(screen_ == Screen::player_setup) {
		const auto setup_title = pending_mode_ ==
			NewClassicNetplayRoomLaunchMode::host ?
				NewClassicMessageId::netplay_room_player_one :
				NewClassicMessageId::netplay_room_player_two;
		if(!font.put_centered(raster, center_x, 96,
			messages.text(setup_title), selected_color) ||
			!font.put(raster, label_left, 136,
				messages.text(NewClassicMessageId::netplay_room_character),
				selection_ == 0U ? selected_color : normal_color) ||
			!font.put_right(raster, value_right, 136,
				messages.text(character_messages[character_ - 1U]), detail_color) ||
			!font.put(raster, label_left, 168,
				messages.text(NewClassicMessageId::netplay_room_palette),
				selection_ == 1U ? selected_color : normal_color) ||
			!font.put_right(raster, value_right, 168,
				messages.text(palette_ == 1U ?
					NewClassicMessageId::netplay_room_palette_first :
					NewClassicMessageId::netplay_room_palette_second), detail_color) ||
			!font.put(raster, label_left, 200,
				messages.text(NewClassicMessageId::option_rank),
				selection_ == 2U ? selected_color : normal_color) ||
			!font.put_right(raster, value_right, 200,
				messages.text(pending_mode_ ==
					NewClassicNetplayRoomLaunchMode::host ? rank_messages[rank_] :
					NewClassicMessageId::netplay_room_host_decides), detail_color) ||
			!font.put_centered(raster, center_x, 248,
				messages.text(NewClassicMessageId::netplay_room_continue),
				selection_ == 3U ? selected_color : normal_color) ||
			!font.put_centered(raster, center_x, 280,
				messages.text(NewClassicMessageId::netplay_room_return),
				selection_ == 4U ? selected_color : normal_color)) {
			return false;
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
			screen_ = Screen::player_setup;
			selection_ = 3U;
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
					.character = character_,
					.palette = palette_,
					.rank = rank_,
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
			if(selection_ == 0U || selection_ == 1U) {
				pending_mode_ = selection_ == 0U ?
					NewClassicNetplayRoomLaunchMode::host :
					NewClassicNetplayRoomLaunchMode::join;
				palette_ = pending_mode_ ==
					NewClassicNetplayRoomLaunchMode::host ? 1U : 2U;
				screen_ = Screen::player_setup;
				selection_ = 0U;
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
	} else if(screen_ == Screen::player_setup) {
		constexpr std::array<std::uint8_t, 5U> host_rows{0U, 1U, 2U, 3U, 4U};
		constexpr std::array<std::uint8_t, 4U> join_rows{0U, 1U, 3U, 4U};
		const auto rows = pending_mode_ == NewClassicNetplayRoomLaunchMode::host ?
			std::span<const std::uint8_t>(host_rows) :
			std::span<const std::uint8_t>(join_rows);
		if(input.ui.up || input.ui.down) {
			const auto found = std::find(rows.begin(), rows.end(), selection_);
			if(found == rows.end()) {
				status_ = NewClassicNetplayRoomStatus::back;
				return {.status = status_};
			}
			auto next = static_cast<std::int32_t>(found - rows.begin()) +
				(input.ui.up ? -1 : 1);
			if(next < 0) {
				next = static_cast<std::int32_t>(rows.size() - 1U);
			} else if(next >= static_cast<std::int32_t>(rows.size())) {
				next = 0;
			}
			selection_ = rows[static_cast<std::size_t>(next)];
			changed = true;
			require_release();
		} else if((input.ui.left || input.ui.right) && selection_ < 3U) {
			const int direction = input.ui.left ? -1 : 1;
			if(selection_ == 0U) {
				character_ = static_cast<std::uint8_t>(
					((static_cast<int>(character_) - 1 + direction + 9) % 9) + 1
				);
			} else if(selection_ == 1U) {
				palette_ = palette_ == 1U ? 2U : 1U;
			} else if(pending_mode_ == NewClassicNetplayRoomLaunchMode::host) {
				rank_ = static_cast<std::uint8_t>(
					(static_cast<int>(rank_) + direction + 4) % 4
				);
			}
			changed = true;
			require_release();
		} else if(input.ui.cancel ||
			(input.ui.confirm && selection_ == 4U)) {
			screen_ = Screen::menu;
			selection_ = pending_mode_ ==
				NewClassicNetplayRoomLaunchMode::host ? 0U : 1U;
			changed = true;
			require_release();
		} else if(input.ui.confirm && selection_ == 3U) {
			screen_ = pending_mode_ ==
				NewClassicNetplayRoomLaunchMode::host ?
					Screen::host_confirmation : Screen::join_entry;
			changed = true;
			require_release();
		}
	} else if(screen_ == Screen::host_confirmation) {
		if(input.ui.cancel) {
			screen_ = Screen::player_setup;
			selection_ = 3U;
			changed = true;
			require_release();
		} else if(input.ui.confirm) {
			launch_ = NewClassicNetplayRoomLaunch{
				.mode = NewClassicNetplayRoomLaunchMode::host,
				.host_port = host_port_,
				.character = character_,
				.palette = palette_,
				.rank = rank_,
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

NewClassicNetplayLobbyView::NewClassicNetplayLobbyView(
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
	NewClassicLanguageAssetProfile language,
	NewClassicNetplayLobbyViewRole role,
	NewClassicNetplayCharacterLock local_lock,
	std::uint8_t rank
) :
	patch_assets_(std::move(patch_assets)),
	language_(language),
	role_(role),
	local_lock_(local_lock),
	rank_(rank)
{
	if(!patch_assets_ || local_lock_.character < 1U ||
		local_lock_.character > character_messages.size() ||
		local_lock_.palette < 1U || local_lock_.palette > 2U || rank_ > 3U ||
		!patch_surface_.reset(patch_assets_->replay_list_background) || !draw()) {
		return;
	}
	valid_ = true;
}

bool NewClassicNetplayLobbyView::draw() noexcept
{
	if(!patch_assets_ || !patch_surface_.begin_hidden_frame(
		patch_assets_->replay_list_background
	)) {
		return false;
	}
	const auto &font = patch_assets_->menu_font;
	const auto &messages = patch_assets_->messages(language_);
	auto &raster = patch_surface_.raster();
	const auto ready = ready_contract_.has_value();
	if(!font.put_centered(raster, center_x, title_top,
		messages.text(NewClassicMessageId::netplay_room_title), title_color) ||
		!font.put_centered(raster, center_x, 104,
			messages.text(ready ? NewClassicMessageId::netplay_room_match_ready :
				(role_ == NewClassicNetplayLobbyViewRole::host ?
					NewClassicMessageId::netplay_room_waiting_player :
					NewClassicMessageId::netplay_room_contacting_host)),
			ready ? selected_color : detail_color)) {
		return false;
	}

	const auto put_lock = [&](std::int32_t y, NewClassicMessageId label,
		NewClassicNetplayCharacterLock lock) noexcept {
		return font.put(raster, label_left, y, messages.text(label), normal_color) &&
			font.put_right(raster, value_right, y,
				messages.text(character_messages[lock.character - 1U]), detail_color) &&
			font.put_right(raster, value_right, y + 20,
				messages.text(lock.palette == 1U ?
					NewClassicMessageId::netplay_room_palette_first :
					NewClassicMessageId::netplay_room_palette_second), detail_color);
	};
	if(ready) {
		if(!put_lock(144, NewClassicMessageId::netplay_room_player_one,
			ready_contract_->player_one) ||
			!put_lock(208, NewClassicMessageId::netplay_room_player_two,
				ready_contract_->player_two) ||
			!font.put(raster, label_left, 272,
				messages.text(NewClassicMessageId::option_rank), normal_color) ||
			!font.put_right(raster, value_right, 272,
				messages.text(rank_messages[ready_contract_->rank]), detail_color)) {
			return false;
		}
	} else {
		const auto local_label = role_ == NewClassicNetplayLobbyViewRole::host ?
			NewClassicMessageId::netplay_room_player_one :
			NewClassicMessageId::netplay_room_player_two;
		if(!put_lock(160, local_label, local_lock_) ||
			!font.put(raster, label_left, 240,
				messages.text(NewClassicMessageId::option_rank), normal_color) ||
			!font.put_right(raster, value_right, 240,
				messages.text(role_ == NewClassicNetplayLobbyViewRole::host ?
					rank_messages[rank_] :
					NewClassicMessageId::netplay_room_host_decides), detail_color)) {
			return false;
		}
	}
	return font.put_centered(raster, center_x, footer_top,
		messages.text(ready ? NewClassicMessageId::netplay_room_starting :
			NewClassicMessageId::netplay_room_cancel_wait), detail_color) &&
		patch_surface_.publish(raster.access_page());
}

bool NewClassicNetplayLobbyView::valid() const noexcept
{
	return valid_;
}

bool NewClassicNetplayLobbyView::mark_ready(
	const NewClassicNetplayMatchContract &contract
) noexcept
{
	const auto validated = create_new_classic_netplay_match_contract({
		.player_one = contract.player_one,
		.rank = contract.rank,
		.seed = contract.seed,
		.cadence_ticks = contract.cadence_ticks,
		.cadence_seconds = contract.cadence_seconds,
	}, {.player_two = contract.player_two});
	if(!validated || *validated != contract) {
		return false;
	}
	ready_contract_ = contract;
	return draw();
}

present::SourceSurface NewClassicNetplayLobbyView::surface() const noexcept
{
	return patch_surface_.surface();
}

} // namespace th03::app

#include "th03/app/new_classic_netplay_report.hpp"

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
constexpr std::int32_t outcome_top = 88;
constexpr std::int32_t rows_top = 128;
constexpr std::int32_t row_height = 20;
constexpr std::int32_t label_left = 112;
constexpr std::int32_t value_right = 528;
constexpr std::int32_t footer_top = 336;
constexpr present::Index4 title_color = 9U;
constexpr present::Index4 outcome_color = 12U;
constexpr present::Index4 label_color = 15U;
constexpr present::Index4 value_color = 12U;
constexpr present::Index4 footer_color = 13U;

[[nodiscard]] constexpr NewClassicMessageId outcome_message(
	NewClassicNetplayReportKind kind
) noexcept
{
	switch(kind) {
	case NewClassicNetplayReportKind::match_complete:
		return NewClassicMessageId::netplay_report_match_complete;
	case NewClassicNetplayReportKind::forfeit_loss:
		return NewClassicMessageId::netplay_report_forfeit_loss;
	case NewClassicNetplayReportKind::forfeit_win:
		return NewClassicMessageId::netplay_report_forfeit_win;
	case NewClassicNetplayReportKind::forfeit_mutual:
		return NewClassicMessageId::netplay_report_forfeit_mutual;
	case NewClassicNetplayReportKind::peer_left:
		return NewClassicMessageId::netplay_report_peer_left;
	case NewClassicNetplayReportKind::peer_timeout:
		return NewClassicMessageId::netplay_report_peer_timeout;
	case NewClassicNetplayReportKind::desync:
		return NewClassicMessageId::netplay_report_desync;
	case NewClassicNetplayReportKind::connection_failed:
		return NewClassicMessageId::netplay_room_connection_failed;
	}
	return NewClassicMessageId::netplay_report_desync;
}

[[nodiscard]] std::string_view decimal(
	std::uint64_t value,
	std::array<char, 32U> &storage
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

[[nodiscard]] bool raw_any(NewClassicUiInput input) noexcept
{
	return input.up || input.down || input.left || input.right ||
		input.confirm || input.bomb || input.cancel;
}

} // namespace

NewClassicNetplayReport::NewClassicNetplayReport(
	NewClassicNetplayReportKind kind,
	NewClassicNetplayReportMetrics metrics,
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
	NewClassicLanguageAssetProfile language
) :
	kind_(kind),
	metrics_(metrics),
	patch_assets_(std::move(patch_assets)),
	language_(language)
{
	if(!patch_assets_ || !patch_surface_.reset(
		patch_assets_->new_menu_background
	) || !draw()) {
		status_ = NewClassicNetplayReportStatus::complete;
		return;
	}
	valid_ = true;
}

bool NewClassicNetplayReport::draw() noexcept
{
	if(!patch_assets_ || !patch_surface_.begin_hidden_frame(
		patch_assets_->new_menu_background
	)) {
		return false;
	}
	const auto &font = patch_assets_->menu_font;
	const auto &messages = patch_assets_->messages(language_);
	auto &raster = patch_surface_.raster();
	if(!font.put_centered(raster, center_x, title_top,
		messages.text(NewClassicMessageId::netplay_report_title), title_color) ||
		!font.put_centered(raster, center_x, outcome_top,
			messages.text(outcome_message(kind_)), outcome_color)) {
		return false;
	}
	if(kind_ == NewClassicNetplayReportKind::connection_failed) {
		return font.put_centered(raster, center_x, 160,
			messages.text(NewClassicMessageId::netplay_room_connection_retry),
			label_color) &&
			font.put_centered(raster, center_x, footer_top,
				messages.text(NewClassicMessageId::netplay_report_footer), footer_color) &&
			patch_surface_.publish(raster.access_page());
	}

	const std::array labels{
		NewClassicMessageId::netplay_report_label_frames,
		NewClassicMessageId::netplay_report_label_predicted,
		NewClassicMessageId::netplay_report_label_rollbacks,
		NewClassicMessageId::netplay_report_label_rewinds,
		NewClassicMessageId::netplay_report_label_resimulated,
		NewClassicMessageId::netplay_report_label_stalls,
		NewClassicMessageId::netplay_report_label_remote_waits,
		NewClassicMessageId::netplay_report_label_max_prediction,
		NewClassicMessageId::netplay_report_label_missed_deadlines,
		NewClassicMessageId::netplay_report_label_round_trip,
	};
	const std::array<std::optional<std::uint64_t>, labels.size()> values{
		metrics_.confirmed_frames,
		metrics_.session.predicted_frames,
		metrics_.session.rollback_requests,
		metrics_.session.rewinds,
		metrics_.session.resimulated_frames,
		metrics_.session.prediction_stall_events,
		metrics_.session.remote_wait_events,
		metrics_.session.maximum_prediction_depth,
		metrics_.missed_deadlines,
		metrics_.round_trip_time_ms,
	};
	for(std::size_t index = 0U; index < labels.size(); ++index) {
		std::array<char, 32U> storage{};
		const auto top = rows_top +
			static_cast<std::int32_t>(index) * row_height;
		const auto value = values[index] ? decimal(*values[index], storage) : "--";
		if(!font.put(raster, label_left, top, messages.text(labels[index]),
				label_color) ||
			!font.put_right(raster, value_right, top, value, value_color)) {
			return false;
		}
	}
	return font.put_centered(raster, center_x, footer_top,
		messages.text(NewClassicMessageId::netplay_report_footer), footer_color) &&
		patch_surface_.publish(raster.access_page());
}

bool NewClassicNetplayReport::valid() const noexcept
{
	return valid_;
}

NewClassicNetplayReportResult NewClassicNetplayReport::consume(
	NewClassicUiInput input
) noexcept
{
	if(status_ != NewClassicNetplayReportStatus::running) {
		return {.status = status_};
	}
	if(input.app_quit_requested) {
		status_ = NewClassicNetplayReportStatus::application_quit;
		return {.status = status_};
	}
	if(release_required_) {
		if(!raw_any(input)) {
			release_required_ = false;
		}
		return {.status = status_};
	}
	if(raw_any(input)) {
		status_ = NewClassicNetplayReportStatus::complete;
	}
	return {.status = status_};
}

present::SourceSurface NewClassicNetplayReport::surface() const noexcept
{
	return patch_surface_.surface();
}

NewClassicNetplayReportStatus NewClassicNetplayReport::status() const noexcept
{
	return status_;
}

NewClassicNetplayReportKind NewClassicNetplayReport::kind() const noexcept
{
	return kind_;
}

const NewClassicNetplayReportMetrics &NewClassicNetplayReport::metrics() const
	noexcept
{
	return metrics_;
}

} // namespace th03::app

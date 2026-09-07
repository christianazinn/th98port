#include "th03/app/oracle/pixel_oracle_identity.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <ostream>
#include <type_traits>

namespace th03::app {
namespace {

[[nodiscard]] bool same_commit_identity(
	const PixelOracleIdentity &left,
	const PixelOracleIdentity &right
) noexcept
{
	return left.process_instance == right.process_instance &&
		left.process == right.process && left.scene == right.scene &&
		left.scene_visit == right.scene_visit &&
		left.logical_sample == right.logical_sample &&
		left.replay_global_frame == right.replay_global_frame &&
		left.round_frame == right.round_frame;
}

template<typename T>
void write_optional(std::ostream &output, std::optional<T> value)
{
	if(value) {
		output << *value;
	} else {
		output << "na";
	}
}

template<typename T>
[[nodiscard]] std::optional<T> parse_unsigned(std::string_view text) noexcept
{
	static_assert(std::is_unsigned_v<T>);
	T value = 0U;
	const auto result = std::from_chars(
		text.data(), text.data() + text.size(), value
	);
	if(text.empty() || result.ec != std::errc{} ||
		result.ptr != text.data() + text.size()) {
		return std::nullopt;
	}
	return value;
}

[[nodiscard]] std::expected<std::optional<std::uint32_t>, const char *>
parse_optional_u32(std::string_view text) noexcept
{
	if(text == "na") {
		return std::optional<std::uint32_t>{};
	}
	const auto value = parse_unsigned<std::uint32_t>(text);
	if(!value) {
		return std::unexpected("invalid-optional-identity");
	}
	return std::optional<std::uint32_t>{*value};
}

[[nodiscard]] std::optional<PixelOracleProcess> parse_process(
	std::string_view text
) noexcept
{
	if(text == "op") { return PixelOracleProcess::op; }
	if(text == "main") { return PixelOracleProcess::main; }
	if(text == "mainl") { return PixelOracleProcess::mainl; }
	return std::nullopt;
}

[[nodiscard]] std::optional<PixelOracleScene> parse_scene(
	std::string_view text
) noexcept
{
	for(std::uint8_t value = static_cast<std::uint8_t>(PixelOracleScene::unknown);
		value <= static_cast<std::uint8_t>(PixelOracleScene::game_over); ++value) {
		const auto scene = static_cast<PixelOracleScene>(value);
		if(pixel_oracle_scene_name(scene) == text) {
			return scene;
		}
	}
	return std::nullopt;
}

[[nodiscard]] std::optional<PixelOracleBoundary> parse_boundary(
	std::string_view text
) noexcept
{
	if(text == "bootstrap") { return PixelOracleBoundary::bootstrap; }
	if(text == "scene_enter") { return PixelOracleBoundary::scene_enter; }
	if(text == "source_tick") { return PixelOracleBoundary::source_tick; }
	return std::nullopt;
}

} // namespace

bool PixelOracleCaptureSelector::matches(
	const PixelOracleIdentity &identity
) const noexcept
{
	return identity.source_changed &&
		identity.process_instance == process_instance &&
		identity.process == process && identity.scene == scene &&
		identity.scene_visit == scene_visit &&
		identity.logical_sample == logical_sample &&
		identity.replay_global_frame == replay_global_frame &&
		identity.round_frame == round_frame && identity.commit == commit &&
		identity.boundary == boundary;
}

std::expected<PixelOracleCaptureSelector, const char *>
parse_pixel_oracle_capture_selector(std::string_view text) noexcept
{
	std::array<std::string_view, 9U> fields{};
	for(std::size_t index = 0U; index < fields.size(); ++index) {
		const auto separator = text.find('|');
		if(index + 1U == fields.size()) {
			if(separator != std::string_view::npos) {
				return std::unexpected("invalid-selector-field-count");
			}
			fields[index] = text;
			text = {};
			continue;
		}
		if(separator == std::string_view::npos) {
			return std::unexpected("invalid-selector-field-count");
		}
		fields[index] = text.substr(0U, separator);
		text.remove_prefix(separator + 1U);
	}

	const auto process_instance = parse_unsigned<std::uint64_t>(fields[0U]);
	const auto process = parse_process(fields[1U]);
	const auto scene = parse_scene(fields[2U]);
	const auto scene_visit = parse_unsigned<std::uint32_t>(fields[3U]);
	const auto logical_sample = parse_optional_u32(fields[4U]);
	const auto replay_global_frame = parse_optional_u32(fields[5U]);
	const auto round_frame = parse_optional_u32(fields[6U]);
	const auto commit = parse_unsigned<std::uint32_t>(fields[7U]);
	const auto boundary = parse_boundary(fields[8U]);
	if(!process_instance || !process || !scene || !scene_visit ||
		!logical_sample || !replay_global_frame || !round_frame || !commit ||
		!boundary) {
		return std::unexpected("invalid-selector-field");
	}
	return PixelOracleCaptureSelector{
		.process_instance = *process_instance,
		.process = *process,
		.scene = *scene,
		.scene_visit = *scene_visit,
		.logical_sample = *logical_sample,
		.replay_global_frame = *replay_global_frame,
		.round_frame = *round_frame,
		.commit = *commit,
		.boundary = *boundary,
	};
}

PixelOracleIdentity PixelOracleIdentityTracker::bootstrap(
	PixelOracleSourceIdentity source
)
{
	source.boundary = PixelOracleBoundary::bootstrap;
	return assign(source, true);
}

PixelOracleIdentity PixelOracleIdentityTracker::consume(
	PixelOracleSourceIdentity source,
	bool source_changed
)
{
	return assign(source, source_changed);
}

PixelOracleIdentity PixelOracleIdentityTracker::assign(
	PixelOracleSourceIdentity source,
	bool source_changed
)
{
	if(!previous_process || *previous_process != source.process) {
		// Every guest executable transition is a fresh invocation, including a
		// resumed OP after MAINL/MAIN. Native aggregate lifetime is irrelevant.
		active_process_instance_ = next_process_instance_++;
	}
	previous_process = source.process;

	const bool scene_entry = !previous_scene_process_instance_ ||
		*previous_scene_process_instance_ != active_process_instance_ ||
		!previous_scene_ || *previous_scene_ != source.scene;
	if(scene_entry) {
		if(!previous_scene_process_instance_ ||
			*previous_scene_process_instance_ != active_process_instance_) {
			active_scene_visit_ = 0U;
		} else {
			++active_scene_visit_;
		}
		previous_scene_process_instance_ = active_process_instance_;
		previous_scene_ = source.scene;
	}
	PixelOracleIdentity identity{
		.event = next_event_++,
		.process_instance = active_process_instance_,
		.process = source.process,
		.scene = source.scene,
		.scene_visit = active_scene_visit_,
		.logical_sample = source.logical_sample,
		.replay_global_frame = source.replay_global_frame,
		.round_frame = source.round_frame,
		.boundary = source.boundary,
		.source_changed = source_changed,
	};
	if(previous_identity_ && same_commit_identity(*previous_identity_, identity)) {
		identity.commit = previous_identity_->commit + 1U;
	}
	previous_identity_ = identity;
	return identity;
}

std::string_view pixel_oracle_process_name(PixelOracleProcess process) noexcept
{
	switch(process) {
	case PixelOracleProcess::op: return "op";
	case PixelOracleProcess::main: return "main";
	case PixelOracleProcess::mainl: return "mainl";
	}
	return "unknown";
}

std::string_view pixel_oracle_scene_name(PixelOracleScene scene) noexcept
{
	switch(scene) {
	case PixelOracleScene::unknown: return "unknown";
	case PixelOracleScene::title: return "title";
	case PixelOracleScene::options: return "options";
	case PixelOracleScene::character_select: return "character_select";
	case PixelOracleScene::practice_setup: return "practice_setup";
	case PixelOracleScene::key_config: return "key_config";
	case PixelOracleScene::replay_browser: return "replay_browser";
	case PixelOracleScene::replay_save: return "replay_save";
	case PixelOracleScene::high_score: return "high_score";
	case PixelOracleScene::gameplay: return "gameplay";
	case PixelOracleScene::pause: return "pause";
	case PixelOracleScene::stage_splash: return "stage_splash";
	case PixelOracleScene::win: return "win";
	case PixelOracleScene::continue_menu: return "continue";
	case PixelOracleScene::ending: return "ending";
	case PixelOracleScene::staff_roll: return "staff_roll";
	case PixelOracleScene::music_room: return "music_room";
	case PixelOracleScene::game_over: return "game_over";
	}
	return "unknown";
}

std::string_view pixel_oracle_boundary_name(
	PixelOracleBoundary boundary
) noexcept
{
	switch(boundary) {
	case PixelOracleBoundary::bootstrap: return "bootstrap";
	case PixelOracleBoundary::scene_enter: return "scene_enter";
	case PixelOracleBoundary::source_tick: return "source_tick";
	}
	return "unknown";
}

bool write_t3ali1_header(std::ostream &output)
{
	output << "stream\tschema\tevent\tfixture_sha256\tprocess_instance\t"
		"process\tscene\t"
		"scene_visit\tlogical_sample\treplay_global_frame\tround_frame\t"
		"commit\tboundary\tsource_changed\tpublication\n";
	output.flush();
	return static_cast<bool>(output);
}

bool write_t3ali1_event(
	std::ostream &output,
	std::string_view fixture_sha256,
	const PixelOracleIdentity &identity,
	std::optional<std::uint64_t> publication
)
{
	if(!valid_pixel_oracle_fixture_sha256(fixture_sha256)) {
		return false;
	}
	output << "T3ALI\t1\t" << identity.event << '\t'
		<< fixture_sha256 << '\t'
		<< identity.process_instance << '\t'
		<< pixel_oracle_process_name(identity.process) << '\t'
		<< pixel_oracle_scene_name(identity.scene) << '\t'
		<< identity.scene_visit << '\t';
	write_optional(output, identity.logical_sample);
	output << '\t';
	write_optional(output, identity.replay_global_frame);
	output << '\t';
	write_optional(output, identity.round_frame);
	output << '\t' << identity.commit << '\t'
		<< pixel_oracle_boundary_name(identity.boundary) << '\t'
		<< static_cast<unsigned>(identity.source_changed) << '\t';
	write_optional(output, publication);
	output << '\n';
	output.flush();
	return static_cast<bool>(output);
}

bool valid_pixel_oracle_fixture_sha256(
	std::string_view fixture_sha256
) noexcept
{
	return fixture_sha256.size() == 64U && std::ranges::all_of(
		fixture_sha256, [](char digit) {
			return (digit >= '0' && digit <= '9') ||
				(digit >= 'a' && digit <= 'f') ||
				(digit >= 'A' && digit <= 'F');
		}
	);
}

} // namespace th03::app

#include "th03/app/pixel_oracle_identity.hpp"

#include <algorithm>
#include <ostream>

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

} // namespace

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

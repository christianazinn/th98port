#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "th03/app/new_classic/replay/replay_slot_store.hpp"
#include "th03/core/game.hpp"
#include "th03/formats/replay.hpp"
#include "th03/formats/v14.hpp"
#include "th03/replay/session.hpp"

namespace {

int checks = 0;
int failures = 0;

void check(bool condition, const std::string &message)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << message << '\n';
	}
}

th03::formats::ReplayDocument make_document()
{
	constexpr auto mode = th03::game_mode::vs_1p_cpu;
	th03::GameInit init{};
	init.resident_rand = 29;
	init.random_seed = 0x12345678U;
	init.game_mode = mode;
	init.is_cpu = {0U, 1U};
	init.paletted_character = {1, 4};
	init.compact_runtime_snapshot = th03::CompactRuntimeSnapshot{
		.round_reset_seed = 29U,
		.random_seed = init.random_seed,
		.first_formation = 1U,
	};
	init.round_start_snapshot = th03::RoundStartSnapshot{};

	th03::formats::ReplayMetadata metadata{};
	metadata.flags = 0x0003U;
	metadata.status = th03::formats::ReplayStatus::finalized;
	metadata.end_reason = th03::formats::ReplayEndReason::menu_return;
	metadata.game_mode = mode;
	metadata.paletted_character = init.paletted_character;
	metadata.is_cpu = init.is_cpu;
	metadata.sample_count = 1U;
	metadata.final_frame_count = 1U;
	metadata.resident_rand = init.resident_rand;
	metadata.random_seed = init.random_seed;
	metadata.summary_flags = 0x007FU;
	metadata.final_route = std::uint8_t{2U};
	metadata.final_game_mode = mode;
	metadata.final_round_id = std::uint8_t{0U};
	metadata.final_winner = std::int8_t{0};

	th03::formats::ReplayDocument document{};
	document.session.source_version = th03::formats::replay_v14_version;
	document.session.init = init;
	document.session.records = {
		th03::replay::InputSample{
			.frame_index = 0U,
			.phase = th03::replay::InputPhase::gameplay,
			.input = {},
			.round_frame = std::nullopt,
			.round_or_result_frame = std::nullopt,
		},
		th03::replay::ProcessBoundary{
			.sample_index = 1U,
			.completed_process = th03::replay::ProcessId::main,
		},
	};
	document.session.sample_count = 1U;
	document.metadata = metadata;
	document.checkpoints.push_back(th03::formats::ReplayCheckpoint{
		.stage = 0x0FU,
		.round = 0U,
		.sample_index = 0U,
		.global_frame = 0U,
		.input_byte_cursor = 0U,
		.init = init,
	});
	return document;
}

std::vector<std::byte> read_file(const std::filesystem::path &path)
{
	std::ifstream input(path, std::ios::binary | std::ios::ate);
	if(!input) {
		return {};
	}
	const auto end = input.tellg();
	if(end < 0) {
		return {};
	}
	std::vector<std::byte> bytes(static_cast<std::size_t>(end));
	input.seekg(0);
	if(!bytes.empty()) {
		input.read(
			reinterpret_cast<char *>(bytes.data()),
			static_cast<std::streamsize>(bytes.size())
		);
	}
	return input ? bytes : std::vector<std::byte>{};
}

void write_malformed(const std::filesystem::path &path)
{
	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	const std::array<char, 3> bytes{'b', 'a', 'd'};
	output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

void test_atomic_slot_faults_and_legacy_cleanup(
	const std::filesystem::path &root,
	const th03::formats::ReplayDocument &base
)
{
	using th03::app::ReplaySlotStoreErrorCode;
	using th03::app::ReplaySlotStoreFaultPoint;
	using th03::app::ReplaySlotStoreTestAccess;

	check(
		th03::app::replay_fallback_path(root).filename() == "TH3LAST.RPY",
		"legacy cleanup recognizes the old native journal filename"
	);
	check(
		th03::app::commit_replay_slot(root, 6U, base).has_value(),
		"the direct slot fixture installs a validated destination"
	);
	const auto base_bytes = read_file(th03::app::replay_slot_path(root, 6U));
	check(
		!base_bytes.empty(),
		"the direct slot contains canonical V14 bytes"
	);

	auto replacement = base;
	replacement.metadata->name = {'F', 'A', 'U', 'L', 'T', 'E', 'D', '!'};
	const std::array faults{
		std::pair{ReplaySlotStoreFaultPoint::directory_create,
			ReplaySlotStoreErrorCode::directory_create},
		std::pair{ReplaySlotStoreFaultPoint::temporary_create,
			ReplaySlotStoreErrorCode::temporary_create},
		std::pair{ReplaySlotStoreFaultPoint::temporary_write,
			ReplaySlotStoreErrorCode::temporary_write},
		std::pair{ReplaySlotStoreFaultPoint::temporary_flush,
			ReplaySlotStoreErrorCode::temporary_flush},
		std::pair{ReplaySlotStoreFaultPoint::temporary_seek,
			ReplaySlotStoreErrorCode::temporary_seek},
		std::pair{ReplaySlotStoreFaultPoint::temporary_read,
			ReplaySlotStoreErrorCode::temporary_read},
		std::pair{ReplaySlotStoreFaultPoint::temporary_validation,
			ReplaySlotStoreErrorCode::temporary_validation},
		std::pair{ReplaySlotStoreFaultPoint::temporary_close,
			ReplaySlotStoreErrorCode::temporary_close},
		std::pair{ReplaySlotStoreFaultPoint::replace_target,
			ReplaySlotStoreErrorCode::replace_target},
	};
	for(const auto &[point, code] : faults) {
		ReplaySlotStoreTestAccess::fail_next(point);
		auto failed = th03::app::commit_replay_slot(root, 6U, replacement);
		check(
			!failed && failed.error().code == code,
			"the injected slot boundary returns its exact storage error"
		);
		check(
			read_file(th03::app::replay_slot_path(root, 6U)) == base_bytes,
			"every failed atomic commit preserves the occupied slot"
		);
	}

	check(
		th03::app::commit_replay_slot(root, 6U, replacement).has_value(),
		"a successful retry publishes the in-memory document directly"
	);
	const auto published = th03::app::load_replay_slot(root, 6U);
	check(
		published && published->document &&
		published->document->metadata->name == replacement.metadata->name,
		"successful publication exposes the validated numbered-slot artifact"
	);

	std::filesystem::copy_file(
		th03::app::replay_slot_path(root, 6U),
		th03::app::replay_fallback_path(root),
		std::filesystem::copy_options::overwrite_existing
	);
	check(
		std::filesystem::is_regular_file(th03::app::replay_fallback_path(root)),
		"legacy cleanup fixture stages an old-build TH3LAST"
	);
	ReplaySlotStoreTestAccess::fail_next(ReplaySlotStoreFaultPoint::remove_target);
	auto remove_failed = th03::app::discard_replay_fallback_on_startup(root);
	check(
		!remove_failed && remove_failed.error().code ==
			ReplaySlotStoreErrorCode::remove_target &&
		std::filesystem::is_regular_file(th03::app::replay_fallback_path(root)),
		"failed legacy cleanup reports the boundary and preserves the file"
	);
	check(
		th03::app::discard_replay_fallback_on_startup(root).has_value() &&
		!std::filesystem::exists(th03::app::replay_fallback_path(root)),
		"legacy cleanup retry removes TH3LAST"
	);
	check(
		th03::app::discard_replay_fallback_on_startup(root).has_value(),
		"legacy cleanup is idempotent when no old-build file exists"
	);

	write_malformed(th03::app::replay_fallback_path(root));
	check(
		th03::app::discard_replay_fallback_on_startup(root).has_value() &&
		!std::filesystem::exists(th03::app::replay_fallback_path(root)),
		"legacy cleanup removes malformed old-build bytes without parsing them"
	);
	ReplaySlotStoreTestAccess::clear_failure();
}

void test_local_netplay_view(
	const std::filesystem::path &root,
	const th03::formats::ReplayDocument &document
)
{
	using th03::app::ReplaySlotLocalOrigin;
	using th03::app::ReplaySlotLocalView;
	constexpr std::uint8_t slot = 10U;
	const ReplaySlotLocalView player_two{
		.origin = ReplaySlotLocalOrigin::netplay,
		.local_player = 1U,
	};
	auto netplay_document = document;
	netplay_document.session.init.game_mode = th03::game_mode::vs_1p_2p;
	netplay_document.session.init.is_cpu = {0U, 0U};
	netplay_document.checkpoints[0U].init = netplay_document.session.init;
	netplay_document.metadata->game_mode = th03::game_mode::vs_1p_2p;
	netplay_document.metadata->final_game_mode = th03::game_mode::vs_1p_2p;
	netplay_document.metadata->is_cpu = {0U, 0U};
	check(
		th03::app::commit_replay_slot(
			root, slot, netplay_document, player_two
		).has_value(),
		"a netplay slot publishes its local-player role in the replay"
	);
	netplay_document.metadata->recording_flags |=
		th03::formats::replay_recording_flag_netplay;
	netplay_document.metadata->recorder_role =
		th03::formats::ReplayRecorderRole::player_two;
	const auto canonical = th03::formats::write_v14_replay(netplay_document);
	check(
		canonical && read_file(th03::app::replay_slot_path(root, slot)) ==
			*canonical,
		"local presentation metadata is self-contained in canonical V14 bytes"
	);
	const auto loaded = th03::app::load_replay_slot(root, slot);
	check(
		loaded && loaded->document && loaded->local_view == player_two,
		"the V14 header restores the recording machine's P2 role"
	);

	auto replacement = document;
	replacement.metadata->name = {'D', 'I', 'G', 'E', 'S', 'T', '!', '!'};
	const auto replacement_bytes = th03::formats::write_v14_replay(replacement);
	check(
		replacement_bytes && th03::app::commit_exact_replay_file(
			th03::app::replay_slot_path(root, slot), *replacement_bytes
		).has_value(),
		"an exact ordinary replay can replace the netplay replay"
	);
	const auto stale = th03::app::load_replay_slot(root, slot);
	check(
		stale && stale->document && !stale->local_view,
		"catalog ownership follows the replacement replay itself"
	);

	check(
		th03::app::commit_replay_slot(root, slot, document).has_value(),
		"an ordinary overwrite replaces prior netplay ownership"
	);
	const auto ordinary = th03::app::load_replay_slot(root, slot);
	check(
		ordinary && ordinary->document && !ordinary->local_view,
		"an ordinary slot never inherits stale local-player metadata"
	);
	const auto before_invalid = read_file(th03::app::replay_slot_path(root, slot));
	const auto invalid = th03::app::commit_replay_slot(
		root, slot, document,
		ReplaySlotLocalView{
			.origin = ReplaySlotLocalOrigin::netplay,
			.local_player = 2U,
		}
	);
	check(
		!invalid && invalid.error().code ==
			th03::app::ReplaySlotStoreErrorCode::invalid_local_view &&
			read_file(th03::app::replay_slot_path(root, slot)) == before_invalid,
		"an invalid local role is rejected before replacing the replay"
	);
}

} // namespace

int main()
{
	const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
	const auto root = std::filesystem::temp_directory_path() /
		("th03-replay-slot-store-" + std::to_string(nonce));
	std::error_code ignored;
	std::filesystem::remove_all(root, ignored);

	auto document = make_document();
	check(
		th03::formats::write_v14_replay(document).has_value(),
		"storage fixture is a valid canonical V14 document"
	);
	check(
		th03::app::replay_slot_path(root, 7U).filename() == "TH3R07.RPY",
		"slot paths preserve the Replay Patch two-digit naming contract"
	);
	const auto missing = th03::app::load_replay_slot(root, 7U);
	check(missing && !missing->document, "a missing slot loads as empty");
	check(
		!th03::app::load_replay_slot({}, 0U),
		"an empty replay root is rejected"
	);
	check(
		!th03::app::load_replay_slot(root, 100U),
		"slot indices outside 00..99 are rejected"
	);
	test_atomic_slot_faults_and_legacy_cleanup(root, document);
	test_local_netplay_view(root, document);

	check(
		th03::app::commit_replay_slot(root, 7U, document).has_value(),
		"a canonical replay commits to an empty slot"
	);
	const auto first = th03::app::load_replay_slot(root, 7U);
	check(
		first && first->document &&
		first->document->metadata->random_seed == 0x12345678U,
		"a committed slot reads back as validated V14"
	);
	const auto first_bytes = read_file(th03::app::replay_slot_path(root, 7U));
	check(!first_bytes.empty(), "the committed slot contains replay bytes");

	document.metadata->name = {'R', 'E', 'P', 'L', 'A', 'C', 'E', 'D'};
	check(
		th03::app::commit_replay_slot(root, 7U, document).has_value(),
		"committing an occupied slot atomically replaces it"
	);
	const auto replaced = th03::app::load_replay_slot(root, 7U);
	check(
		replaced && replaced->document &&
		replaced->document->metadata->name == document.metadata->name,
		"replacement bytes retain their replay metadata"
	);
	const auto replaced_bytes = read_file(th03::app::replay_slot_path(root, 7U));
	check(
		replaced_bytes != first_bytes,
		"slot replacement publishes the new encoded artifact"
	);

	auto invalid = document;
	invalid.metadata.reset();
	check(
		!th03::app::commit_replay_slot(root, 7U, invalid),
		"an invalid document is rejected before replacement"
	);
	check(
		read_file(th03::app::replay_slot_path(root, 7U)) == replaced_bytes,
		"a failed commit preserves the prior slot byte-for-byte"
	);

	write_malformed(th03::app::replay_slot_path(root, 8U));
	const auto malformed = th03::app::load_replay_slot(root, 8U);
	check(
		!malformed && malformed.error().code ==
			th03::app::ReplaySlotStoreErrorCode::format,
		"malformed existing bytes fail as a format error"
	);
	check(
		th03::app::commit_replay_slot(root, 8U, document).has_value(),
		"a valid commit can replace a malformed existing slot"
	);

	std::filesystem::create_directories(th03::app::replay_slot_path(root, 9U));
	const auto nonregular = th03::app::load_replay_slot(root, 9U);
	check(
		!nonregular && nonregular.error().code ==
			th03::app::ReplaySlotStoreErrorCode::source_not_regular,
		"a slot path that is not a regular file is rejected"
	);

	std::filesystem::remove_all(root, ignored);
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0 ? 0 : 1;
}

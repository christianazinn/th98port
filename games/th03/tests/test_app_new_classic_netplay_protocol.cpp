#include "th03/app/new_classic/netplay/new_classic_netplay_protocol.hpp"
#include "th03/app/new_classic/netplay/new_classic_netplay_datagram.hpp"
#include "th03/app/new_classic/netplay/new_classic_netplay_lobby.hpp"
#include "th03/app/new_classic/netplay/new_classic_netplay_match_contract.hpp"
#include "th03/app/new_classic/netplay/new_classic_netplay_pause_barrier.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <span>
#include <string_view>

namespace {

std::size_t checks = 0U;
std::size_t failures = 0U;

void expect(bool condition, std::string_view message)
{
	++checks;
	if(!condition) {
		++failures;
		std::fprintf(stderr, "FAIL: %.*s\n",
			static_cast<int>(message.size()), message.data());
	}
}

void test_wire_vectors()
{
	using namespace th03::app;
	constexpr NewClassicNetplayOwnedInput input{
		.word = 0x0F25U,
		.charge = 1U,
		.control = NewClassicNetplayControl::pause_request,
	};
	const auto encoded = encode_new_classic_netplay_input(input);
	constexpr std::array<std::byte, 4U> golden{
		std::byte{0x25U}, std::byte{0x0FU},
		std::byte{0x01U}, std::byte{0x01U},
	};
	expect(encoded && *encoded == golden,
		"T3NET002 owned input has exact four-byte pause-marker vector");
	const auto decoded = decode_new_classic_netplay_input(golden);
	expect(decoded && *decoded == input,
		"T3NET002 owned input golden vector round-trips exactly");
	const auto truncated = decode_new_classic_netplay_input(
		std::span(golden).first<3U>()
	);
	expect(!truncated && truncated.error().code ==
		NewClassicNetplayInputErrorCode::invalid_wire_size,
		"owned input rejects truncated wire before reading");
}

void test_validation_and_composition()
{
	using namespace th03::app;
	constexpr NewClassicNetplayOwnedInput p1{
		.word = 0x0F21U,
		.charge = 1U,
	};
	constexpr NewClassicNetplayOwnedInput p2{
		.word = 0x0032U,
	};
	const auto combined = combine_new_classic_netplay_inputs(p1, p2);
	expect(combined && combined->p1 == p1.word &&
		combined->p2 == p2.word && combined->system == 0U &&
		combined->charge_mask == 0x01U,
		"independent player values compose with no source-global injection");

	th03::CoreInput retained{
		.p1 = 0x0001U,
		.p2 = 0x0002U,
		.system = 0x7F3FU,
		.charge_mask = 0x03U,
	};
	expect(apply_new_classic_netplay_input(
		retained, NewClassicNetplayPlayer::player_two,
		{.word = 0x0020U, .charge = 0U}
	) && retained.p1 == 0x0001U && retained.p2 == 0x0020U &&
		retained.system == 0x7F3FU && retained.charge_mask == 0x01U,
		"one owned player cannot overwrite the other player or system word");

	const auto invalid_word = encode_new_classic_netplay_input({
		.word = 0x8000U,
	});
	const auto invalid_charge = encode_new_classic_netplay_input({
		.charge = 2U,
	});
	const auto invalid_control = encode_new_classic_netplay_input({
		.control = static_cast<NewClassicNetplayControl>(2U),
	});
	expect(!invalid_word && invalid_word.error().code ==
		NewClassicNetplayInputErrorCode::invalid_player_word,
		"owned input rejects non-player logical bits");
	expect(!invalid_charge && invalid_charge.error().code ==
		NewClassicNetplayInputErrorCode::invalid_charge,
		"owned input rejects non-binary Charge");
	expect(!invalid_control && invalid_control.error().code ==
		NewClassicNetplayInputErrorCode::invalid_control,
		"owned input rejects unknown control markers");

	constexpr std::array<std::byte, 4U> malformed_control{
		std::byte{0U}, std::byte{0U}, std::byte{0U}, std::byte{2U},
	};
	const auto malformed = decode_new_classic_netplay_input(malformed_control);
	expect(!malformed && malformed.error().code ==
		NewClassicNetplayInputErrorCode::invalid_control,
		"wire decoder applies the same fail-closed semantic validation");

	const auto invalid_player = apply_new_classic_netplay_input(
		retained, static_cast<NewClassicNetplayPlayer>(2U), {}
	);
	expect(!invalid_player && invalid_player.error().code ==
		NewClassicNetplayInputErrorCode::invalid_player,
		"unknown player ownership cannot alias either side");
}

void test_datagram_contract()
{
	using namespace th03::app;
	expect(new_classic_netplay_proof_order(240U, 120U) ==
			NewClassicNetplayProofOrder::stale &&
		new_classic_netplay_proof_order(240U, 240U) ==
			NewClassicNetplayProofOrder::current &&
		new_classic_netplay_proof_order(240U, 360U) ==
			NewClassicNetplayProofOrder::future,
		"runtime digests and ACKs share one stale/current/future ordering");
	NewClassicNetplayDatagram input{
		.type = NewClassicNetplayDatagramType::input,
		.sender = NewClassicNetplayPlayer::player_two,
		.input_count = 2U,
		.session_id = 0x0102030405060708ULL,
		.contract_hash = 0x8877665544332211ULL,
		.latest_frame = 9U,
		.confirmed_frame_count = 7U,
	};
	input.inputs[0U] = {
		.word = 0x0F25U,
		.charge = 1U,
		.control = NewClassicNetplayControl::pause_request,
	};
	input.inputs[1U] = {.word = 0x0012U};
	const auto encoded = encode_new_classic_netplay_datagram(input);
	expect(encoded && encoded->size() == 104U &&
		(*encoded)[0U] == std::byte{'T'} &&
		(*encoded)[1U] == std::byte{'3'} &&
		(*encoded)[2U] == std::byte{'N'} &&
		(*encoded)[3U] == std::byte{'P'} &&
		(*encoded)[4U] == std::byte{7U} &&
		(*encoded)[5U] == std::byte{3U} &&
		(*encoded)[6U] == std::byte{1U} &&
		(*encoded)[7U] == std::byte{2U} &&
		(*encoded)[40U] == std::byte{0x25U} &&
		(*encoded)[41U] == std::byte{0x0FU} &&
		(*encoded)[43U] == std::byte{0x01U},
		"T3NP input datagram has frozen header and newest-first history bytes");
	if(encoded) {
		const auto decoded = decode_new_classic_netplay_datagram(*encoded);
		expect(decoded && decoded->type == input.type &&
			decoded->sender == input.sender &&
			decoded->input_count == input.input_count &&
			decoded->session_id == input.session_id &&
			decoded->contract_hash == input.contract_hash &&
			decoded->latest_frame == input.latest_frame &&
			decoded->confirmed_frame_count == input.confirmed_frame_count &&
			decoded->inputs[0U] == input.inputs[0U] &&
			decoded->inputs[1U] == input.inputs[1U],
			"T3NP datagram round-trips exact session, contract, and history");
		auto corrupt = *encoded;
		corrupt[0U] = std::byte{0U};
		const auto bad_magic = decode_new_classic_netplay_datagram(corrupt);
		expect(!bad_magic && bad_magic.error().code ==
			NewClassicNetplayDatagramErrorCode::invalid_magic,
			"unknown UDP traffic cannot enter the netplay protocol");
		corrupt = *encoded;
		corrupt[43U] = std::byte{2U};
		const auto bad_control = decode_new_classic_netplay_datagram(corrupt);
		expect(!bad_control && bad_control.error().code ==
			NewClassicNetplayDatagramErrorCode::invalid_input &&
			bad_control.error().input_index == 0U,
			"nested T3NET002 control bytes remain strict in UDP history");
	} else {
		expect(false, "valid datagram reaches decode checks");
		expect(false, "valid datagram reaches corruption checks");
		expect(false, "valid datagram reaches nested input checks");
	}
	NewClassicNetplayDatagram hello{
		.type = NewClassicNetplayDatagramType::hello,
		.sender = NewClassicNetplayPlayer::player_two,
		.contract_hash = 1U,
	};
	expect(encode_new_classic_netplay_datagram(hello).has_value(),
		"join hello is the sole session-zero packet");
	hello.session_id = 1U;
	const auto bad_hello = encode_new_classic_netplay_datagram(hello);
	expect(!bad_hello && bad_hello.error().code ==
		NewClassicNetplayDatagramErrorCode::invalid_session,
		"hello with a peer-selected session fails closed");
	auto bad = input;
	bad.input_count = 17U;
	const auto bad_count = encode_new_classic_netplay_datagram(bad);
	expect(!bad_count && bad_count.error().code ==
		NewClassicNetplayDatagramErrorCode::invalid_history,
		"history above the fixed redundant window fails closed");
	const NewClassicNetplayDatagram disconnect{
		.type = NewClassicNetplayDatagramType::disconnect,
		.sender = NewClassicNetplayPlayer::player_one,
		.session_id = 0x0102030405060708ULL,
		.contract_hash = 0x8877665544332211ULL,
	};
	const auto encoded_disconnect =
		encode_new_classic_netplay_datagram(disconnect);
	const auto decoded_disconnect = encoded_disconnect ?
		decode_new_classic_netplay_datagram(*encoded_disconnect) :
		std::expected<NewClassicNetplayDatagram,
			NewClassicNetplayDatagramError>{
				std::unexpected(NewClassicNetplayDatagramError{})
			};
	expect(encoded_disconnect && decoded_disconnect &&
		decoded_disconnect->type ==
			NewClassicNetplayDatagramType::disconnect &&
		decoded_disconnect->sender == disconnect.sender &&
		decoded_disconnect->session_id == disconnect.session_id &&
		decoded_disconnect->latest_frame == 0U &&
		decoded_disconnect->input_count == 0U,
		"intentional departure is a bounded state-free session control");

	NewClassicNetplayDatagram digest{
		.type = NewClassicNetplayDatagramType::simulation_digest,
		.sender = NewClassicNetplayPlayer::player_one,
		.session_id = 0x0102030405060708ULL,
		.contract_hash = 0x8877665544332211ULL,
		.latest_frame = 120U,
		.confirmed_frame_count = 120U,
		.simulation_digest_kind =
			NewClassicNetplaySimulationDigestKind::periodic,
	};
	for(std::size_t index = 0U;
		index < digest.simulation_digest.size(); ++index) {
		digest.simulation_digest[index] = static_cast<std::uint8_t>(index + 1U);
	}
	const auto encoded_digest = encode_new_classic_netplay_datagram(digest);
	expect(encoded_digest && (*encoded_digest)[5U] == std::byte{5U} &&
		(*encoded_digest)[24U] == std::byte{120U} &&
		(*encoded_digest)[32U] == std::byte{120U} &&
		(*encoded_digest)[40U] == std::byte{1U} &&
		(*encoded_digest)[71U] == std::byte{32U} &&
		(*encoded_digest)[72U] == std::byte{1U} &&
		(*encoded_digest)[73U] == std::byte{0U},
		"simulation digest reuses the bounded history payload exactly");
	const auto decoded_digest = encoded_digest ?
		decode_new_classic_netplay_datagram(*encoded_digest) :
		std::expected<NewClassicNetplayDatagram,
			NewClassicNetplayDatagramError>{
				std::unexpected(NewClassicNetplayDatagramError{})
			};
	expect(decoded_digest && decoded_digest->type == digest.type &&
		decoded_digest->latest_frame == 120U &&
		decoded_digest->confirmed_frame_count == 120U &&
		decoded_digest->simulation_digest == digest.simulation_digest,
		"simulation digest packet round-trips frame identity and SHA-256");
	digest.type = NewClassicNetplayDatagramType::simulation_digest_ack;
	const auto encoded_ack = encode_new_classic_netplay_datagram(digest);
	const auto decoded_ack = encoded_ack ?
		decode_new_classic_netplay_datagram(*encoded_ack) :
		std::expected<NewClassicNetplayDatagram,
			NewClassicNetplayDatagramError>{
				std::unexpected(NewClassicNetplayDatagramError{})
			};
	expect(decoded_ack && decoded_ack->type ==
		NewClassicNetplayDatagramType::simulation_digest_ack &&
		decoded_ack->latest_frame == digest.latest_frame &&
		decoded_ack->simulation_digest == digest.simulation_digest,
		"digest acknowledgement freezes the same bounded proof payload");
	digest.latest_frame = 0U;
	digest.confirmed_frame_count = 0U;
	const auto zero_frame_ack = encode_new_classic_netplay_datagram(digest);
	expect(!zero_frame_ack && zero_frame_ack.error().code ==
		NewClassicNetplayDatagramErrorCode::invalid_digest,
		"frame-zero proof acknowledgement is not a valid runtime message");
	digest.type = NewClassicNetplayDatagramType::simulation_digest;
	digest.simulation_digest_kind =
		NewClassicNetplaySimulationDigestKind::initial;
	const auto initial_digest = encode_new_classic_netplay_datagram(digest);
	expect(static_cast<bool>(initial_digest),
		"frame-zero digest keeps both proof identities zero");
	digest.simulation_digest = {};
	const auto zero_digest = encode_new_classic_netplay_datagram(digest);
	expect(!zero_digest && zero_digest.error().code ==
		NewClassicNetplayDatagramErrorCode::invalid_digest,
		"all-zero simulation identity fails closed");

	digest.simulation_digest[0U] = 1U;
	digest.latest_frame = 37U;
	digest.confirmed_frame_count = 23U;
	digest.simulation_digest_kind =
		NewClassicNetplaySimulationDigestKind::boundary;
	digest.boundary_proof = NewClassicNetplayBoundaryProof::round_transition;
	const auto boundary_digest = encode_new_classic_netplay_datagram(digest);
	const auto decoded_boundary = boundary_digest ?
		decode_new_classic_netplay_datagram(*boundary_digest) :
		std::expected<NewClassicNetplayDatagram,
			NewClassicNetplayDatagramError>{
				std::unexpected(NewClassicNetplayDatagramError{})
			};
	expect(boundary_digest && decoded_boundary &&
		decoded_boundary->simulation_digest_kind ==
			NewClassicNetplaySimulationDigestKind::boundary &&
		decoded_boundary->boundary_proof ==
			NewClassicNetplayBoundaryProof::round_transition &&
		(*boundary_digest)[72U] == std::byte{2U} &&
		(*boundary_digest)[73U] == std::byte{2U},
		"lifecycle proof freezes explicit kind and source boundary identity");
	expect(new_classic_netplay_boundary_proof(th03::StepStatus::running) ==
			NewClassicNetplayBoundaryProof::none &&
		new_classic_netplay_boundary_proof(
			th03::StepStatus::phase_transition
		) ==
			NewClassicNetplayBoundaryProof::phase_transition &&
		new_classic_netplay_boundary_proof(
			th03::StepStatus::round_transition
		) ==
			NewClassicNetplayBoundaryProof::round_transition &&
		new_classic_netplay_boundary_proof(th03::StepStatus::terminal) ==
			NewClassicNetplayBoundaryProof::terminal &&
		new_classic_netplay_boundary_proof(th03::StepStatus::paused) ==
			NewClassicNetplayBoundaryProof::pause,
		"wire boundary vocabulary is the exact MAIN lifecycle projection");
	if(boundary_digest) {
		auto corrupt = *boundary_digest;
		corrupt[74U] = std::byte{1U};
		const auto reserved = decode_new_classic_netplay_datagram(corrupt);
		expect(!reserved && reserved.error().code ==
			NewClassicNetplayDatagramErrorCode::invalid_digest,
			"digest-only tail remains reserved and fails closed");
	}
}

void test_pause_barrier()
{
	using namespace th03::app;
	constexpr std::uint64_t frame = 123U;
	constexpr std::uint64_t gameplay = 97U;
	const NewClassicNetplayDatagram waiting{
		.type = NewClassicNetplayDatagramType::pause_waiting,
		.sender = NewClassicNetplayPlayer::player_one,
		.session_id = 0x0102030405060708ULL,
		.contract_hash = 0x8877665544332211ULL,
		.latest_frame = frame,
		.confirmed_frame_count = gameplay,
		.boundary_proof = NewClassicNetplayBoundaryProof::pause,
	};
	const auto encoded = encode_new_classic_netplay_datagram(waiting);
	const auto decoded = encoded ? decode_new_classic_netplay_datagram(*encoded) :
		std::expected<NewClassicNetplayDatagram,
			NewClassicNetplayDatagramError>{
				std::unexpected(NewClassicNetplayDatagramError{})
			};
	expect(encoded && decoded && decoded->type == waiting.type &&
		decoded->latest_frame == frame &&
		decoded->confirmed_frame_count == gameplay &&
		decoded->boundary_proof == NewClassicNetplayBoundaryProof::pause,
		"Pause-waiting signal round-trips exact confirmed boundary identity");
	for(const auto type : std::array{
		NewClassicNetplayDatagramType::forfeit_request,
		NewClassicNetplayDatagramType::forfeit_ack,
	}) {
		auto signal = waiting;
		signal.type = type;
		const auto wire = encode_new_classic_netplay_datagram(signal);
		const auto round_trip = wire ?
			decode_new_classic_netplay_datagram(*wire) :
			std::expected<NewClassicNetplayDatagram,
				NewClassicNetplayDatagramError>{
					std::unexpected(NewClassicNetplayDatagramError{})
				};
		expect(round_trip && round_trip->type == type &&
			round_trip->latest_frame == frame &&
			round_trip->confirmed_frame_count == gameplay,
			"Forfeit control round-trips the exact confirmed Pause identity");
	}
	if(encoded) {
		auto corrupt = *encoded;
		corrupt[40U] = std::byte{1U};
		const auto payload = decode_new_classic_netplay_datagram(corrupt);
		expect(!payload && payload.error().code ==
			NewClassicNetplayDatagramErrorCode::invalid_history,
			"Pause signals reject hidden payload bytes");
		corrupt = *encoded;
		corrupt[73U] = std::byte{0U};
		const auto proof = decode_new_classic_netplay_datagram(corrupt);
		expect(!proof && proof.error().code ==
			NewClassicNetplayDatagramErrorCode::invalid_history,
			"Pause signals require the typed Pause boundary proof");
	}
	auto invalid = NewClassicNetplayPauseBarrier::create(0U, gameplay);
	expect(!invalid && invalid.error().code ==
		NewClassicNetplayPauseBarrierErrorCode::invalid_identity,
		"Pause ready barrier rejects the bootstrap identity");
	auto barrier = NewClassicNetplayPauseBarrier::create(frame, gameplay);
	expect(barrier && barrier->status() ==
		NewClassicNetplayPauseBarrierStatus::waiting_release,
		"Pause ready barrier starts behind the source release lock");
	if(!barrier) {
		return;
	}
	barrier->consume_local({.system = 0x1000U});
	expect(barrier->status() ==
		NewClassicNetplayPauseBarrierStatus::waiting_release,
		"held opening Esc cannot become Resume");
	barrier->consume_local({});
	barrier->consume_local({.p1 = 0x0020U});
	expect(barrier->status() ==
		NewClassicNetplayPauseBarrierStatus::waiting_local_ready &&
		barrier->outbound_signal() ==
			NewClassicNetplayDatagramType::pause_waiting,
		"non-Cancel input after release does not arm Resume");
	barrier->consume_local({.system = 0x1000U});
	expect(barrier->status() ==
		NewClassicNetplayPauseBarrierStatus::waiting_remote_ready &&
		barrier->outbound_signal() ==
			NewClassicNetplayDatagramType::resume_ready,
		"fresh Esc publishes Resume-ready without advancing source time");
	const auto wrong = barrier->receive(
		NewClassicNetplayDatagramType::resume_ready, frame + 1U, gameplay
	);
	expect(!wrong && wrong.error().code ==
		NewClassicNetplayPauseBarrierErrorCode::invalid_identity,
		"Resume-ready cannot cross Pause boundary identities");
	expect(barrier->receive(
		NewClassicNetplayDatagramType::pause_waiting, frame, gameplay
	).has_value() && !barrier->ready(),
		"peer waiting is a keepalive but not Resume consent");
	expect(barrier->receive(
		NewClassicNetplayDatagramType::resume_ready, frame, gameplay
	).has_value() && barrier->ready(),
		"two fresh Esc choices complete the ready barrier");

	auto lost = NewClassicNetplayPauseBarrier::create(frame, gameplay);
	if(lost) {
		lost->consume_local({});
		lost->consume_local({.system = 0x1000U});
		expect(lost->observe_peer_gameplay(frame).has_value() && lost->ready(),
			"first post-Resume gameplay closes a lost ready-beacon window");
	}
	auto premature = NewClassicNetplayPauseBarrier::create(frame, gameplay);
	if(premature) {
		const auto resumed = premature->observe_peer_gameplay(frame);
		expect(!resumed && resumed.error().code ==
			NewClassicNetplayPauseBarrierErrorCode::
				peer_resumed_before_local_ready,
			"peer gameplay cannot override the local Resume choice");
	}
	auto local_forfeit = NewClassicNetplayPauseBarrier::create(frame, gameplay);
	if(local_forfeit) {
		local_forfeit->choose_local_forfeit();
		expect(local_forfeit->status() ==
				NewClassicNetplayPauseBarrierStatus::local_forfeit_waiting &&
			local_forfeit->outbound_signal() ==
				NewClassicNetplayDatagramType::forfeit_request &&
			!local_forfeit->ready(),
			"local Forfeit supersedes Resume and retries a typed request");
		expect(local_forfeit->receive(
			NewClassicNetplayDatagramType::forfeit_ack, frame, gameplay
		).has_value() && local_forfeit->forfeit_acknowledged() &&
			local_forfeit->status() ==
				NewClassicNetplayPauseBarrierStatus::
					local_forfeit_acknowledged,
			"Forfeit acknowledgement completes the local losing route");
	}
	auto remote_forfeit = NewClassicNetplayPauseBarrier::create(frame, gameplay);
	if(remote_forfeit) {
		expect(remote_forfeit->receive(
			NewClassicNetplayDatagramType::forfeit_request, frame, gameplay
		).has_value() && remote_forfeit->remote_forfeit() &&
			remote_forfeit->status() ==
				NewClassicNetplayPauseBarrierStatus::remote_forfeit &&
			remote_forfeit->outbound_signal() ==
				NewClassicNetplayDatagramType::forfeit_ack,
			"remote Forfeit selects the local winning acknowledgement route");
	}
	auto mutual = NewClassicNetplayPauseBarrier::create(frame, gameplay);
	if(mutual) {
		mutual->choose_local_forfeit();
		expect(mutual->receive(
			NewClassicNetplayDatagramType::forfeit_request, frame, gameplay
		).has_value() && mutual->status() ==
			NewClassicNetplayPauseBarrierStatus::mutual_forfeit,
			"crossed Forfeit requests resolve to one mutual terminal state");
	}
	auto unsolicited_ack = NewClassicNetplayPauseBarrier::create(frame, gameplay);
	if(unsolicited_ack) {
		const auto ack = unsolicited_ack->receive(
			NewClassicNetplayDatagramType::forfeit_ack, frame, gameplay
		);
		expect(!ack && ack.error().code ==
			NewClassicNetplayPauseBarrierErrorCode::invalid_signal,
			"Forfeit acknowledgement without a local request fails closed");
	}
}

void test_match_contract()
{
	using namespace th03::app;
	const auto contract = create_new_classic_netplay_match_contract({
		.player_one = {.character = 1U, .palette = 1U},
		.player_one_autofire = true,
		.rank = 1U,
		.seed = 34U,
		.input_delay_frames = 4U,
	}, {
		.player_two = {.character = 2U, .palette = 2U},
	});
	const auto encoded = contract ?
		encode_new_classic_netplay_match_contract(*contract) :
		std::expected<
			std::array<std::byte, new_classic_netplay_match_contract_bytes>,
			NewClassicNetplayMatchContractError
		>{std::unexpected(NewClassicNetplayMatchContractError{})};
	constexpr std::array<std::byte, 64U> golden{
		std::byte{0x54U}, std::byte{0x33U}, std::byte{0x4EU},
		std::byte{0x43U}, std::byte{0x04U}, std::byte{0x01U},
		std::byte{0x01U}, std::byte{0x01U}, std::byte{0x02U},
		std::byte{0x02U}, std::byte{0x04U}, std::byte{0x01U},
		std::byte{0x22U}, std::byte{0x00U}, std::byte{0x00U},
		std::byte{0x00U}, std::byte{0xEBU}, std::byte{0x07U},
		std::byte{0x08U}, std::byte{0x00U}, std::byte{0x70U},
		std::byte{0x24U}, std::byte{0x00U}, std::byte{0x00U},
		std::byte{0x02U}, std::byte{0x00U}, std::byte{0x00U},
		std::byte{0x00U}, std::byte{0x00U}, std::byte{0x00U},
		std::byte{0x00U}, std::byte{0x00U},
	};
	const auto decoded = encoded ?
		decode_new_classic_netplay_match_contract(*encoded) :
		std::expected<NewClassicNetplayMatchContract,
			NewClassicNetplayMatchContractError>{
				std::unexpected(NewClassicNetplayMatchContractError{})
			};
	const auto hash = contract ?
		new_classic_netplay_match_contract_hash(*contract) :
		std::expected<std::uint64_t,
			NewClassicNetplayMatchContractError>{
				std::unexpected(NewClassicNetplayMatchContractError{})
			};
	expect(contract && contract->autofire_mask == 1U && encoded &&
		*encoded == golden && decoded && *decoded == *contract && hash &&
		*hash == 0xBCD25407931220AFULL,
		"T3NLC001 freezes host rules, choices, and asymmetric Autofire");
	const auto join_autofire = create_new_classic_netplay_match_contract({
		.player_one_autofire = false,
	}, {
		.player_two_autofire = true,
	});
	expect(join_autofire && join_autofire->autofire_mask == 2U,
		"the joining peer's Story Autofire maps only to the P2 lane");

	const auto invalid_host = create_new_classic_netplay_match_contract({
		.player_one = {.character = 0U, .palette = 1U},
	}, {});
	const auto invalid_join = create_new_classic_netplay_match_contract({}, {
		.player_two = {.character = 1U, .palette = 0U},
	});
	const auto invalid_delay = create_new_classic_netplay_match_contract({
		.input_delay_frames = 7U,
	}, {});
	AppSha256 different_identity{};
	different_identity.front() = 1U;
	const auto identity_mismatch = create_new_classic_netplay_match_contract({
		.identity_digest = different_identity,
	}, {});
	expect(!invalid_host && invalid_host.error().code ==
		NewClassicNetplayMatchContractErrorCode::invalid_host_lock &&
		!invalid_join && invalid_join.error().code ==
			NewClassicNetplayMatchContractErrorCode::invalid_join_lock &&
		!invalid_delay && invalid_delay.error().code ==
			NewClassicNetplayMatchContractErrorCode::invalid_input_delay &&
		!identity_mismatch && identity_mismatch.error().code ==
			NewClassicNetplayMatchContractErrorCode::identity_mismatch,
		"each peer's character lock fails in its own ownership domain");
	const auto palette_collision = create_new_classic_netplay_match_contract({
		.player_one = {.character = 4U, .palette = 2U},
	}, {
		.player_two = {.character = 4U, .palette = 2U},
	});
	expect(palette_collision &&
		palette_collision->player_one == NewClassicNetplayCharacterLock{
			.character = 4U, .palette = 2U
		} && palette_collision->player_two == NewClassicNetplayCharacterLock{
			.character = 4U, .palette = 1U
		}, "same-character collision gives P1 priority and flips P2's palette");
	if(encoded) {
		auto corrupt = *encoded;
		corrupt[4U] = std::byte{5U};
		const auto version = decode_new_classic_netplay_match_contract(corrupt);
		corrupt = *encoded;
		corrupt[24U] = std::byte{3U};
		const auto ruleset = decode_new_classic_netplay_match_contract(corrupt);
		corrupt = *encoded;
		corrupt[63U] = std::byte{1U};
		const auto reserved = decode_new_classic_netplay_match_contract(corrupt);
		expect(!version && version.error().code ==
			NewClassicNetplayMatchContractErrorCode::unsupported_version &&
			!ruleset && ruleset.error().code ==
				NewClassicNetplayMatchContractErrorCode::unsupported_ruleset &&
			!reserved && reserved.error().code ==
				NewClassicNetplayMatchContractErrorCode::nonzero_reserved,
			"match identity rejects unknown schema, ruleset, and reserved bytes");
	}
}

void test_lobby_negotiation()
{
	using namespace th03::app;
	expect(!new_classic_netplay_lobby_wait_expired(
			NewClassicNetplayLobbyRole::host, false, ~std::uint64_t{0U}
		) && !new_classic_netplay_lobby_wait_expired(
			NewClassicNetplayLobbyRole::join, true,
			new_classic_netplay_lobby_timeout_ms
		) && new_classic_netplay_lobby_wait_expired(
			NewClassicNetplayLobbyRole::join, true,
			new_classic_netplay_lobby_timeout_ms + 1U
		) && new_classic_netplay_lobby_wait_expired(
			NewClassicNetplayLobbyRole::host, true,
			new_classic_netplay_lobby_timeout_ms + 1U
		),
		"host waits indefinitely before a peer but all started exchanges time out");
	constexpr NewClassicNetplayHostLock host_lock{
		.player_one = {.character = 1U, .palette = 1U},
		.player_one_autofire = true,
		.rank = 1U,
		.seed = 34U,
		.input_delay_frames = 4U,
	};
	constexpr NewClassicNetplayJoinLock join_lock{
		.player_two = {.character = 2U, .palette = 2U},
	};
	constexpr std::uint64_t session = 0x0102030405060708ULL;
	auto host = NewClassicNetplayLobby::create_host(host_lock, session);
	auto join = NewClassicNetplayLobby::create_join(join_lock);
	expect(host && join && !host->outbound() && join->outbound() &&
		join->outbound()->type ==
			NewClassicNetplayLobbyDatagramType::join_request,
		"only the joiner initiates a lobby before a peer is known");

	const auto request = join ? join->outbound() : std::nullopt;
	const auto encoded_request = request ?
		encode_new_classic_netplay_lobby_datagram(*request) :
		std::expected<
			std::array<std::byte, new_classic_netplay_lobby_bytes>,
			NewClassicNetplayLobbyDatagramError
		>{std::unexpected(NewClassicNetplayLobbyDatagramError{})};
	expect(encoded_request && (*encoded_request)[0U] == std::byte{'T'} &&
		(*encoded_request)[3U] == std::byte{'L'} &&
		(*encoded_request)[4U] == std::byte{4U} &&
		(*encoded_request)[5U] == std::byte{1U} &&
		(*encoded_request)[32U] == std::byte{2U} &&
		(*encoded_request)[33U] == std::byte{2U},
		"T3NL request carries only the join-owned character lock");
	if(host && request) {
		expect(host->receive(*request).has_value(),
			"host accepts the first bounded join request");
	}
	const auto offer = host ? host->outbound() : std::nullopt;
	const auto encoded_offer = offer ?
		encode_new_classic_netplay_lobby_datagram(*offer) :
		std::expected<
			std::array<std::byte, new_classic_netplay_lobby_bytes>,
			NewClassicNetplayLobbyDatagramError
		>{std::unexpected(NewClassicNetplayLobbyDatagramError{})};
	constexpr std::array<std::byte, 96U> offer_golden{
		std::byte{0x54U}, std::byte{0x33U}, std::byte{0x4EU}, std::byte{0x4CU},
		std::byte{0x04U}, std::byte{0x02U}, std::byte{0x00U}, std::byte{0x00U},
		std::byte{0x08U}, std::byte{0x07U}, std::byte{0x06U}, std::byte{0x05U},
		std::byte{0x04U}, std::byte{0x03U}, std::byte{0x02U}, std::byte{0x01U},
		std::byte{0x01U}, std::byte{0x01U}, std::byte{0x01U}, std::byte{0x04U},
		std::byte{0x22U}, std::byte{0x00U}, std::byte{0x00U}, std::byte{0x00U},
		std::byte{0xEBU}, std::byte{0x07U}, std::byte{0x08U}, std::byte{0x00U},
		std::byte{0x70U}, std::byte{0x24U}, std::byte{0x00U}, std::byte{0x00U},
		std::byte{0x02U}, std::byte{0x02U}, std::byte{0x01U}, std::byte{0x00U},
		std::byte{0x00U}, std::byte{0x00U}, std::byte{0x00U}, std::byte{0x00U},
		std::byte{0xAFU}, std::byte{0x20U}, std::byte{0x12U}, std::byte{0x93U},
		std::byte{0x07U}, std::byte{0x54U}, std::byte{0xD2U}, std::byte{0xBCU},
		std::byte{0x00U}, std::byte{0x00U}, std::byte{0x00U}, std::byte{0x00U},
		std::byte{0x00U}, std::byte{0x00U}, std::byte{0x00U}, std::byte{0x00U},
		std::byte{0x00U}, std::byte{0x00U}, std::byte{0x00U}, std::byte{0x00U},
		std::byte{0x00U}, std::byte{0x00U}, std::byte{0x00U}, std::byte{0x00U},
	};
	const auto decoded_offer = encoded_offer ?
		decode_new_classic_netplay_lobby_datagram(*encoded_offer) :
		std::expected<NewClassicNetplayLobbyDatagram,
			NewClassicNetplayLobbyDatagramError>{
				std::unexpected(NewClassicNetplayLobbyDatagramError{})
			};
	expect(encoded_offer && *encoded_offer == offer_golden && decoded_offer &&
		decoded_offer->contract_hash == 0xBCD25407931220AFULL &&
		decoded_offer->host->player_one_autofire &&
		!decoded_offer->join->player_two_autofire,
		"T3NL offer freezes the full independently owned match vector");

	if(join && decoded_offer) {
		expect(join->receive(*decoded_offer).has_value(),
			"joiner derives the offered host rules against its own lock");
	}
	const auto ready = join ? join->outbound() : std::nullopt;
	if(host && ready) {
		expect(host->receive(*ready).has_value() &&
			host->status() == NewClassicNetplayLobbyStatus::ready,
			"host reaches ready only after the joiner echoes the exact contract");
	}
	const auto accepted = host ? host->outbound() : std::nullopt;
	if(join && accepted) {
		expect(join->receive(*accepted).has_value() &&
			join->status() == NewClassicNetplayLobbyStatus::ready &&
			join->session_id() == session && host->contract() &&
			join->contract() && *host->contract() == *join->contract(),
			"host acceptance leaves both peers on one exact ready contract");
	}

	if(host && request && ready) {
		expect(host->receive(*request).has_value() &&
			host->receive(*ready).has_value() && host->status() ==
				NewClassicNetplayLobbyStatus::ready,
			"duplicate request and ready retries are idempotent after acceptance");
	}
	if(join && offer && accepted) {
		expect(join->receive(*offer).has_value() &&
			join->receive(*accepted).has_value() && join->status() ==
				NewClassicNetplayLobbyStatus::ready,
			"duplicate offer and acceptance retries are idempotent");
	}

	if(offer) {
		auto changed = *offer;
		changed.host->seed = 35U;
		const auto changed_hash = create_new_classic_netplay_match_contract(
			*changed.host, *changed.join
		);
		if(changed_hash) {
			changed.contract_hash =
				new_classic_netplay_match_contract_hash(*changed_hash).value_or(0U);
		}
		const auto rejected = join->receive(changed);
		expect(!rejected && rejected.error().code ==
			NewClassicNetplayLobbyErrorCode::contract_mismatch,
			"a host cannot mutate its lock after the joiner accepted it");
		auto stale = *offer;
		++stale.session_id;
		const auto stale_result = join->receive(stale);
		expect(!stale_result && stale_result.error().code ==
			NewClassicNetplayLobbyErrorCode::invalid_session,
			"a stale or replacement session cannot enter an active lobby");
	}
	if(encoded_offer) {
		auto corrupt = *encoded_offer;
		corrupt[80U] = std::byte{1U};
		const auto reserved = decode_new_classic_netplay_lobby_datagram(corrupt);
		expect(!reserved && reserved.error().code ==
			NewClassicNetplayLobbyDatagramErrorCode::nonzero_reserved,
			"lobby wire rejects every unversioned extension byte");
		corrupt = *encoded_offer;
		corrupt[40U] ^= std::byte{1U};
		const auto mismatched = decode_new_classic_netplay_lobby_datagram(corrupt);
		expect(!mismatched && mismatched.error().code ==
			NewClassicNetplayLobbyDatagramErrorCode::invalid_contract,
			"lobby wire binds the full contract rather than trusting a hash field");
	}

	AppSha256 account_identity{};
	account_identity.front() = 0xA5U;
	account_identity.back() = 0x5AU;
	auto bound_host_lock = host_lock;
	bound_host_lock.identity_digest = account_identity;
	auto bound_join_lock = join_lock;
	bound_join_lock.identity_digest = account_identity;
	auto bound_host = NewClassicNetplayLobby::create_host(
		bound_host_lock, session
	);
	auto bound_join = NewClassicNetplayLobby::create_join(bound_join_lock);
	const auto bound_request = bound_join ? bound_join->outbound() : std::nullopt;
	if(bound_host && bound_join && bound_request) {
		expect(bound_host->receive(*bound_request).has_value(),
			"host accepts the same authenticated identity digest");
		const auto bound_offer = bound_host->outbound();
		const auto bound_wire = bound_offer ?
			encode_new_classic_netplay_lobby_datagram(*bound_offer) :
			std::expected<
				std::array<std::byte, new_classic_netplay_lobby_bytes>,
				NewClassicNetplayLobbyDatagramError
			>{std::unexpected(NewClassicNetplayLobbyDatagramError{})};
		expect(bound_offer && bound_wire && (*bound_wire)[48U] ==
			std::byte{0xA5U} && (*bound_wire)[79U] == std::byte{0x5AU} &&
			bound_join->receive(*bound_offer).has_value(),
			"T3NL carries and agrees on the complete T3NID001 digest");
	}
	auto wrong_join_lock = bound_join_lock;
	wrong_join_lock.identity_digest.front() ^= 1U;
	auto wrong_join = NewClassicNetplayLobby::create_join(wrong_join_lock);
	auto rejecting_host = NewClassicNetplayLobby::create_host(
		bound_host_lock, session
	);
	const auto wrong_request = wrong_join ? wrong_join->outbound() : std::nullopt;
	const auto mismatch = rejecting_host && wrong_request ?
		rejecting_host->receive(*wrong_request) :
		std::expected<void, NewClassicNetplayLobbyError>{};
	expect(rejecting_host && wrong_request && !mismatch,
		"different room or account snapshots fail before contract acceptance");
}

} // namespace

int main()
{
	test_wire_vectors();
	test_validation_and_composition();
	test_datagram_contract();
	test_pause_barrier();
	test_match_contract();
	test_lobby_negotiation();
	if(failures != 0U) {
		std::fprintf(stderr, "%zu of %zu checks failed\n", failures, checks);
		return 1;
	}
	std::printf("all %zu New Classic netplay input checks passed\n", checks);
	return 0;
}

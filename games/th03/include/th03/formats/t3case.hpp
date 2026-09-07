#ifndef TH03_FORMATS_T3CASE_HPP
#define TH03_FORMATS_T3CASE_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

#include "th03/error.hpp"
#include "th03/replay/session.hpp"

namespace th03::formats {

inline constexpr std::size_t t3case_header_size = 64;
inline constexpr std::size_t t3case_startup_size = 64;
inline constexpr std::size_t t3case_v11_snapshot_size = 935;
inline constexpr std::size_t t3case_record_size = 20;

enum class T3CaseSourceKind : std::uint8_t {
	master_direct = 1,
	normalized_v11 = 2,
	normalized_v12 = 3,
};

enum class T3CaseProducer : std::uint8_t {
	master_mod = 1,
	anniversary_mod = 2,
	host = 3,
};

enum class T3CaseRecordKind : std::uint8_t {
	input = 1,
	control = 2,
};

struct T3CaseHeader {
	std::uint16_t version = 0;
	std::uint32_t payload_offset = 0;
	std::uint32_t payload_size = 0;
	std::uint32_t sample_count = 0;
	std::uint32_t record_count = 0;
	T3CaseSourceKind source_kind = T3CaseSourceKind::master_direct;
	std::uint8_t input_semantics = 0;
	std::uint8_t ruleset_id = 0;
	std::uint8_t scenario_id = 0;
	replay::ProcessId first_process = replay::ProcessId::main;
	std::uint8_t producer = 0;
	std::uint16_t flags = 0;
	std::uint32_t case_id = 0;
	std::uint32_t source_digest = 0;
	std::uint32_t source_commit = 0;
	std::uint32_t payload_checksum = 0;
	std::uint32_t header_checksum = 0;
	std::uint32_t total_size = 0;
};

struct T3CaseRecord {
	T3CaseRecordKind kind = T3CaseRecordKind::input;
	std::uint8_t phase = 0;
	std::uint16_t round_or_result_frame = 0;
	std::uint32_t frame_index = 0;
	std::uint32_t round_frame = 0;
	std::uint16_t input_p1 = 0;
	std::uint16_t input_p2 = 0;
	std::uint16_t input_system = 0;
	std::uint16_t control = 0;
};

struct T3Case {
	T3CaseHeader header{};
	GameInit init{};
	std::vector<T3CaseRecord> records;
};

std::uint32_t fnv1a32(
	std::span<const std::byte> bytes,
	std::uint32_t seed = 0x811C9DC5U
) noexcept;

std::expected<T3Case, Error> parse_t3case(std::span<const std::byte> bytes);
std::expected<replay::ReplaySession, Error> make_replay_session(const T3Case &input);

} // namespace th03::formats

#endif

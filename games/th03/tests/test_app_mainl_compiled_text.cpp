#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <vector>

#include "content/diet_executable_internal.hpp"
#include "classic/mainl/mainl_compiled_text_internal.hpp"

namespace {

using th03::app::MainExecutableSha256;
using th03::app::MainlCompiledTextErrorCode;
using th03::app::MainlCompiledTextItem;
using th03::app::MainlExecutableProfile;
using th03::app::MainlStageTextImport;
using th03::app::detail::DietExecutableErrorCode;

class DietStreamBuilder final {
public:
	DietStreamBuilder()
	{
		append_control_word();
	}

	void bit(std::uint8_t value)
	{
		if(value != 0U) {
			auto control = std::to_integer<std::uint16_t>(
				bytes_[control_offset_]
			) | (static_cast<std::uint16_t>(
				std::to_integer<std::uint8_t>(bytes_[control_offset_ + 1U])
			) << 8U);
			control |= static_cast<std::uint16_t>(1U << bit_index_);
			bytes_[control_offset_] = static_cast<std::byte>(
				static_cast<std::uint8_t>(control)
			);
			bytes_[control_offset_ + 1U] = static_cast<std::byte>(
				static_cast<std::uint8_t>(control >> 8U)
			);
		}
		++bit_index_;
		if(bit_index_ == 16U) {
			// The decoder refills before the next byte-stream operand.
			append_control_word();
		}
	}

	void byte(std::uint8_t value)
	{
		bytes_.push_back(static_cast<std::byte>(value));
	}

	void literal(std::uint8_t value)
	{
		bit(1U);
		byte(value);
	}

	void short_match(std::size_t distance)
	{
		bit(0U);
		bit(0U);
		byte(static_cast<std::uint8_t>(256U - distance));
		bit(0U);
	}

	void long_two_byte_match(
		std::uint8_t a,
		std::uint8_t b,
		std::uint8_t c,
		std::uint8_t operand
	)
	{
		bit(0U);
		bit(0U);
		byte(operand);
		bit(1U);
		bit(a);
		bit(b);
		bit(c);
	}

	void variable_match(
		std::uint8_t distance_class,
		std::uint8_t a,
		std::uint8_t b,
		std::uint8_t c,
		std::uint8_t d,
		std::uint8_t operand,
		std::size_t length
	)
	{
		bit(0U);
		bit(1U);
		byte(operand);
		bit(a);
		if(distance_class == 0U) {
			bit(1U);
		} else {
			bit(0U);
			if(distance_class == 1U) {
				bit(1U);
			} else {
				bit(0U);
				bit(b);
				if(distance_class == 2U) {
					bit(1U);
				} else {
					bit(0U);
					bit(c);
					if(distance_class == 3U) {
						bit(1U);
					} else {
						bit(0U);
						bit(d);
					}
				}
			}
		}
		match_length(length);
	}

	void refresh()
	{
		bit(0U);
		bit(0U);
		byte(0xFFU);
		bit(0U);
		bit(1U);
	}

	void stop()
	{
		bit(0U);
		bit(0U);
		byte(0xFFU);
		bit(0U);
		bit(0U);
	}

	[[nodiscard]] std::vector<std::byte> take()
	{
		return std::move(bytes_);
	}

private:
	void append_control_word()
	{
		control_offset_ = bytes_.size();
		bytes_.push_back(std::byte{0U});
		bytes_.push_back(std::byte{0U});
		bit_index_ = 0U;
	}

	void match_length(std::size_t length)
	{
		if(length >= 3U && length <= 6U) {
			for(std::size_t index = 3U; index < length; ++index) {
				bit(0U);
			}
			bit(1U);
			return;
		}
		bit(0U);
		bit(0U);
		bit(0U);
		bit(0U);
		if(length <= 8U) {
			bit(1U);
			bit(static_cast<std::uint8_t>(length - 7U));
			return;
		}
		bit(0U);
		if(length <= 16U) {
			bit(0U);
			const auto value = length - 9U;
			bit(static_cast<std::uint8_t>((value >> 2U) & 1U));
			bit(static_cast<std::uint8_t>((value >> 1U) & 1U));
			bit(static_cast<std::uint8_t>(value & 1U));
			return;
		}
		bit(1U);
		byte(static_cast<std::uint8_t>(length - 17U));
	}

	std::vector<std::byte> bytes_{};
	std::size_t control_offset_ = 0U;
	std::uint8_t bit_index_ = 0U;
};

std::size_t checks = 0U;
std::size_t failures = 0U;

void expect(bool condition, const char *message)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << message << '\n';
	}
}

void write_u16(
	std::vector<std::byte> &bytes,
	std::size_t offset,
	std::uint16_t value
)
{
	bytes[offset] = static_cast<std::byte>(value & 0xFFU);
	bytes[offset + 1U] = static_cast<std::byte>(value >> 8U);
}

struct TextSpec final {
	MainlCompiledTextItem item;
	std::uint8_t character;
	std::size_t load_offset;
	std::size_t payload_bytes;
};

constexpr std::array<TextSpec, 18U> specs = {{
	{MainlCompiledTextItem::title, 0U, 0xE5C8U, 28U},
	{MainlCompiledTextItem::name, 0U, 0xE5E5U, 13U},
	{MainlCompiledTextItem::title, 1U, 0xE5F3U, 28U},
	{MainlCompiledTextItem::name, 1U, 0xE610U, 10U},
	{MainlCompiledTextItem::title, 2U, 0xE61BU, 28U},
	{MainlCompiledTextItem::name, 2U, 0xE638U, 15U},
	{MainlCompiledTextItem::title, 3U, 0xE648U, 28U},
	{MainlCompiledTextItem::name, 3U, 0xE665U, 10U},
	{MainlCompiledTextItem::title, 4U, 0xE670U, 28U},
	{MainlCompiledTextItem::name, 4U, 0xE68DU, 10U},
	{MainlCompiledTextItem::title, 5U, 0xE698U, 28U},
	{MainlCompiledTextItem::name, 5U, 0xE6B5U, 16U},
	{MainlCompiledTextItem::title, 6U, 0xE6C6U, 28U},
	{MainlCompiledTextItem::name, 6U, 0xE6E3U, 14U},
	{MainlCompiledTextItem::title, 7U, 0xE6F2U, 28U},
	{MainlCompiledTextItem::name, 7U, 0xE70FU, 15U},
	{MainlCompiledTextItem::title, 8U, 0xE71FU, 28U},
	{MainlCompiledTextItem::name, 8U, 0xE73CU, 13U},
}};

constexpr std::array<TextSpec, 10U> registration_specs = {{
	{MainlCompiledTextItem::registration_playchar, 0U, 0xED72U, 12U},
	{MainlCompiledTextItem::registration_playchar, 1U, 0xED7FU, 12U},
	{MainlCompiledTextItem::registration_playchar, 2U, 0xED8CU, 12U},
	{MainlCompiledTextItem::registration_playchar, 3U, 0xED99U, 12U},
	{MainlCompiledTextItem::registration_playchar, 4U, 0xEDA6U, 12U},
	{MainlCompiledTextItem::registration_playchar, 5U, 0xEDB3U, 12U},
	{MainlCompiledTextItem::registration_playchar, 6U, 0xEDC0U, 12U},
	{MainlCompiledTextItem::registration_playchar, 7U, 0xEDCDU, 12U},
	{MainlCompiledTextItem::registration_playchar, 8U, 0xEDDAU, 12U},
	{MainlCompiledTextItem::registration_playchar, 9U, 0xEDE7U, 12U},
}};

constexpr std::array<TextSpec, 15U> staff_specs = {{
	{MainlCompiledTextItem::staff_rank, 0U, 0xEFC2U, 11U},
	{MainlCompiledTextItem::staff_rank, 1U, 0xEFCEU, 13U},
	{MainlCompiledTextItem::staff_rank, 2U, 0xEFDCU, 11U},
	{MainlCompiledTextItem::staff_rank, 3U, 0xEFE8U, 14U},
	{MainlCompiledTextItem::staff_number, 0U, 0xEFF7U, 2U},
	{MainlCompiledTextItem::staff_number, 1U, 0xEFFAU, 2U},
	{MainlCompiledTextItem::staff_number, 2U, 0xEFFDU, 2U},
	{MainlCompiledTextItem::staff_number, 3U, 0xF000U, 2U},
	{MainlCompiledTextItem::staff_number, 4U, 0xF003U, 2U},
	{MainlCompiledTextItem::staff_number, 5U, 0xF006U, 2U},
	{MainlCompiledTextItem::staff_number, 6U, 0xF009U, 2U},
	{MainlCompiledTextItem::staff_number, 7U, 0xF00CU, 2U},
	{MainlCompiledTextItem::staff_number, 8U, 0xF00FU, 2U},
	{MainlCompiledTextItem::staff_number, 9U, 0xF012U, 2U},
	{MainlCompiledTextItem::staff_point, 0U, 0xF015U, 2U},
}};

constexpr std::size_t flake_masks_load_offset = 0xEE52U;

struct Fixture final {
	std::vector<std::byte> executable{};
	std::array<std::vector<std::uint8_t>, 18U> expected{};
	std::array<std::vector<std::uint8_t>, 10U> expected_registration{};
	std::array<std::vector<std::uint8_t>, 15U> expected_staff{};
	std::array<std::uint8_t, 64U> expected_flake_masks{};
};

Fixture fixture()
{
	Fixture result{};
	result.executable.assign(67372U, std::byte{0xCCU});
	write_u16(result.executable, 0U, 0x5A4DU);
	write_u16(result.executable, 2U, 300U);
	write_u16(result.executable, 4U, 132U);
	write_u16(result.executable, 6U, 0U);
	write_u16(result.executable, 8U, 256U);
	write_u16(result.executable, 24U, 28U);

	for(std::size_t index = 0U; index < specs.size(); ++index) {
		const auto &spec = specs[index];
		auto &expected = result.expected[index];
		expected.reserve(spec.payload_bytes);
		while(expected.size() + 1U < spec.payload_bytes) {
			expected.push_back(static_cast<std::uint8_t>(
				0x81U + (spec.character % 0x1FU)
			));
			expected.push_back(static_cast<std::uint8_t>(
				0x40U + ((index + expected.size()) % 0x3EU)
			));
		}
		if(expected.size() < spec.payload_bytes) {
			expected.push_back(static_cast<std::uint8_t>('A' + spec.character));
		}
		const auto file_offset = 4096U + spec.load_offset;
		for(std::size_t byte = 0U; byte < expected.size(); ++byte) {
			result.executable[file_offset + byte] =
				static_cast<std::byte>(expected[byte]);
		}
		result.executable[file_offset + expected.size()] = std::byte{0U};
	}
	for(std::size_t index = 0U; index < staff_specs.size(); ++index) {
		const auto &spec = staff_specs[index];
		auto &expected = result.expected_staff[index];
		expected.reserve(spec.payload_bytes);
		while(expected.size() + 1U < spec.payload_bytes) {
			expected.push_back(static_cast<std::uint8_t>(
				0x81U + (index % 0x1FU)
			));
			expected.push_back(static_cast<std::uint8_t>(
				0x40U + ((index + expected.size()) % 0x3EU)
			));
		}
		if(expected.size() < spec.payload_bytes) {
			expected.push_back(static_cast<std::uint8_t>('a' + index));
		}
		const auto file_offset = 4096U + spec.load_offset;
		for(std::size_t byte = 0U; byte < expected.size(); ++byte) {
			result.executable[file_offset + byte] =
				static_cast<std::byte>(expected[byte]);
		}
		result.executable[file_offset + expected.size()] = std::byte{0U};
	}
	for(std::size_t index = 0U; index < registration_specs.size(); ++index) {
		const auto &spec = registration_specs[index];
		auto &expected = result.expected_registration[index];
		for(std::size_t pair = 0U; pair < 6U; ++pair) {
			expected.push_back(static_cast<std::uint8_t>(0x81U + index));
			expected.push_back(static_cast<std::uint8_t>(0x40U + pair));
		}
		const auto file_offset = 4096U + spec.load_offset;
		for(std::size_t byte = 0U; byte < expected.size(); ++byte) {
			result.executable[file_offset + byte] =
				static_cast<std::byte>(expected[byte]);
		}
		result.executable[file_offset + expected.size()] = std::byte{0U};
	}
	for(std::size_t index = 0U; index < result.expected_flake_masks.size();
		++index) {
		result.expected_flake_masks[index] = static_cast<std::uint8_t>(
			(index * 37U) & 0xFFU
		);
		result.executable[4096U + flake_masks_load_offset + index] =
			static_cast<std::byte>(result.expected_flake_masks[index]);
	}
	return result;
}

std::expected<MainlStageTextImport, th03::app::MainlCompiledTextError>
import_fixture(std::vector<std::byte> &bytes)
{
	const auto digest = th03::app::main_executable_sha256(bytes);
	return th03::app::detail::import_mainl_stage_text_for_test(bytes, digest);
}

void test_success_and_ownership()
{
	auto source = fixture();
	auto imported = import_fixture(source.executable);
	expect(imported.has_value(), "synthetic authenticated profile imports");
	if(!imported) {
		return;
	}
	expect(
		imported->profile == MainlExecutableProfile::unpacked_original_japanese,
		"profile identity is retained"
	);
	std::fill(source.executable.begin(), source.executable.end(), std::byte{0U});
	for(std::size_t index = 0U; index < specs.size(); ++index) {
		const auto &spec = specs[index];
		const auto &character = imported->assets.characters[spec.character];
		const auto &actual = spec.item == MainlCompiledTextItem::title ?
			character.title : character.name;
		expect(actual == source.expected[index], "exact owning text payload");
	}
	for(std::size_t index = 0U; index < staff_specs.size(); ++index) {
		const auto &spec = staff_specs[index];
		const std::vector<std::uint8_t> *actual = &imported->staff.point;
		if(spec.item == MainlCompiledTextItem::staff_rank) {
			actual = &imported->staff.ranks[spec.character];
		} else if(spec.item == MainlCompiledTextItem::staff_number) {
			actual = &imported->staff.numbers[spec.character];
		}
		expect(
			*actual == source.expected_staff[index],
			"exact owning verdict payload"
		);
	}
	for(std::size_t index = 0U; index < registration_specs.size(); ++index) {
		expect(
			imported->assets.registration_playchars[index] ==
				source.expected_registration[index],
			"exact owning registration playchar payload"
		);
	}
	expect(
		imported->staff.flake_masks == source.expected_flake_masks,
		"exact owning flake mask payload"
	);
}

void test_container_and_profile_rejection()
{
	{
		std::array<std::byte, 27U> short_input{};
		auto imported = th03::app::import_mainl_stage_text(short_input);
		expect(
			!imported && imported.error().code ==
				MainlCompiledTextErrorCode::input_too_short,
			"short input is named"
		);
	}
	{
		auto source = fixture();
		source.executable[0U] = std::byte{'N'};
		auto imported = import_fixture(source.executable);
		expect(
			!imported && imported.error().code ==
				MainlCompiledTextErrorCode::invalid_mz_signature,
			"MZ signature is checked before profile"
		);
	}
	{
		auto source = fixture();
		const auto &spec = registration_specs.back();
		const auto terminator = 4096U + spec.load_offset + spec.payload_bytes;
		source.executable[terminator] = std::byte{'X'};
		auto imported = import_fixture(source.executable);
		expect(
			!imported && imported.error().code ==
				MainlCompiledTextErrorCode::malformed_source_text &&
				imported.error().item ==
					MainlCompiledTextItem::registration_playchar &&
				imported.error().character_index == 9U &&
				imported.error().offset == terminator,
			"late registration label terminator is precisely named"
		);
	}
	{
		auto source = fixture();
		write_u16(source.executable, 8U, 0U);
		auto imported = import_fixture(source.executable);
		expect(
			!imported && imported.error().code ==
				MainlCompiledTextErrorCode::invalid_mz_layout,
			"MZ header size is validated"
		);
	}
	{
		auto source = fixture();
		const auto accepted = th03::app::main_executable_sha256(source.executable);
		source.executable[100U] ^= std::byte{1U};
		auto imported = th03::app::detail::import_mainl_stage_text_for_test(
			source.executable, accepted
		);
		expect(
			!imported && imported.error().code ==
				MainlCompiledTextErrorCode::unsupported_profile,
			"whole-file mutation is rejected"
		);
	}
}

void test_text_validation()
{
	{
		auto source = fixture();
		const auto &spec = specs[17U];
		const auto terminator = 4096U + spec.load_offset + spec.payload_bytes;
		source.executable[terminator] = std::byte{'X'};
		auto imported = import_fixture(source.executable);
		expect(
			!imported && imported.error().code ==
				MainlCompiledTextErrorCode::malformed_source_text &&
				imported.error().item == MainlCompiledTextItem::name &&
				imported.error().character_index == 8U &&
				imported.error().offset == terminator,
			"late missing terminator is precisely named"
		);
	}
	{
		auto source = fixture();
		const auto &spec = specs[0U];
		const auto malformed = 4096U + spec.load_offset + 1U;
		source.executable[malformed] = std::byte{0x7FU};
		auto imported = import_fixture(source.executable);
		expect(
			!imported && imported.error().code ==
				MainlCompiledTextErrorCode::malformed_source_text &&
				imported.error().item == MainlCompiledTextItem::title &&
				imported.error().character_index == 0U &&
				imported.error().offset == malformed,
			"invalid Shift-JIS trail is precisely named"
		);
	}
	{
		auto source = fixture();
		const auto &spec = specs[4U];
		const auto malformed = 4096U + spec.load_offset + spec.payload_bytes - 1U;
		source.executable[malformed - 1U] = std::byte{'A'};
		source.executable[malformed] = std::byte{0x81U};
		auto imported = import_fixture(source.executable);
		expect(
			!imported && imported.error().code ==
				MainlCompiledTextErrorCode::malformed_source_text &&
				imported.error().offset == malformed,
			"dangling Shift-JIS lead is rejected"
		);
	}
	{
		auto source = fixture();
		const auto &spec = staff_specs.back();
		const auto terminator = 4096U + spec.load_offset + spec.payload_bytes;
		source.executable[terminator] = std::byte{'X'};
		auto imported = import_fixture(source.executable);
		expect(
			!imported && imported.error().code ==
				MainlCompiledTextErrorCode::malformed_source_text &&
				imported.error().item == MainlCompiledTextItem::staff_point &&
				imported.error().offset == terminator,
			"late verdict terminator is precisely named"
		);
	}
}

void append_expected_match(
	std::vector<std::byte> &expected,
	std::size_t distance,
	std::size_t length
)
{
	const auto source = expected.size() - distance;
	for(std::size_t index = 0U; index < length; ++index) {
		expected.push_back(expected[source + index]);
	}
}

void test_diet_stream_grammar()
{
	DietStreamBuilder builder;
	std::vector<std::byte> expected{};
	for(std::size_t index = 0U; index < 272U; ++index) {
		const auto value = static_cast<std::uint8_t>(index & 0xFFU);
		builder.literal(value);
		expected.push_back(static_cast<std::byte>(value));
	}
	for(std::size_t index = 0U; index < 16U; ++index) {
		builder.variable_match(0U, 1U, 0U, 0U, 0U, 0U, 256U);
		append_expected_match(expected, 256U, 256U);
	}
	builder.short_match(2U);
	append_expected_match(expected, 2U, 2U);
	builder.long_two_byte_match(0U, 0U, 0U, 0U);
	append_expected_match(expected, 2304U, 2U);

	struct DistanceCase final {
		std::uint8_t distance_class;
		std::uint8_t a;
		std::uint8_t b;
		std::uint8_t c;
		std::uint8_t d;
		std::uint8_t operand;
		std::size_t distance;
	};
	constexpr std::array distance_cases{
		DistanceCase{0U, 0U, 0U, 0U, 0U, 240U, 272U},
		DistanceCase{1U, 1U, 0U, 0U, 0U, 168U, 600U},
		DistanceCase{2U, 1U, 1U, 0U, 0U, 244U, 1036U},
		DistanceCase{3U, 1U, 1U, 1U, 0U, 252U, 2052U},
		DistanceCase{4U, 1U, 1U, 1U, 1U, 251U, 4101U},
	};
	for(const auto &test : distance_cases) {
		builder.variable_match(
			test.distance_class,
			test.a,
			test.b,
			test.c,
			test.d,
			test.operand,
			3U
		);
		append_expected_match(expected, test.distance, 3U);
	}
	constexpr std::array<std::size_t, 10U> lengths{
		3U, 4U, 5U, 6U, 7U, 8U, 9U, 16U, 17U, 272U,
	};
	for(const auto length : lengths) {
		builder.variable_match(0U, 0U, 0U, 0U, 0U, 240U, length);
		append_expected_match(expected, 272U, length);
	}
	builder.refresh();
	builder.stop();
	auto stream = builder.take();
	auto decoded = th03::app::detail::decode_diet_stream_for_test(
		stream, expected.size(), true
	);
	expect(
		decoded && decoded->idata == expected &&
			decoded->consumed_bytes == stream.size() &&
			decoded->segment_refreshes == 1U,
		"bounded DIET stream covers all distance/length/refill/refresh paths"
	);
}

void test_diet_stream_rejections()
{
	{
		std::array<std::byte, 1U> truncated{};
		auto decoded = th03::app::detail::decode_diet_stream_for_test(
			truncated, 1U, false
		);
		expect(
			!decoded && decoded.error().code ==
				DietExecutableErrorCode::truncated_stream,
			"truncated initial control word is rejected"
		);
	}
	{
		DietStreamBuilder invalid;
		invalid.short_match(2U);
		invalid.stop();
		auto decoded = th03::app::detail::decode_diet_stream_for_test(
			invalid.take(), 8U, false
		);
		expect(
			!decoded && decoded.error().code ==
				DietExecutableErrorCode::invalid_stream,
			"back-reference before output is rejected"
		);
	}
	{
		DietStreamBuilder overlap;
		overlap.literal('A');
		overlap.variable_match(0U, 1U, 0U, 0U, 0U, 255U, 3U);
		overlap.stop();
		auto decoded = th03::app::detail::decode_diet_stream_for_test(
			overlap.take(), 8U, false
		);
		expect(
			!decoded && decoded.error().code ==
				DietExecutableErrorCode::invalid_stream,
			"source-unused overlapping DIET match is rejected"
		);
	}
	{
		DietStreamBuilder capped;
		capped.literal('A');
		capped.literal('B');
		capped.stop();
		auto decoded = th03::app::detail::decode_diet_stream_for_test(
			capped.take(), 1U, false
		);
		expect(
			!decoded && decoded.error().code ==
				DietExecutableErrorCode::output_limit_exceeded,
			"DIET output cap is enforced before append"
		);
	}
	{
		DietStreamBuilder refresh;
		refresh.refresh();
		refresh.stop();
		auto decoded = th03::app::detail::decode_diet_stream_for_test(
			refresh.take(), 1U, false
		);
		expect(
			!decoded && decoded.error().code ==
				DietExecutableErrorCode::invalid_stream,
			"segment refresh requires its authenticated header flag"
		);
	}
	{
		DietStreamBuilder tailed;
		tailed.literal('A');
		tailed.stop();
		auto bytes = tailed.take();
		bytes.push_back(std::byte{0xCCU});
		auto decoded = th03::app::detail::decode_diet_stream_for_test(
			bytes, 1U, false
		);
		expect(
			!decoded && decoded.error().code ==
				DietExecutableErrorCode::invalid_stream,
			"stream stop must consume exactly the declared compressed span"
		);
	}
}

std::uint16_t test_crc16_arc(std::span<const std::byte> bytes)
{
	std::uint16_t crc = 0U;
	for(const auto value : bytes) {
		crc ^= std::to_integer<std::uint8_t>(value);
		for(std::uint8_t bit = 0U; bit < 8U; ++bit) {
			crc = ((crc & 1U) != 0U) ?
				static_cast<std::uint16_t>((crc >> 1U) ^ 0xA001U) :
				static_cast<std::uint16_t>(crc >> 1U);
		}
	}
	return crc;
}

struct PackedFixture final {
	std::vector<std::byte> packed{};
	std::vector<std::byte> unpacked{};
	th03::app::detail::DietExecutableProfile profile{};
};

PackedFixture packed_fixture(
	bool with_relocation = false,
	bool invalid_guest_signature = false,
	bool truncated_relocation = false
)
{
	PackedFixture result{};
	result.unpacked.assign(48U, std::byte{0U});
	write_u16(result.unpacked, 0U, 0x5A4DU);
	write_u16(result.unpacked, 2U, 48U);
	write_u16(result.unpacked, 4U, 1U);
	write_u16(result.unpacked, 6U, with_relocation ? 1U : 0U);
	write_u16(result.unpacked, 8U, 2U);
	write_u16(result.unpacked, 24U, 28U);
	if(with_relocation) {
		write_u16(result.unpacked, 28U, 0x1234U);
		write_u16(result.unpacked, 30U, 0x0001U);
	}
	for(std::size_t index = 32U; index < result.unpacked.size(); ++index) {
		result.unpacked[index] = static_cast<std::byte>(index);
	}

	std::vector<std::byte> idata(
		result.unpacked.begin() + 32, result.unpacked.end()
	);
	idata.insert(idata.end(), result.unpacked.begin(), result.unpacked.begin() + 28);
	if(with_relocation) {
		idata.insert(idata.end(), result.unpacked.begin() + 30,
			result.unpacked.begin() + 32);
		idata.insert(idata.end(), result.unpacked.begin() + 28,
			result.unpacked.begin() + 30);
	} else {
		idata.insert(idata.end(), result.unpacked.begin() + 28,
			result.unpacked.begin() + 32);
	}
	if(invalid_guest_signature) {
		idata[16U] = std::byte{'N'};
	}
	if(truncated_relocation) {
		idata.pop_back();
	}

	DietStreamBuilder stream_builder;
	for(const auto value : idata) {
		stream_builder.literal(std::to_integer<std::uint8_t>(value));
	}
	stream_builder.stop();
	auto stream = stream_builder.take();
	constexpr std::size_t compressed_offset = 0x77U;
	result.packed.assign(
		compressed_offset + stream.size() + 3U, std::byte{0U}
	);
	write_u16(result.packed, 0U, 0x5A4DU);
	const auto pages = (result.packed.size() + 511U) / 512U;
	const auto final_page = result.packed.size() % 512U;
	write_u16(result.packed, 2U, static_cast<std::uint16_t>(final_page));
	write_u16(result.packed, 4U, static_cast<std::uint16_t>(pages));
	write_u16(result.packed, 6U, 0U);
	write_u16(result.packed, 8U, 2U);
	write_u16(result.packed, 20U, 0U);
	write_u16(result.packed, 22U, 0U);
	write_u16(result.packed, 24U, 28U);
	write_u16(result.packed, 0x3AU, 1U);
	constexpr std::array<std::uint8_t, 16U> fingerprint{
		0x8EU, 0xDBU, 0x8EU, 0xC0U, 0x33U, 0xF6U, 0x33U, 0xFFU,
		0xB9U, 0x08U, 0x00U, 0xF3U, 0xA5U, 0x4BU, 0x48U, 0x4AU,
	};
	for(std::size_t index = 0U; index < fingerprint.size(); ++index) {
		result.packed[0x4DU + index] = static_cast<std::byte>(fingerprint[index]);
	}
	result.packed[0x6CU] = std::byte{'d'};
	result.packed[0x6DU] = std::byte{'l'};
	result.packed[0x6EU] = std::byte{'z'};
	result.packed[0x6FU] = static_cast<std::byte>(
		0x20U | ((stream.size() >> 16U) & 0x0FU)
	);
	write_u16(result.packed, 0x70U,
		static_cast<std::uint16_t>(stream.size() & 0xFFFFU));
	write_u16(result.packed, 0x72U, test_crc16_arc(stream));
	result.packed[0x74U] = std::byte{0U};
	write_u16(result.packed, 0x75U,
		static_cast<std::uint16_t>(result.unpacked.size()));
	std::copy(stream.begin(), stream.end(), result.packed.begin() + compressed_offset);
	result.packed[result.packed.size() - 3U] = std::byte{0xA1U};
	result.packed[result.packed.size() - 2U] = std::byte{0xB2U};
	result.packed[result.packed.size() - 1U] = std::byte{0xC3U};
	result.profile = {
		.packed_file_bytes = result.packed.size(),
		.packed_sha256 = th03::app::main_executable_sha256(result.packed),
		.unpacked_file_bytes = result.unpacked.size(),
		.unpacked_sha256 = th03::app::main_executable_sha256(result.unpacked),
	};
	return result;
}

void test_diet_executable_reconstruction()
{
	{
		auto fixture = packed_fixture();
		fixture.profile.packed_sha256 = {};
		auto decoded = th03::app::detail::decode_diet_145f_executable(
			fixture.packed, fixture.profile
		);
		expect(
			!decoded && decoded.error().code ==
				DietExecutableErrorCode::unsupported_profile,
			"whole packed digest authenticates before DIET-private reads"
		);
	}
	for(const bool with_relocation : {false, true}) {
		auto fixture = packed_fixture(with_relocation);
		auto decoded = th03::app::detail::decode_diet_145f_executable(
			fixture.packed, fixture.profile
		);
		expect(
			decoded && *decoded == fixture.unpacked,
			"bounded DIET EXE reconstruction preserves MZ/custom/relocation/code"
		);
	}
	{
		auto fixture = packed_fixture();
		fixture.packed[0x72U] ^= std::byte{1U};
		fixture.profile.packed_sha256 =
			th03::app::main_executable_sha256(fixture.packed);
		auto decoded = th03::app::detail::decode_diet_145f_executable(
			fixture.packed, fixture.profile
		);
		expect(
			!decoded && decoded.error().code ==
				DietExecutableErrorCode::crc_mismatch,
			"authenticated packed CRC mismatch is named"
		);
	}
	{
		auto fixture = packed_fixture();
		fixture.packed[0x4DU] ^= std::byte{1U};
		fixture.profile.packed_sha256 =
			th03::app::main_executable_sha256(fixture.packed);
		auto decoded = th03::app::detail::decode_diet_145f_executable(
			fixture.packed, fixture.profile
		);
		expect(
			!decoded && decoded.error().code ==
				DietExecutableErrorCode::invalid_diet_header,
			"authenticated non-1.45f fingerprint is rejected"
		);
	}
	{
		auto fixture = packed_fixture();
		fixture.profile.unpacked_sha256 = {};
		auto decoded = th03::app::detail::decode_diet_145f_executable(
			fixture.packed, fixture.profile
		);
		expect(
			!decoded && decoded.error().code ==
				DietExecutableErrorCode::reconstructed_profile_mismatch,
			"reconstructed whole-file digest is independently required"
		);
	}
	{
		auto fixture = packed_fixture(false, true);
		auto decoded = th03::app::detail::decode_diet_145f_executable(
			fixture.packed, fixture.profile
		);
		expect(
			!decoded && decoded.error().code ==
				DietExecutableErrorCode::invalid_reconstructed_mz,
			"invalid intermediate guest MZ is rejected"
		);
	}
	{
		auto fixture = packed_fixture(true, false, true);
		auto decoded = th03::app::detail::decode_diet_145f_executable(
			fixture.packed, fixture.profile
		);
		expect(
			!decoded && decoded.error().code ==
				DietExecutableErrorCode::invalid_relocation_table,
			"truncated encoded relocation table is rejected"
		);
	}
}

std::vector<std::byte> read_file(const std::filesystem::path &path)
{
	std::ifstream input(path, std::ios::binary);
	const std::vector<char> raw{
		std::istreambuf_iterator<char>(input),
		std::istreambuf_iterator<char>()
	};
	std::vector<std::byte> result{};
	result.reserve(raw.size());
	for(const auto value : raw) {
		result.push_back(static_cast<std::byte>(
			static_cast<unsigned char>(value)
		));
	}
	return result;
}

void optional_local_gate(const char *path)
{
	auto bytes = read_file(path);
	auto imported = th03::app::import_mainl_stage_text(bytes);
	expect(imported.has_value(), "local authenticated MAINL profile imports");
	if(!imported) {
		return;
	}
	const auto expected_profile = (bytes.size() == 37975U) ?
		MainlExecutableProfile::packed_original_japanese :
		MainlExecutableProfile::unpacked_original_japanese;
	expect(
		imported->profile == expected_profile,
		"local MAINL caller profile identity is retained"
	);
	for(std::size_t index = 0U; index < imported->assets.characters.size(); ++index) {
		expect(
			imported->assets.characters[index].title.size() == 28U,
			"local title length matches source table"
		);
		expect(
			!imported->assets.characters[index].name.empty(),
			"local name is nonempty"
		);
	}
	for(const auto &rank : imported->staff.ranks) {
		expect(!rank.empty(), "local verdict rank is nonempty");
	}
	for(const auto &playchar : imported->assets.registration_playchars) {
		expect(
			playchar.size() == 12U,
			"local registration playchar is exactly six fullwidth cells"
		);
	}
	for(const auto &number : imported->staff.numbers) {
		expect(number.size() == 2U, "local verdict number is one fullwidth glyph");
	}
	expect(
		imported->staff.point.size() == 2U,
		"local verdict point is one fullwidth glyph"
	);
	expect(
		std::any_of(
			imported->staff.flake_masks.begin(),
			imported->staff.flake_masks.end(),
			[](std::uint8_t value) { return value != 0U; }
		),
		"local flake mask table is populated"
	);
}

void optional_local_equivalence(const char *first_path, const char *second_path)
{
	auto first = th03::app::import_mainl_stage_text(read_file(first_path));
	auto second = th03::app::import_mainl_stage_text(read_file(second_path));
	expect(first.has_value() && second.has_value(),
		"packed and unpacked MAINL profiles both authenticate");
	if(!first || !second) {
		return;
	}
	bool equal = first->assets.registration_playchars ==
		second->assets.registration_playchars &&
		first->staff.ranks == second->staff.ranks &&
		first->staff.numbers == second->staff.numbers &&
		first->staff.point == second->staff.point &&
		first->staff.flake_masks == second->staff.flake_masks;
	for(std::size_t index = 0U; index < first->assets.characters.size(); ++index) {
		equal = equal && first->assets.characters[index].title ==
			second->assets.characters[index].title &&
			first->assets.characters[index].name ==
			second->assets.characters[index].name;
	}
	expect(equal, "packed and unpacked MAINL imports are byte-identical");
}

} // namespace

int main(int argc, char **argv)
{
	test_success_and_ownership();
	test_container_and_profile_rejection();
	test_text_validation();
	test_diet_stream_grammar();
	test_diet_stream_rejections();
	test_diet_executable_reconstruction();
	if(argc == 2) {
		optional_local_gate(argv[1]);
	} else if(argc == 3) {
		optional_local_gate(argv[1]);
		optional_local_gate(argv[2]);
		optional_local_equivalence(argv[1], argv[2]);
	} else if(argc != 1) {
		std::cerr <<
			"usage: test_app_mainl_compiled_text [MAINL.EXE [MAINL.EXE]]\n";
		return 2;
	}
	if(failures != 0U) {
		std::cerr << failures << " of " << checks << " checks failed\n";
		return 1;
	}
	std::cout << checks << " MAINL compiled-text checks passed\n";
	return 0;
}

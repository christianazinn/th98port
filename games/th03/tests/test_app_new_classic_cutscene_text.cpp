#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

#include "th03/app/content/main_compiled_content.hpp"
#include "th03/app/new_classic/content/new_classic_cutscene_text.hpp"
#include "th03/present/cutscene_script.hpp"

namespace {

int failures = 0;
int checks = 0;

void expect(bool condition, std::string_view message)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << message << '\n';
	}
}

void put_u16(std::vector<std::byte> &bytes, std::size_t offset, std::uint16_t value)
{
	bytes[offset] = static_cast<std::byte>(value & 0xFFU);
	bytes[offset + 1U] = static_cast<std::byte>((value >> 8U) & 0xFFU);
}

void put_u32(std::vector<std::byte> &bytes, std::size_t offset, std::uint32_t value)
{
	for(std::size_t index = 0U; index < 4U; ++index) {
		bytes[offset + index] = static_cast<std::byte>(
			(value >> (index * 8U)) & 0xFFU
		);
	}
}

void put_u64(std::vector<std::byte> &bytes, std::size_t offset, std::uint64_t value)
{
	for(std::size_t index = 0U; index < 8U; ++index) {
		bytes[offset + index] = static_cast<std::byte>(
			(value >> (index * 8U)) & 0xFFU
		);
	}
}

std::uint64_t fnv1a64(std::span<const std::uint8_t> bytes)
{
	auto result = 14695981039346656037ULL;
	for(const auto value : bytes) {
		result ^= value;
		result *= 1099511628211ULL;
	}
	return result;
}

std::vector<std::byte> pack(std::string_view locale = "test")
{
	constexpr std::size_t header = 128U;
	constexpr std::size_t scripts = 28U * 24U;
	constexpr std::size_t runs = 28U * 24U;
	constexpr std::array<std::uint8_t, 6U> source{
		0x82U, 0xA0U, 0x82U, 0xA2U,
		static_cast<std::uint8_t>('\\'), static_cast<std::uint8_t>('$'),
	};
	constexpr std::array<std::uint8_t, 4U> replacement{
		0x82U, 0xA4U, 0x82U, 0xA6U,
	};
	const auto payload = replacement.size() * 28U;
	std::vector<std::byte> result(
		header + locale.size() + scripts + runs + payload
	);
	for(std::size_t index = 0U; index < 8U; ++index) {
		result[index] = static_cast<std::byte>("T3CTP001"[index]);
	}
	put_u16(result, 8U, 1U);
	put_u16(result, 10U, header);
	put_u16(result, 12U, 28U);
	result[14U] = std::byte{1U};
	result[15U] = static_cast<std::byte>(locale.size());
	put_u32(result, 20U, 28U);
	put_u32(result, 24U, scripts);
	put_u32(result, 28U, runs);
	put_u32(result, 32U, static_cast<std::uint32_t>(payload));
	put_u64(result, 40U, 0x0123456789ABCDEFULL);
	for(std::size_t index = 0U; index < locale.size(); ++index) {
		result[header + index] = static_cast<std::byte>(locale[index]);
	}
	const auto script_directory = header + locale.size();
	const auto run_directory = script_directory + scripts;
	const auto payload_offset = run_directory + runs;
	for(std::size_t index = 0U; index < 28U; ++index) {
		const auto script = script_directory + index * 24U;
		put_u32(result, script, static_cast<std::uint32_t>(source.size()));
		put_u32(result, script + 4U, 1U);
		put_u32(result, script + 8U, static_cast<std::uint32_t>(index));
		put_u64(result, script + 16U, fnv1a64(source));
		const auto run = run_directory + index * 24U;
		put_u32(result, run, 0U);
		put_u16(result, run + 4U, 2U);
		put_u64(result, run + 8U, fnv1a64(
			std::span<const std::uint8_t>{source}.first<4U>()
		));
		put_u32(result, run + 16U,
			static_cast<std::uint32_t>(index * replacement.size()));
		put_u16(result, run + 20U,
			static_cast<std::uint16_t>(replacement.size()));
		for(std::size_t byte = 0U; byte < replacement.size(); ++byte) {
			result[payload_offset + index * replacement.size() + byte] =
				static_cast<std::byte>(replacement[byte]);
		}
	}
	const auto digest = th03::app::app_sha256(
		std::span<const std::byte>{result}.subspan(header)
	);
	for(std::size_t index = 0U; index < digest.size(); ++index) {
		result[48U + index] = static_cast<std::byte>(digest[index]);
	}
	return result;
}

void rehash(std::vector<std::byte> &bytes)
{
	const auto digest = th03::app::app_sha256(
		std::span<const std::byte>{bytes}.subspan(128U)
	);
	for(std::size_t index = 0U; index < digest.size(); ++index) {
		bytes[48U + index] = static_cast<std::byte>(digest[index]);
	}
}

void write(const std::filesystem::path &path, std::span<const std::byte> bytes)
{
	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	output.write(reinterpret_cast<const char *>(bytes.data()),
		static_cast<std::streamsize>(bytes.size()));
}

bool same_non_text_operation(
	th03::present::CutsceneOperation left,
	th03::present::CutsceneOperation right
)
{
	if(left.index() != right.index()) {
		return false;
	}
	if(auto *glyph = std::get_if<th03::present::CutsceneDrawGlyph>(&left)) {
		glyph->shift_jis = {};
	}
	if(auto *glyph = std::get_if<th03::present::CutsceneDrawGlyph>(&right)) {
		glyph->shift_jis = {};
	}
	return left == right;
}

void expect_vm_contract_unchanged(
	std::span<const std::uint8_t> source,
	std::span<const std::array<std::uint8_t, 2U>> replacement
)
{
	auto source_vm = th03::present::CutsceneScriptVm::create(source);
	auto replacement_vm = th03::present::CutsceneScriptVm::create(source);
	expect(source_vm && replacement_vm,
		"source and replacement scripts both construct the production VM");
	if(!source_vm || !replacement_vm) {
		return;
	}
	std::size_t replacement_index = 0U;
	for(std::size_t iteration = 0U; iteration < 32U; ++iteration) {
		auto source_batch = iteration == 0U ? source_vm->begin() :
			source_vm->step({.cancel_held = true, .any_key_held = true});
		auto replacement_batch = iteration == 0U ? replacement_vm->begin() :
			replacement_vm->step({.cancel_held = true, .any_key_held = true});
		expect(source_batch && replacement_batch,
			"source and replacement scripts remain valid at every VM iteration");
		if(!source_batch || !replacement_batch) {
			return;
		}
		for(std::size_t operation = 0U;
			operation < replacement_batch->count; ++operation) {
			if(auto *glyph = std::get_if<th03::present::CutsceneDrawGlyph>(
				&replacement_batch->storage[operation]
			)) {
				expect(replacement_index < replacement.size(),
					"replacement provides one cell for every source glyph");
				if(replacement_index >= replacement.size()) {
					return;
				}
				glyph->shift_jis = replacement[replacement_index++];
			}
		}
		expect(source_batch->operations().size() ==
			replacement_batch->operations().size(),
			"replacement preserves the operation count of every VM iteration");
		if(source_batch->operations().size() !=
			replacement_batch->operations().size()) {
			return;
		}
		for(std::size_t operation = 0U;
			operation < source_batch->operations().size(); ++operation) {
			expect(same_non_text_operation(
				source_batch->operations()[operation],
				replacement_batch->operations()[operation]
			), "replacement preserves every non-glyph operation field");
		}
		expect(source_vm->byte_offset() == replacement_vm->byte_offset() &&
			source_vm->cursor() == replacement_vm->cursor() &&
			source_vm->finished() == replacement_vm->finished(),
			"replacement preserves byte, cursor, and terminal timing state");
		if(source_vm->finished()) {
			expect(replacement_vm->finished(),
				"replacement terminates on the same VM iteration");
			expect(replacement_index == replacement.size(),
				"replacement consumes every cell at the source terminator");
			return;
		}
	}
	expect(false, "synthetic source and replacement terminate within the bound");
}

void test_pack()
{
	const auto path = std::filesystem::temp_directory_path() /
		"th03-test-cutscene-text.t3ctp";
	auto bytes = pack();
	write(path, bytes);
	auto loaded = th03::app::load_new_classic_cutscene_text_pack(path);
	expect(loaded && loaded->locale() == "test" &&
		loaded->source_profile_fnv1a64() == 0x0123456789ABCDEFULL,
		"runtime accepts a complete deterministic T3CTP001 catalog");
	if(loaded) {
		const std::array<std::uint8_t, 6U> source{
			0x82U, 0xA0U, 0x82U, 0xA2U,
			static_cast<std::uint8_t>('\\'), static_cast<std::uint8_t>('$'),
		};
		auto replacements = loaded->replacement_glyphs(0U, source);
		constexpr std::array<std::uint8_t, 4U> replacement_bytes{
			0x82U, 0xA4U, 0x82U, 0xA6U,
		};
		expect(loaded->run_count(0U) == 1U && loaded->run_count(28U) == 0U &&
			std::ranges::equal(
				loaded->replacement_run_bytes(0U, 0U),
				std::span<const std::uint8_t>{replacement_bytes}
			) && loaded->replacement_run_bytes(0U, 1U).empty() &&
			loaded->replacement_run_bytes(28U, 0U).empty(),
			"validated catalog exposes bounded read-only translator run bytes");
		expect(replacements && *replacements ==
			std::vector<std::array<std::uint8_t, 2U>>{{
				{0x82U, 0xA4U}, {0x82U, 0xA6U},
			}}, "catalog returns glyph cells without copying source commands");
		if(replacements) {
			expect_vm_contract_unchanged(source, *replacements);
		}
		auto mismatched = source;
		mismatched[0U] ^= 1U;
		auto rejected = loaded->replacement_glyphs(0U, mismatched);
		expect(!rejected && rejected.error().code ==
			th03::app::NewClassicCutsceneTextPackErrorCode::script_mismatch,
			"materialization rejects a mismatched authenticated script");
		rejected = loaded->replacement_glyphs(28U, source);
		expect(!rejected && rejected.error().code ==
			th03::app::NewClassicCutsceneTextPackErrorCode::script_mismatch,
			"materialization rejects an out-of-range script identity");
	}

	bytes.back() ^= std::byte{1U};
	write(path, bytes);
	auto rejected = th03::app::load_new_classic_cutscene_text_pack(path);
	expect(!rejected && rejected.error().code ==
		th03::app::NewClassicCutsceneTextPackErrorCode::body_digest_mismatch,
		"runtime authenticates the complete pack body");

	bytes = pack("x");
	write(path, bytes);
	rejected = th03::app::load_new_classic_cutscene_text_pack(path);
	expect(!rejected && rejected.error().code ==
		th03::app::NewClassicCutsceneTextPackErrorCode::locale_shape,
		"runtime enforces the same locale shape as the author compiler");

	bytes = pack();
	constexpr std::size_t first_payload = 128U + 4U + 28U * 24U + 28U * 24U;
	bytes[first_payload] = std::byte{'A'};
	bytes[first_payload + 1U] = std::byte{'B'};
	bytes[first_payload + 2U] = std::byte{'C'};
	bytes[first_payload + 3U] = std::byte{'D'};
	rehash(bytes);
	write(path, bytes);
	auto halfwidth = th03::app::load_new_classic_cutscene_text_pack(path);
	expect(halfwidth.has_value(),
		"runtime accepts two halfwidth CP932 glyphs in each 16-pixel VM cell");

	bytes = pack();
	bytes[first_payload] = std::byte{'A'};
	bytes[first_payload + 1U] = std::byte{0x82U};
	rehash(bytes);
	write(path, bytes);
	rejected = th03::app::load_new_classic_cutscene_text_pack(path);
	expect(!rejected && rejected.error().code ==
		th03::app::NewClassicCutsceneTextPackErrorCode::malformed_shift_jis,
		"correctly hashed packs reject CP932 glyphs split across VM cells");
	std::error_code ignored;
	std::filesystem::remove(path, ignored);
}

} // namespace

int main()
{
	test_pack();
	std::cout << "new classic cutscene text checks: " << checks << '\n';
	return failures == 0 ? 0 : 1;
}

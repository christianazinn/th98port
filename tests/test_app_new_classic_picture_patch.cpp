#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

#include "th03/app/main_compiled_content.hpp"
#include "th03/app/new_classic_picture_patch.hpp"

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
	bytes[offset + 1U] = static_cast<std::byte>(value >> 8U);
}

void put_u32(std::vector<std::byte> &bytes, std::size_t offset, std::uint32_t value)
{
	for(std::size_t index = 0U; index < 4U; ++index) {
		bytes[offset + index] = static_cast<std::byte>(
			(value >> (index * 8U)) & 0xFFU
		);
	}
}

th03::assets::PiImage source_image()
{
	th03::assets::PiImage image{};
	image.palette.emplace();
	for(std::size_t index = 0U; index < image.palette->colors.size(); ++index) {
		image.palette->colors[index] = {
			static_cast<std::uint8_t>(index),
			static_cast<std::uint8_t>(15U - index),
			static_cast<std::uint8_t>(index / 2U),
		};
	}
	image.image.width = th03::app::new_classic_picture_patch_width;
	image.image.height = th03::app::new_classic_picture_patch_height;
	image.image.indices.assign(
		static_cast<std::size_t>(image.image.width) * image.image.height, 0U
	);
	return image;
}

std::vector<std::byte> pack(const th03::app::AppSha256 &source_digest)
{
	constexpr std::size_t header = 96U;
	constexpr std::size_t picture = 80U;
	constexpr std::size_t runs = 24U;
	constexpr std::size_t payload = 3U;
	std::vector<std::byte> result(header + picture + runs + payload);
	for(std::size_t index = 0U; index < 8U; ++index) {
		result[index] = static_cast<std::byte>("T3PTP001"[index]);
	}
	put_u32(result, 8U, header);
	put_u32(result, 12U, 1U);
	put_u32(result, 16U, 2U);
	put_u32(result, 20U, header);
	put_u32(result, 24U, header + picture);
	put_u32(result, 28U, static_cast<std::uint32_t>(result.size()));
	put_u32(result, 32U, header + picture + runs);
	constexpr std::string_view locale = "en-US";
	for(std::size_t index = 0U; index < locale.size(); ++index) {
		result[68U + index] = static_cast<std::byte>(locale[index]);
	}

	constexpr std::string_view member = "DM3B.PI";
	for(std::size_t index = 0U; index < member.size(); ++index) {
		result[header + index] = static_cast<std::byte>(member[index]);
	}
	for(std::size_t index = 0U; index < source_digest.size(); ++index) {
		result[header + 16U + index] = static_cast<std::byte>(source_digest[index]);
	}
	put_u32(result, header + 48U, 0U);
	put_u32(result, header + 52U, 2U);
	put_u16(result, header + 56U, 1U);
	put_u16(result, header + 58U, 1U);
	put_u16(result, header + 60U, 7U);
	put_u16(result, header + 62U, 3U);
	put_u32(result, header + 64U, 5U);

	const auto run = header + picture;
	put_u32(result, run, 641U);
	put_u16(result, run + 4U, 3U);
	put_u32(result, run + 8U, 0U);
	put_u32(result, run + 12U, 1285U);
	put_u16(result, run + 16U, 2U);
	put_u32(result, run + 20U, 2U);
	result[header + picture + runs] = std::byte{0x23U};
	result[header + picture + runs + 1U] = std::byte{0x40U};
	result[header + picture + runs + 2U] = std::byte{0xF1U};
	const auto digest = th03::app::app_sha256(
		std::span<const std::byte>{result}.subspan(header)
	);
	for(std::size_t index = 0U; index < digest.size(); ++index) {
		result[36U + index] = static_cast<std::byte>(digest[index]);
	}
	return result;
}

void rehash(std::vector<std::byte> &bytes)
{
	const auto digest = th03::app::app_sha256(
		std::span<const std::byte>{bytes}.subspan(96U)
	);
	for(std::size_t index = 0U; index < digest.size(); ++index) {
		bytes[36U + index] = static_cast<std::byte>(digest[index]);
	}
}

void write(const std::filesystem::path &path, std::span<const std::byte> bytes)
{
	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	output.write(
		reinterpret_cast<const char *>(bytes.data()),
		static_cast<std::streamsize>(bytes.size())
	);
}

void test_pack_and_application()
{
	auto source = source_image();
	const auto source_digest = th03::app::new_classic_picture_source_sha256(source);
	expect(source_digest.has_value(), "canonical decoded source image hashes");
	if(!source_digest) {
		return;
	}
	constexpr th03::app::AppSha256 compiler_fixture_digest{
		0xE5U, 0x88U, 0x73U, 0x1BU, 0xF8U, 0x4FU, 0x2EU, 0xEEU,
		0x5FU, 0x2DU, 0x7AU, 0xA4U, 0xCDU, 0xA1U, 0x9BU, 0xAEU,
		0x4CU, 0xF8U, 0xCAU, 0x6BU, 0x5BU, 0x60U, 0x4FU, 0x77U,
		0xADU, 0x05U, 0x06U, 0xEDU, 0x45U, 0x48U, 0x66U, 0xE4U,
	};
	expect(*source_digest == compiler_fixture_digest,
		"C++ decoded-image identity matches the Python BMP compiler fixture");
	const auto path = std::filesystem::temp_directory_path() /
		"th03-test-picture-patch.t3ptp";
	auto bytes = pack(*source_digest);
	write(path, bytes);
	auto loaded = th03::app::load_new_classic_picture_patch(path);
	expect(loaded && loaded->locale() == "en-US" && loaded->picture_count() == 1U,
		"runtime accepts a deterministic T3PTP001 catalog");
	if(loaded) {
		auto untouched = source;
		auto absent = loaded->apply("DM1A.PI", untouched);
		expect(absent && !*absent && untouched.image.indices == source.image.indices,
			"an unpatched picture returns false without mutation");
		auto applied = loaded->apply("dm3b.pi", source);
		expect(applied && *applied && source.image.indices[641U] == 2U &&
			source.image.indices[642U] == 3U && source.image.indices[643U] == 4U &&
			source.image.indices[1285U] == 15U && source.image.indices[1286U] == 1U,
			"matching member application is case-insensitive and exact");
		expect(source.image.indices[640U] == 0U && source.image.indices[644U] == 0U &&
			source.image.indices[1284U] == 0U && source.image.indices[1287U] == 0U,
			"application retains every neighboring source pixel");

		auto mismatch = source_image();
		mismatch.image.indices[0U] = 1U;
		const auto before = mismatch.image.indices;
		auto rejected = loaded->apply("DM3B.PI", mismatch);
		expect(!rejected && rejected.error().code ==
			th03::app::NewClassicPicturePatchErrorCode::source_digest_mismatch &&
			mismatch.image.indices == before,
			"source identity mismatch fails before any mutation");
	}

	bytes.back() ^= std::byte{1U};
	write(path, bytes);
	auto rejected = th03::app::load_new_classic_picture_patch(path);
	expect(!rejected && rejected.error().code ==
		th03::app::NewClassicPicturePatchErrorCode::body_digest_mismatch,
		"runtime authenticates the complete pack body");

	bytes = pack(*source_digest);
	bytes.back() = std::byte{0xF2U};
	rehash(bytes);
	write(path, bytes);
	rejected = th03::app::load_new_classic_picture_patch(path);
	expect(rejected.has_value(), "an even run uses both packed nibbles");

	bytes = pack(*source_digest);
	put_u32(bytes, 96U + 80U + 8U, 1U);
	rehash(bytes);
	write(path, bytes);
	rejected = th03::app::load_new_classic_picture_patch(path);
	expect(!rejected && rejected.error().code ==
		th03::app::NewClassicPicturePatchErrorCode::run_shape,
		"replacement payloads must be stored in canonical run order");

	bytes = pack(*source_digest);
	put_u32(bytes, 96U + 80U, 639U);
	rehash(bytes);
	write(path, bytes);
	rejected = th03::app::load_new_classic_picture_patch(path);
	expect(!rejected && rejected.error().code ==
		th03::app::NewClassicPicturePatchErrorCode::run_shape,
		"a run that crosses its source row fails closed");

	bytes = pack(*source_digest);
	put_u16(bytes, 96U + 60U, 8U);
	rehash(bytes);
	write(path, bytes);
	rejected = th03::app::load_new_classic_picture_patch(path);
	expect(!rejected && rejected.error().code ==
		th03::app::NewClassicPicturePatchErrorCode::member_shape,
		"declared changed bounds must exactly match replacement runs");

	auto malformed = source_image();
	malformed.palette.reset();
	expect(!th03::app::new_classic_picture_source_sha256(malformed),
		"source digest rejects palette-free decoded pictures");
	std::error_code ignored{};
	std::filesystem::remove(path, ignored);
}

} // namespace

int main()
{
	test_pack_and_application();
	std::cout << "new classic picture patch checks: " << checks << '\n';
	return failures == 0 ? 0 : 1;
}

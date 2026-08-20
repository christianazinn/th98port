#include "th03/app/update_package.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace th03::app;

std::size_t checks = 0U;
std::size_t failures = 0U;

void expect(bool condition, const char *label)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << label << '\n';
	}
}

struct Member final {
	std::string path{};
	std::vector<std::byte> bytes{};
	bool executable = false;
};

void append_u16(std::vector<std::byte> &out, std::uint16_t value)
{
	out.push_back(static_cast<std::byte>(value & 0xFFU));
	out.push_back(static_cast<std::byte>(value >> 8U));
}

void append_u32(std::vector<std::byte> &out, std::uint32_t value)
{
	for(std::size_t index = 0U; index < 4U; ++index) {
		out.push_back(static_cast<std::byte>(value >> (index * 8U)));
	}
}

void append_u64(std::vector<std::byte> &out, std::uint64_t value)
{
	for(std::size_t index = 0U; index < 8U; ++index) {
		out.push_back(static_cast<std::byte>(value >> (index * 8U)));
	}
}

[[nodiscard]] std::vector<std::byte> package(
	std::vector<Member> members
)
{
	std::uint64_t table_size = 0U;
	std::uint64_t payload_size = 0U;
	for(const auto &member : members) {
		table_size += update_package_entry_bytes + member.path.size();
		payload_size += member.bytes.size();
	}
	std::vector<std::byte> result;
	for(const char character : std::string("T3PKG001")) {
		result.push_back(static_cast<std::byte>(character));
	}
	append_u16(result, 1U);
	append_u16(result, update_package_header_bytes);
	append_u32(result, static_cast<std::uint32_t>(members.size()));
	append_u64(result, table_size);
	append_u64(result, payload_size);
	for(std::size_t index = 0U; index < members.size(); ++index) {
		const auto &member = members[index];
		append_u16(result, static_cast<std::uint16_t>(member.path.size()));
		append_u16(result, member.executable ? 1U : 0U);
		append_u32(result, 0U);
		append_u64(result, member.bytes.size());
		for(std::size_t digest = 0U; digest < 32U; ++digest) {
			result.push_back(static_cast<std::byte>(index + digest + 1U));
		}
		for(const char character : member.path) {
			result.push_back(static_cast<std::byte>(character));
		}
	}
	for(const auto &member : members) {
		result.insert(result.end(), member.bytes.begin(), member.bytes.end());
	}
	return result;
}

void expect_error(
	std::span<const std::byte> bytes,
	UpdatePackageErrorCode code,
	const char *label
)
{
	const auto parsed = parse_update_package(bytes);
	expect(!parsed && parsed.error().code == code, label);
}

[[nodiscard]] Member member(std::string path, std::string bytes)
{
	Member result{.path = std::move(path)};
	for(const auto character : bytes) {
		result.bytes.push_back(static_cast<std::byte>(character));
	}
	return result;
}

} // namespace

int main()
{
	using enum UpdatePackageErrorCode;
	auto valid = package({
		member("licenses/SDL3/LICENSE.txt", "license"),
		Member{.path = "th03-app.exe", .bytes = {std::byte{'M'}, std::byte{'Z'}},
			.executable = true},
	});
	const auto parsed = parse_update_package(valid);
	expect(parsed && parsed->entries.size() == 2U,
		"valid two-member package parses");
	if(parsed) {
		expect(parsed->payload_offset == valid.size() - 9U &&
			parsed->entries[0U].path == "licenses/SDL3/LICENSE.txt" &&
			parsed->entries[0U].size == 7U &&
			!parsed->entries[0U].executable &&
			parsed->entries[1U].path == "th03-app.exe" &&
			parsed->entries[1U].payload_offset == valid.size() - 2U &&
			parsed->entries[1U].executable,
			"member metadata and payload offsets are exact");
	}
	expect_error(std::span(valid).first(10U), too_short,
		"short header is rejected");
	auto changed = valid;
	changed[0U] = std::byte{'X'};
	expect_error(changed, invalid_magic, "wrong magic is rejected");
	expect_error(package({member("PODD.GEN", "reserved")}), invalid_path,
		"materializer ownership marker path is reserved");
	changed = valid;
	changed[8U] = std::byte{2U};
	expect_error(changed, unsupported_version, "wrong version is rejected");
	changed = valid;
	changed[10U] = std::byte{31U};
	expect_error(changed, invalid_header_size, "wrong header size is rejected");
	changed = valid;
	changed[12U] = std::byte{0U};
	expect_error(changed, invalid_file_count, "empty package is rejected");
	changed = valid;
	changed[16U] ^= std::byte{1U};
	expect_error(changed, invalid_size, "wrong table size is rejected");
	changed = valid;
	changed[34U] = std::byte{2U};
	expect_error(changed, unsupported_flags, "unknown flags are rejected");
	changed = valid;
	changed[36U] = std::byte{1U};
	expect_error(changed, nonzero_reserved, "reserved fields stay zero");
	changed = valid;
	changed[32U] = std::byte{0U};
	changed[33U] = std::byte{0U};
	expect_error(changed, invalid_path_length, "empty path is rejected");

	for(const auto &[path, label] : std::array{
		std::pair{"/absolute", "absolute path is rejected"},
		std::pair{"a//b", "empty path component is rejected"},
		std::pair{"a/../b", "parent traversal is rejected"},
		std::pair{"a/./b", "dot component is rejected"},
		std::pair{"a\\b", "backslash path is rejected"},
		std::pair{"a:b", "drive/colon path is rejected"},
		std::pair{"CON.txt", "Windows device path is rejected"},
		std::pair{"dir/Lpt9", "numbered Windows device path is rejected"},
		std::pair{"trailing.", "trailing-dot component is rejected"},
	}) {
		expect_error(package({member(path, "x")}), invalid_path, label);
	}
	expect_error(
		package({member("b", "1"), member("a", "2")}), unsorted_path,
		"unsorted table is rejected"
	);
	expect_error(
		package({member("A", "1"), member("a", "2")}), duplicate_path,
		"case-insensitive path collision is rejected"
	);
	changed = valid;
	changed[24U] ^= std::byte{1U};
	expect_error(changed, invalid_size, "wrong aggregate payload size is rejected");
	changed = valid;
	changed[40U] = std::byte{0xFFU};
	expect_error(changed, invalid_payload_size,
		"member payload overflow is rejected");
	changed = valid;
	changed.push_back(std::byte{0U});
	expect_error(changed, invalid_size, "trailing bytes are rejected");

	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}

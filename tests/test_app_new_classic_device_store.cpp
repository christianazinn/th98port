#include "th03/app/new_classic_device_store.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace {

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

th03::app::NewClassicControllerIdentity identity(std::uint8_t value)
{
	th03::app::NewClassicControllerIdentity result{};
	result.bytes[0] = value;
	return result;
}

} // namespace

int main()
{
	using namespace th03::app;
	NewClassicDeviceAssignment assignment{};
	assignment.mode = {
		NewClassicControllerAssignmentMode::specific,
		NewClassicControllerAssignmentMode::disabled,
	};
	assignment.identity[0] = identity(7U);
	auto encoded = encode_new_classic_device_assignment(assignment);
	expect(static_cast<bool>(encoded), "specific assignment encodes");
	if(encoded) {
		auto decoded = decode_new_classic_device_assignment(*encoded);
		expect(decoded && *decoded == assignment, "assignment round-trips");
	}

	auto duplicate = assignment;
	duplicate.mode[1] = NewClassicControllerAssignmentMode::specific;
	duplicate.identity[1] = duplicate.identity[0];
	auto duplicate_encoded = encode_new_classic_device_assignment(duplicate);
	expect(!duplicate_encoded && duplicate_encoded.error().code ==
		NewClassicDeviceCodecErrorCode::duplicate_identity,
		"one identity cannot own both players");
	auto stale = assignment;
	stale.mode[0] = NewClassicControllerAssignmentMode::automatic;
	expect(!encode_new_classic_device_assignment(stale),
		"automatic assignment cannot retain a stale identity");

	const auto root = std::filesystem::temp_directory_path() /
		"th03-new-classic-device-store-test";
	std::error_code ignored;
	std::filesystem::remove_all(root, ignored);
	const auto path = root / "TH3DEV.CFG";
	auto absent = load_new_classic_device_assignment(path, {});
	expect(absent && !absent->file_present &&
		absent->assignment == NewClassicDeviceAssignment{},
		"absent store returns automatic defaults");
	expect(static_cast<bool>(store_new_classic_device_assignment(path, assignment)),
		"assignment stores atomically");
	auto loaded = load_new_classic_device_assignment(path, {});
	expect(loaded && loaded->file_present && !loaded->recovered_defaults &&
		loaded->assignment == assignment,
		"stored assignment reads back");

	if(encoded) {
		(*encoded)[20] ^= std::byte{0x01U};
		std::ofstream corrupt(path, std::ios::binary | std::ios::trunc);
		corrupt.write(reinterpret_cast<const char *>(encoded->data()),
			static_cast<std::streamsize>(encoded->size()));
		corrupt.close();
	}
	auto recovered = load_new_classic_device_assignment(path, {});
	expect(recovered && recovered->file_present &&
		recovered->recovered_defaults &&
		recovered->assignment == NewClassicDeviceAssignment{},
		"corrupt store recovers automatic defaults");
	std::filesystem::remove_all(root, ignored);

	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}

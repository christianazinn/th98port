#ifndef TH03_APP_NEW_CLASSIC_CONTROLLER_HPP
#define TH03_APP_NEW_CLASSIC_CONTROLLER_HPP

#include <array>
#include <cstddef>
#include <cstdint>

namespace th03::app {

inline constexpr std::size_t new_classic_controller_identity_bytes = 16U;
inline constexpr std::size_t new_classic_controller_name_bytes = 48U;
inline constexpr std::size_t new_classic_controller_capacity = 15U;

struct NewClassicControllerIdentity final {
	std::array<std::uint8_t, new_classic_controller_identity_bytes> bytes{};

	[[nodiscard]] constexpr bool empty() const noexcept
	{
		for(const auto value : bytes) {
			if(value != 0U) {
				return false;
			}
		}
		return true;
	}

	friend constexpr bool operator==(
		const NewClassicControllerIdentity &,
		const NewClassicControllerIdentity &
	) = default;
};

enum class NewClassicControllerAssignmentMode : std::uint8_t {
	automatic,
	specific,
	disabled,
};

enum class NewClassicControllerPlayer : std::uint8_t {
	none,
	p1,
	p2,
};

struct NewClassicDeviceAssignment final {
	std::array<NewClassicControllerAssignmentMode, 2U> mode{
		NewClassicControllerAssignmentMode::automatic,
		NewClassicControllerAssignmentMode::automatic,
	};
	std::array<NewClassicControllerIdentity, 2U> identity{};

	friend constexpr bool operator==(
		const NewClassicDeviceAssignment &,
		const NewClassicDeviceAssignment &
	) = default;
};

struct NewClassicControllerSnapshot final {
	NewClassicControllerIdentity identity{};
	std::array<char, new_classic_controller_name_bytes> name{};
	NewClassicControllerPlayer player = NewClassicControllerPlayer::none;
	std::uint16_t p1 = 0U;
	std::uint16_t p2 = 0U;
	std::uint16_t system = 0U;
	std::uint8_t charge_mask = 0U;

	friend constexpr bool operator==(
		const NewClassicControllerSnapshot &,
		const NewClassicControllerSnapshot &
	) = default;
};

struct NewClassicControllerCatalog final {
	std::array<NewClassicControllerSnapshot, new_classic_controller_capacity>
		controllers{};
	std::size_t count = 0U;

	friend constexpr bool operator==(
		const NewClassicControllerCatalog &,
		const NewClassicControllerCatalog &
	) = default;
};

using NewClassicControllerCatalogRead = bool (*)(
	void *context,
	NewClassicControllerCatalog &catalog
) noexcept;

[[nodiscard]] constexpr bool valid_new_classic_device_assignment(
	const NewClassicDeviceAssignment &assignment
) noexcept
{
	for(std::size_t player = 0U; player < assignment.mode.size(); ++player) {
		const auto mode = assignment.mode[player];
		if(mode != NewClassicControllerAssignmentMode::automatic &&
			mode != NewClassicControllerAssignmentMode::specific &&
			mode != NewClassicControllerAssignmentMode::disabled) {
			return false;
		}
		if((mode == NewClassicControllerAssignmentMode::specific) ==
			assignment.identity[player].empty()) {
			return false;
		}
	}
	return assignment.mode[0] != NewClassicControllerAssignmentMode::specific ||
		assignment.mode[1] != NewClassicControllerAssignmentMode::specific ||
		assignment.identity[0] != assignment.identity[1];
}

} // namespace th03::app

#endif

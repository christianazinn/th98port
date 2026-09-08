#pragma once

#include "pc98/th0405_extend.hpp"

#include <cstdint>

namespace th05::core {

using ExtendState = pc98::th0405::ExtendState;
using ExtendEvents = pc98::th0405::ExtendEvents;

struct PointExtendState {
	std::uint16_t extend_point_items_collected{};
	std::uint8_t extends_gained{};
	bool operator==(const PointExtendState &) const = default;
};

// th05/main/item/collect.cpp::point_items_extend_update. Its caller has
// already incremented the item counters. This does not collect an item or
// change the point count, and awards at most one threshold per invocation.
[[nodiscard]] inline ExtendEvents point_items_extend_update(
	PointExtendState &points, ExtendState &state) noexcept
{
	const auto threshold = (static_cast<unsigned>(points.extends_gained) + 1U) * 100U;
	if(points.extend_point_items_collected < threshold) return {};
	return pc98::th0405::grant_extend(points.extends_gained, state, 99);
}

} // namespace th05::core

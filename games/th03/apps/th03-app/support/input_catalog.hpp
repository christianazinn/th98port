#pragma once

#include <cstdint>
#include <string_view>

#include "th03/app/new_classic/input/new_classic_controller.hpp"

namespace th03::app::launcher_support {

[[nodiscard]] std::string_view sdl_scancode_name(
	void *context,
	std::uint16_t key
) noexcept;

[[nodiscard]] bool sdl_controller_catalog(
	void *context,
	NewClassicControllerCatalog &catalog
) noexcept;

} // namespace th03::app::launcher_support

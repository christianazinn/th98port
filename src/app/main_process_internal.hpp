#ifndef TH03_APP_MAIN_PROCESS_INTERNAL_HPP
#define TH03_APP_MAIN_PROCESS_INTERNAL_HPP

#include <cstddef>
#include <expected>
#include <memory>
#include <span>
#include <utility>

#include "th03/app/main_process.hpp"

namespace th03::app {

// Hermetic tests need structurally valid synthetic asset bytes that cannot
// match the authenticated original-resource fingerprints. Production callers
// have no access to this source-private seam.
struct MainProcessTestAccess final {
	[[nodiscard]] static std::expected<
		std::unique_ptr<MainProcess>,
		MainProcessError
	> create_with_synthetic_resources(
		const MainDirectVsConfig &config,
		std::span<const LoadedMainResource> resources,
		MainCompiledContent compiled_content,
		std::span<const std::byte> font_rom
	)
	{
		return MainProcess::create_impl(
			config,
			resources,
			std::move(compiled_content),
			font_rom,
			false
		);
	}

	static void fail_presentation(MainProcess &process) noexcept
	{
		process.fail_presentation_for_test();
	}

	[[nodiscard]] static const assets::MrsImage *portrait(
		const MainProcess &process,
		bool bomb,
		std::size_t player
	) noexcept;
};

} // namespace th03::app

#endif

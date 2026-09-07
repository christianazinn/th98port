#ifndef TH03_APP_NEW_CLASSIC_NETPLAY_INPUT_DELAY_HPP
#define TH03_APP_NEW_CLASSIC_NETPLAY_INPUT_DELAY_HPP

#include <array>
#include <cstdint>
#include <expected>
#include <optional>

#include "th03/app/new_classic/netplay/new_classic_netplay_protocol.hpp"

namespace th03::app {

inline constexpr std::uint8_t new_classic_netplay_max_input_delay = 6U;

enum class NewClassicNetplayInputDelayError : std::uint8_t {
	invalid_delay,
	frame_regressed,
	conflicting_retry,
	frame_overflow,
};

// Fixed-capacity symmetric gameplay delay. A source input sampled for frame F
// is used at F + delay; the newly entered gameplay domain begins with neutral
// values. Retrying one stalled frame is idempotent and may not replace the
// already sampled physical value.
class NewClassicNetplayInputDelay final {
public:
	[[nodiscard]] static std::expected<
		NewClassicNetplayInputDelay,
		NewClassicNetplayInputDelayError
	> create(std::uint8_t frames) noexcept;

	[[nodiscard]] std::expected<
		NewClassicNetplayOwnedInput,
		NewClassicNetplayInputDelayError
	> stage(
		std::uint64_t frame,
		NewClassicNetplayOwnedInput sampled
	) noexcept;

	void reset() noexcept;
	[[nodiscard]] std::uint8_t frames() const noexcept;

private:
	struct Slot final {
		bool valid = false;
		std::uint64_t frame = 0U;
		NewClassicNetplayOwnedInput input{};
	};

	explicit NewClassicNetplayInputDelay(std::uint8_t frames) noexcept;

	std::array<Slot, new_classic_netplay_max_input_delay + 1U> slots_{};
	std::optional<std::uint64_t> next_frame_{};
	std::optional<std::uint64_t> retry_frame_{};
	NewClassicNetplayOwnedInput retry_sampled_{};
	NewClassicNetplayOwnedInput retry_effective_{};
	std::uint8_t frames_ = 0U;
};

} // namespace th03::app

#endif

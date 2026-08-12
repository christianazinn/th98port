#ifndef TH03_APP_CLASSIC_INPUT_BITS_INTERNAL_HPP
#define TH03_APP_CLASSIC_INPUT_BITS_INTERNAL_HPP

#include <cstdint>

namespace th03::app {

inline constexpr std::uint16_t input_up = 0x0001U;
inline constexpr std::uint16_t input_down = 0x0002U;
inline constexpr std::uint16_t input_left = 0x0004U;
inline constexpr std::uint16_t input_right = 0x0008U;
inline constexpr std::uint16_t input_bomb = 0x0010U;
inline constexpr std::uint16_t input_shot = 0x0020U;
inline constexpr std::uint16_t input_cancel = 0x1000U;
inline constexpr std::uint16_t input_ok = 0x2000U;

} // namespace th03::app

#endif

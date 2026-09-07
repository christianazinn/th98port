#ifndef TH03_CORE_NUMERIC_HPP
#define TH03_CORE_NUMERIC_HPP

#include <bit>
#include <concepts>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace th03::exact {

template <std::integral T>
using unsigned_t = std::make_unsigned_t<T>;

template <std::integral T>
constexpr unsigned_t<T> bits(T value) noexcept
{
	if constexpr(std::is_unsigned_v<T>) {
		return value;
	} else {
		return std::bit_cast<unsigned_t<T>>(value);
	}
}

template <std::integral T>
constexpr T from_bits(unsigned_t<T> value) noexcept
{
	if constexpr(std::is_unsigned_v<T>) {
		return value;
	} else {
		return std::bit_cast<T>(value);
	}
}

template <std::integral T>
constexpr T wrap_add(T left, T right) noexcept
{
	return from_bits<T>(static_cast<unsigned_t<T>>(bits(left) + bits(right)));
}

template <std::integral T>
constexpr T wrap_subtract(T left, T right) noexcept
{
	return from_bits<T>(static_cast<unsigned_t<T>>(bits(left) - bits(right)));
}

template <std::integral T>
constexpr T wrap_negate(T value) noexcept
{
	return from_bits<T>(static_cast<unsigned_t<T>>(0U - bits(value)));
}

template <std::integral T>
constexpr T wrap_multiply(T left, T right) noexcept
{
	return from_bits<T>(static_cast<unsigned_t<T>>(bits(left) * bits(right)));
}

template <std::integral T>
constexpr T wrap_shift_left(T value, unsigned count) noexcept
{
	constexpr auto width = std::numeric_limits<unsigned_t<T>>::digits;
	if(count >= width) {
		return from_bits<T>(0U);
	}
	return from_bits<T>(static_cast<unsigned_t<T>>(bits(value) << count));
}

template <std::signed_integral T>
constexpr T arithmetic_shift_right(T value, unsigned count) noexcept
{
	constexpr auto width = std::numeric_limits<unsigned_t<T>>::digits;
	if(count == 0U) {
		return value;
	}
	if(count >= width) {
		return (value < 0) ? static_cast<T>(-1) : static_cast<T>(0);
	}

	auto shifted = static_cast<unsigned_t<T>>(bits(value) >> count);
	if(value < 0) {
		const auto fill = static_cast<unsigned_t<T>>(
			~unsigned_t<T>{0} << (width - count)
		);
		shifted = static_cast<unsigned_t<T>>(shifted | fill);
	}
	return from_bits<T>(shifted);
}

template <std::integral Rep, unsigned FractionBits>
class Fixed {
public:
	using rep_type = Rep;
	using unsigned_rep_type = unsigned_t<Rep>;

	static constexpr unsigned fraction_bits = FractionBits;
	static constexpr unsigned bit_width =
		std::numeric_limits<unsigned_rep_type>::digits;
	static_assert(FractionBits < bit_width);

	constexpr Fixed() noexcept = default;

	static constexpr Fixed from_raw(Rep raw) noexcept
	{
		return Fixed(raw);
	}

	static constexpr Fixed from_raw_bits(unsigned_rep_type raw) noexcept
	{
		return Fixed(from_bits<Rep>(raw));
	}

	static constexpr Fixed from_integer_wrapping(std::int64_t value) noexcept
	{
		const auto narrowed = static_cast<unsigned_rep_type>(value);
		return from_raw_bits(static_cast<unsigned_rep_type>(narrowed << FractionBits));
	}

	constexpr Rep raw() const noexcept
	{
		return raw_;
	}

	constexpr unsigned_rep_type raw_bits() const noexcept
	{
		return bits(raw_);
	}

	constexpr auto to_integer_floor() const noexcept
	{
		if constexpr(std::is_signed_v<Rep>) {
			return arithmetic_shift_right(raw_, FractionBits);
		} else {
			return static_cast<Rep>(raw_ >> FractionBits);
		}
	}

	constexpr auto to_integer_trunc() const noexcept
	{
		return static_cast<Rep>(raw_ / static_cast<Rep>(unsigned_rep_type{1} << FractionBits));
	}

	friend constexpr bool operator==(Fixed, Fixed) noexcept = default;

private:
	explicit constexpr Fixed(Rep raw) noexcept : raw_(raw) {}
	Rep raw_{};
};

using Subpixel = Fixed<std::int16_t, 4>;       // PC-98 Q12.4
using SubpixelDelta8 = Fixed<std::int8_t, 4>;  // PC-98 signed Q4.4
using SubpixelLength8 = Fixed<std::uint8_t, 4>; // PC-98 unsigned Q4.4

class DecimalSubpixel {
public:
	static constexpr DecimalSubpixel from_raw(std::int16_t raw) noexcept
	{
		return DecimalSubpixel(raw);
	}

	constexpr std::int16_t raw() const noexcept { return raw_; }
	constexpr std::int16_t to_integer_trunc() const noexcept
	{
		return static_cast<std::int16_t>(raw_ / 10);
	}

	friend constexpr bool operator==(DecimalSubpixel, DecimalSubpixel) noexcept = default;

private:
	explicit constexpr DecimalSubpixel(std::int16_t raw) noexcept : raw_(raw) {}
	std::int16_t raw_{};
};

} // namespace th03::exact

#endif

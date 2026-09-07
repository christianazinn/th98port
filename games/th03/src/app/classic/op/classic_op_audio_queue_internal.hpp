#ifndef TH03_APP_CLASSIC_OP_AUDIO_QUEUE_INTERNAL_HPP
#define TH03_APP_CLASSIC_OP_AUDIO_QUEUE_INTERNAL_HPP

#include <array>
#include <cstddef>
#include <optional>

#include "th03/app/classic/classic_audio.hpp"

namespace th03::app::detail {

class ClassicOpAudioQueue final {
public:
	static constexpr std::size_t capacity = 64U;

	[[nodiscard]] constexpr bool can_accept(
		std::size_t command_count = 1U
	) const noexcept
	{
		return command_count <= (commands_.size() - count_);
	}

	[[nodiscard]] constexpr bool push(ClassicAudioCommand command) noexcept
	{
		if(!can_accept()) {
			return false;
		}
		const auto tail = (head_ + count_) % commands_.size();
		commands_[tail] = command;
		++count_;
		return true;
	}

	template <typename Scene>
	[[nodiscard]] bool drain(Scene &scene) noexcept
	{
		while(auto command = scene.take_audio_command()) {
			if(!push(*command)) {
				return false;
			}
		}
		return true;
	}

	[[nodiscard]] constexpr bool has_pending() const noexcept
	{
		return count_ != 0U;
	}

	[[nodiscard]] constexpr std::optional<ClassicAudioCommand> take() noexcept
	{
		if(!has_pending()) {
			return std::nullopt;
		}
		const auto result = commands_[head_];
		head_ = (head_ + 1U) % commands_.size();
		--count_;
		return result;
	}

private:
	std::array<ClassicAudioCommand, capacity> commands_{};
	std::size_t head_ = 0U;
	std::size_t count_ = 0U;
};

} // namespace th03::app::detail

#endif

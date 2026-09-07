#include "classic/op/classic_op_audio_queue_internal.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>

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

struct AudioSource final {
	std::array<th03::app::ClassicAudioCommand, 3U> commands{};
	std::size_t next = 0U;

	[[nodiscard]] std::optional<th03::app::ClassicAudioCommand>
	take_audio_command() noexcept
	{
		if(next >= commands.size()) {
			return std::nullopt;
		}
		return commands[next++];
	}
};

void test_capacity_fifo_and_wraparound()
{
	using namespace th03::app;
	detail::ClassicOpAudioQueue queue{};
	expect(!queue.has_pending(), "new queue is empty");
	expect(queue.can_accept(detail::ClassicOpAudioQueue::capacity),
		"new queue admits its full capacity");
	for(std::size_t index = 0U;
		index < detail::ClassicOpAudioQueue::capacity; ++index) {
		expect(queue.push({
			.type = ClassicAudioCommandType::play_effect,
			.parameter = static_cast<std::uint16_t>(index),
		}), "queue accepts each command through capacity");
	}
	expect(queue.has_pending(), "filled queue reports pending commands");
	expect(!queue.can_accept(), "filled queue rejects one more command");
	expect(!queue.push({.type = ClassicAudioCommandType::stop_song}),
		"overflow does not mutate the queue");

	for(std::size_t index = 0U; index < 8U; ++index) {
		const auto command = queue.take();
		expect(command && command->parameter == index,
			"queue preserves FIFO order before wraparound");
	}
	for(std::size_t index = 0U; index < 8U; ++index) {
		expect(queue.push({
			.type = ClassicAudioCommandType::play_effect,
			.parameter = static_cast<std::uint16_t>(64U + index),
		}), "queue accepts wrapped tail commands");
	}
	for(std::size_t index = 8U; index < 72U; ++index) {
		const auto command = queue.take();
		expect(command && command->parameter == index,
			"queue preserves FIFO order across wraparound");
	}
	expect(!queue.take(), "empty queue returns no command");
}

void test_scene_drain_and_multi_command_admission()
{
	using namespace th03::app;
	detail::ClassicOpAudioQueue queue{};
	AudioSource source{};
	source.commands = {{
		{.type = ClassicAudioCommandType::stop_song, .parameter = 1U},
		{.type = ClassicAudioCommandType::play_song, .parameter = 2U},
		{.type = ClassicAudioCommandType::play_effect, .parameter = 3U},
	}};
	expect(queue.drain(source), "scene drain accepts a bounded command batch");
	expect(queue.can_accept(
		detail::ClassicOpAudioQueue::capacity - source.commands.size()
	), "capacity query admits the exact remaining space");
	expect(!queue.can_accept(
		detail::ClassicOpAudioQueue::capacity - source.commands.size() + 1U
	), "capacity query rejects a batch larger than remaining space");
	for(const auto &expected : source.commands) {
		const auto actual = queue.take();
		expect(actual && actual->type == expected.type &&
			actual->parameter == expected.parameter,
			"scene drain preserves command order and payload");
	}
}

} // namespace

int main()
{
	test_capacity_fifo_and_wraparound();
	test_scene_drain_and_multi_command_admission();
	if(failures != 0U) {
		std::cerr << failures << " of " << checks << " checks failed\n";
		return 1;
	}
	std::cout << checks << " checks passed\n";
	return 0;
}

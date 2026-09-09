#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "th02/core/player.hpp"
#include "th02/core/spark.hpp"

namespace th02::core {

struct RandomState;

inline constexpr std::size_t stage_scenery_particle_count = 50U;
inline constexpr std::size_t stage_scenery_page_count = 2U;
inline constexpr std::uint8_t stage_scenery_free = 0U;
inline constexpr std::uint8_t stage_scenery_alive = 1U;
inline constexpr std::uint8_t stage_scenery_remove = 2U;

// Semantic fixed-width form of stage_scenery_particle_t from s1.cpp. The
// source's unused byte remains visible; native code does not rely on host
// packing or a DOS far-pointer layout.
struct StageSceneryParticle {
	std::uint8_t flag{};
	std::uint8_t unused{};
	std::array<std::int16_t, stage_scenery_page_count> left{};
	std::array<std::int16_t, stage_scenery_page_count> top{};
	std::int16_t speed{};
	bool operator==(const StageSceneryParticle &) const = default;
};

struct StageSceneryPoolState {
	std::array<StageSceneryParticle, stage_scenery_particle_count> particles{};
	bool operator==(const StageSceneryPoolState &) const = default;
};

// The shared Stage 1/2 flash clock globals. PaletteTone is passed separately
// by reference because it is a source-global word shared with other effects;
// frame is a source signed word and next_frame is a source unsigned word.
struct StageBgFlashState {
	std::int16_t frame{};
	std::uint16_t next_frame{};
	bool operator==(const StageBgFlashState &) const = default;
};

struct StageSceneryRgb {
	std::uint8_t r{};
	std::uint8_t g{};
	std::uint8_t b{};
	bool operator==(const StageSceneryRgb &) const = default;
};

struct StageSceneryInvalidation {
	ScreenPoint topleft{};
	std::int16_t width{};
	std::int16_t height{};
	bool operator==(const StageSceneryInvalidation &) const = default;
};

// Synchronous source-position/presentation seams. Implementations must not
// mutate borrowed scenery, midboss, spark, random, flash, or page state from a
// callback. No VRAM, renderer, or asset fidelity is implied.
class StageSceneryCallbacks {
public:
	virtual ~StageSceneryCallbacks() = default;
	virtual void invalidate_scenery(
		StageSceneryInvalidation invalidation) noexcept = 0;
	virtual void palette_set(std::uint8_t index,
		StageSceneryRgb color) noexcept = 0;
	virtual void palette_tone_set(std::uint16_t tone) noexcept = 0;
	virtual void palette_show() noexcept = 0;
	virtual void grc_set_clip(std::int16_t left, std::int16_t top,
		std::int16_t right, std::int16_t bottom) noexcept = 0;
	virtual void egc_off() noexcept = 0;
	virtual void grcg_set_color(std::uint8_t color) noexcept = 0;
	virtual void dot_square(ScreenPoint topleft,
		std::uint8_t edge) noexcept = 0;
	virtual void grcg_off() noexcept = 0;
};

enum class StageSceneryAddResult : std::uint8_t {
	added,
	pool_full,
};

enum class StageSceneryPageResult : std::uint8_t {
	completed,
	invalid_page,
};

enum class StageSceneryResult : std::uint8_t {
	idle,
	updated,
	pool_full,
	invalid_page,
};
using Stage1SceneryResult = StageSceneryResult;

// th02/main/stage/s1.cpp::stage_scenery_particle_add. A full pool does not
// consume the random byte, matching the source's free-slot loop.
[[nodiscard]] StageSceneryAddResult stage_scenery_particle_add(
	StageSceneryPoolState &state, RandomState &random, std::int16_t left,
	std::int16_t top) noexcept;

// th02/main/stage/s1.cpp::stage_scenery_invalidate. Every nonfree flag first
// emits its 2x3 rectangle; remove then becomes free, while all other raw flags
// carry the front page to the back page.
[[nodiscard]] StageSceneryPageResult stage_scenery_invalidate(
	StageSceneryPoolState &state, std::size_t page_back,
	std::size_t page_front, StageSceneryCallbacks &callbacks) noexcept;

// th02/main/stage/s1.cpp::stage_scenery_particles_render. This is the source
// update/render body: alive records move, out-of-bounds records become remove,
// and visible dots are emitted before the final GRCG-off boundary.
[[nodiscard]] StageSceneryPageResult stage_scenery_particles_render(
	StageSceneryPoolState &state, std::size_t page_back,
	std::int16_t scroll_line, StageSceneryCallbacks &callbacks) noexcept;

// th02/main/stage/s2.cpp::stage_bg_flash_update. It is shared by the Stage 1
// and Stage 2 callers.
void stage_bg_flash_update(StageBgFlashState &state,
	std::uint16_t &palette_tone, RandomState &random,
	StageSceneryCallbacks &callbacks) noexcept;

// th02/main/stage/s1.cpp::stage1_update_and_render. The source
// stage1_scenery_frame is borrowed as a caller reference (normally
// Midboss1State::scenery_frame) rather than duplicated. PaletteTone is also a
// caller reference so another shared owner can bind the same source word. The
// spark acceleration write is likewise applied to the shared parameter object.
[[nodiscard]] Stage1SceneryResult stage1_update_and_render(
	StageSceneryPoolState &state, std::int16_t &scenery_frame,
	StageBgFlashState &flash, std::uint16_t &palette_tone,
	SparkUpdateParameters &spark_parameters, RandomState &random,
	std::size_t page_back, std::int16_t scroll_line, bool reduce_effects,
	StageSceneryCallbacks &callbacks) noexcept;

// th02/main/stage/s2.cpp. Unlike Stage 1, spawn precedes flash update, the
// clock is unsigned, and reduce_effects does not change its every-other-frame
// spawn rate. Setup and clock are retained source globals, not reset here.
[[nodiscard]] StageSceneryResult stage2_update_and_render(
	StageSceneryPoolState &state, std::uint16_t &scenery_frame, std::uint8_t &setup,
	StageBgFlashState &flash, std::uint16_t &palette_tone,
	SparkUpdateParameters &spark_parameters, RandomState &random,
	std::size_t page_back, std::int16_t scroll_line,
	StageSceneryCallbacks &callbacks) noexcept;
} // namespace th02::core

#ifndef TH03_CORE_OBSERVER_HPP
#define TH03_CORE_OBSERVER_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace th03 {

namespace detail {

struct DeterministicState;
struct ObserverAccess;

} // namespace detail

enum class PlayerId : std::uint8_t {
	p1 = 0,
	p2 = 1,
};

enum class ShotPairSlot : std::uint8_t {};
enum class EnemySlot : std::uint8_t {};
enum class FireballSlot : std::uint8_t {};

// Exact source enemy/fireball/explosion flag byte. Explosion hits can raise
// this above the two named explosion values, so it must not be a closed enum.
struct EfeFlag {
	std::uint8_t raw = 0;

	[[nodiscard]] constexpr bool is_free() const noexcept
	{
		return raw == 0U;
	}

	[[nodiscard]] constexpr bool is_explosion() const noexcept
	{
		return raw >= 9U;
	}

	friend bool operator==(const EfeFlag &, const EfeFlag &) = default;
};

enum class EnemyClassification : std::uint8_t {
	free,
	running_spawned,
	running_unspawned,
	explosion,
};

enum class FireballClassification : std::uint8_t {
	free,
	fall,
	transfer,
	explosion,
};

enum class FireballVariant : std::uint8_t {
	blue = 2,
	red = 3,
};

struct MainPhaseContext {
	std::uint8_t round_id = 0;
	std::uint16_t round_or_result_frame = 0;
	std::array<std::int16_t, 2> playfield_fg_shift_x{};

	friend bool operator==(
		const MainPhaseContext &,
		const MainPhaseContext &
	) = default;
};

struct ShotPairFact {
	ShotPairSlot slot{};
	bool alive = false;
	std::int16_t topleft_x = 0;
	std::int16_t topleft_y = 0;
	PlayerId player_id = PlayerId::p1;

	friend bool operator==(
		const ShotPairFact &,
		const ShotPairFact &
	) = default;
};

struct EnemyFact {
	EnemySlot slot{};
	EfeFlag flag{};
	std::uint8_t frame = 0;
	std::int16_t center_x = 0;
	std::int16_t center_y = 0;
	PlayerId player_id = PlayerId::p1;
	std::uint8_t size_pixels = 0;
	std::uint8_t hp = 0;
	std::uint8_t explosion_max_enemy_hits_half = 0;

	[[nodiscard]] constexpr EnemyClassification classification() const noexcept
	{
		if(flag.raw == 0U) {
			return EnemyClassification::free;
		}
		if(flag.raw == 1U) {
			return EnemyClassification::running_spawned;
		}
		if(flag.raw == 3U) {
			return EnemyClassification::running_unspawned;
		}
		return EnemyClassification::explosion;
	}

	friend bool operator==(const EnemyFact &, const EnemyFact &) = default;
};

struct FireballFact {
	FireballSlot slot{};
	EfeFlag flag{};
	std::uint8_t frame = 0;
	std::int16_t center_x = 0;
	std::int16_t center_y = 0;
	PlayerId player_id = PlayerId::p1;
	FireballVariant variant = FireballVariant::blue;

	[[nodiscard]] constexpr FireballClassification classification() const noexcept
	{
		if(flag.raw == 0U) {
			return FireballClassification::free;
		}
		if(flag.raw == 1U) {
			return FireballClassification::fall;
		}
		if(flag.raw < 9U) {
			return FireballClassification::transfer;
		}
		return FireballClassification::explosion;
	}

	friend bool operator==(const FireballFact &, const FireballFact &) = default;
};

struct ShotPairSpawned {
	ShotPairSlot slot{};
	PlayerId player_id = PlayerId::p1;

	friend bool operator==(
		const ShotPairSpawned &,
		const ShotPairSpawned &
	) = default;
};

// Borrowed views over one exact MAIN phase boundary. They are valid only for
// the duration of the observer callback; every accessor returns a fact by value.
class ShotsEnemiesView {
public:
	static constexpr std::size_t shot_count = 32U;
	static constexpr std::size_t enemy_count = 40U;

	[[nodiscard]] MainPhaseContext context() const noexcept;
	[[nodiscard]] std::optional<ShotPairFact> shot(
		ShotPairSlot slot
	) const noexcept;
	[[nodiscard]] std::optional<EnemyFact> enemy(
		EnemySlot slot
	) const noexcept;

private:
	friend struct detail::ObserverAccess;

	explicit ShotsEnemiesView(
		const detail::DeterministicState *state
	) noexcept : state_(state)
	{}

	const detail::DeterministicState *state_;
};

class FireballsView {
public:
	static constexpr std::size_t fireball_count = 24U;

	[[nodiscard]] MainPhaseContext context() const noexcept;
	[[nodiscard]] std::optional<FireballFact> fireball(
		FireballSlot slot
	) const noexcept;

private:
	friend struct detail::ObserverAccess;

	explicit FireballsView(
		const detail::DeterministicState *state
	) noexcept : state_(state)
	{}

	const detail::DeterministicState *state_;
};

class GameplayObserver {
public:
	virtual ~GameplayObserver() = default;

	// Callbacks are synchronous. They must not reenter or mutate the Game that
	// issued them; borrowed phase views expire when their callback returns.
	virtual void shot_pair_spawned(const ShotPairSpawned &) noexcept
	{}
	virtual void observe(const ShotsEnemiesView &) noexcept
	{}
	virtual void observe(const FireballsView &) noexcept
	{}
};

} // namespace th03

#endif

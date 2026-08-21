#ifndef TH03_CORE_GAMEPLAY_RULESET_HPP
#define TH03_CORE_GAMEPLAY_RULESET_HPP

#include <cstdint>
#include <string_view>

namespace th03 {

enum class GameplayRulesetId : std::uint8_t {
	stock = 0U,
	dopamine_test_v1 = 1U,
};

enum class FormationSelectionPolicy : std::uint8_t {
	stock_randomized = 0U,
	locked = 1U,
};

enum class CpuGaugeAttackPolicy : std::uint8_t {
	stock = 0U,
	level_two_only = 1U,
};

enum class SpellCutinPolicy : std::uint8_t {
	stock_hold = 0U,
	no_hold = 1U,
};

// One immutable deterministic contract selected before a Game exists. New
// mechanics belong in a named/versioned object rather than product-mode flags
// scattered through the frame pipeline.
struct GameplayRuleset final {
	GameplayRulesetId id = GameplayRulesetId::stock;
	std::uint8_t version = 1U;
	FormationSelectionPolicy formations =
		FormationSelectionPolicy::stock_randomized;
	std::uint8_t locked_formation = 0U;
	CpuGaugeAttackPolicy cpu_gauge_attacks = CpuGaugeAttackPolicy::stock;
	SpellCutinPolicy spell_cutins = SpellCutinPolicy::stock_hold;

	friend bool operator==(const GameplayRuleset &, const GameplayRuleset &) =
		default;
};

inline constexpr GameplayRuleset stock_gameplay_ruleset{};

// Stock ENEDAT formation 10 enters as one enemy, then rows of two, three, and
// four above it: the requested downward-pointing triangle.
inline constexpr GameplayRuleset dopamine_test_v1_ruleset{
	.id = GameplayRulesetId::dopamine_test_v1,
	.version = 1U,
	.formations = FormationSelectionPolicy::locked,
	.locked_formation = 10U,
	.cpu_gauge_attacks = CpuGaugeAttackPolicy::level_two_only,
	.spell_cutins = SpellCutinPolicy::no_hold,
};

[[nodiscard]] constexpr bool valid_gameplay_ruleset(
	const GameplayRuleset &ruleset
) noexcept
{
	return ruleset == stock_gameplay_ruleset ||
		ruleset == dopamine_test_v1_ruleset;
}

[[nodiscard]] constexpr bool stock_gameplay_rules(
	const GameplayRuleset &ruleset
) noexcept
{
	return ruleset == stock_gameplay_ruleset;
}

[[nodiscard]] constexpr std::string_view gameplay_ruleset_name(
	const GameplayRuleset &ruleset
) noexcept
{
	return ruleset == dopamine_test_v1_ruleset ? "dopamine-test-v1" :
		(ruleset == stock_gameplay_ruleset ? "stock" : "unsupported");
}

} // namespace th03

#endif

#include "th03/app/new_classic_practice.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <utility>

namespace th03::app {
namespace {

constexpr std::uint8_t preset_vs = 0U;
constexpr std::uint8_t preset_story = 1U;
constexpr std::uint8_t stock_vs_rules = 0xFFU;

constexpr std::array<std::string_view, 6U> main_labels{
	"Mode", "Stage", "Round", "Advanced Settings", "Reset Defaults", "Start",
};
constexpr std::array<std::string_view, 12U> advanced_labels{
	"CPU Timer", "Round Speed", "Bullet Bonus", "P1 Spell Rank",
	"CPU Spell Rank", "P1 Start Gauge", "CPU Start Gauge", "Boss Rank",
	"CPU Damage", "Stock", "Extends Gained", "Back",
};
constexpr std::array<std::uint8_t, 9U> stage_six_opponent{
	1U, 0U, 0U, 2U, 0U, 3U, 5U, 4U, 6U,
};

[[nodiscard]] std::uint8_t default_round_speed(
	std::uint8_t rank,
	std::uint8_t round
) noexcept
{
	std::uint16_t result = 0U;
	switch(rank) {
	case 0U: result = static_cast<std::uint16_t>(round) << 4U; break;
	case 1U: result = static_cast<std::uint16_t>(round) << 5U; break;
	case 2U: result = static_cast<std::uint16_t>(
		(static_cast<std::uint16_t>(round) << 5U) + 0x20U
	); break;
	case 3U: result = 0x60U; break;
	default: break;
	}
	return static_cast<std::uint8_t>(std::min<std::uint16_t>(result, 0x7FU));
}

[[nodiscard]] std::uint8_t default_bullet_speed(std::uint8_t rank) noexcept
{
	return rank == 2U ? 8U : rank == 3U ? 0x18U : 0U;
}

[[nodiscard]] std::uint8_t default_boss_level(
	std::uint8_t rank,
	std::uint8_t stage,
	std::uint8_t round
) noexcept
{
	std::uint16_t level = stage;
	if(rank == 0U) {
		level = static_cast<std::uint16_t>((stage / 2U) + round);
	} else {
		level = static_cast<std::uint16_t>(stage + (round * 2U));
		if(rank == 2U) {
			level += 2U;
		} else if(rank == 3U) {
			level += 8U;
		}
	}
	return static_cast<std::uint8_t>(std::min<std::uint16_t>(level, 15U));
}

[[nodiscard]] std::uint8_t default_cpu_damage(std::uint8_t stage) noexcept
{
	return stage < 2U ? 3U : stage < 4U ? 2U : stage < 6U ? 1U : 0U;
}

[[nodiscard]] std::string decimal(std::uint16_t value)
{
	return std::to_string(value);
}

[[nodiscard]] std::string q4(std::uint8_t value)
{
	const auto fraction = static_cast<std::uint16_t>((value & 0x0FU) * 625U);
	auto result = decimal(static_cast<std::uint8_t>(value >> 4U));
	result += '.';
	result += static_cast<char>('0' + ((fraction / 1000U) % 10U));
	result += static_cast<char>('0' + ((fraction / 100U) % 10U));
	result += static_cast<char>('0' + ((fraction / 10U) % 10U));
	result += static_cast<char>('0' + (fraction % 10U));
	return result;
}

[[nodiscard]] bool valid_base_config(
	const formats::ReplayPracticeConfig &config
) noexcept
{
	const bool base = config.preset <= 1U && config.cpu_timer <= 2U &&
		config.round_speed <= 0x7FU && config.bullet_speed <= 0x7FU &&
		config.p1_spell >= 1U && config.p1_spell <= 16U &&
		config.cpu_spell >= 1U && config.cpu_spell <= 16U &&
		config.boss_level <= 15U && config.cpu_damage <= 3U &&
		config.p1_gauge >= 64U && config.cpu_gauge >= 64U;
	if(!base) {
		return false;
	}
	if(config.preset == preset_vs) {
		return config.stage == 0U && config.round == 0U &&
			config.stock == stock_vs_rules && config.extends_gained == 0U;
	}
	return config.stage < 9U && config.round <= 5U && config.stock <= 4U &&
		config.extends_gained <= 2U &&
		config.stock <= static_cast<std::uint8_t>(2U + config.extends_gained);
}

} // namespace

bool new_classic_practice_is_exact_vs_default(
	const formats::ReplayPracticeConfig &config,
	std::uint8_t rank
) noexcept
{
	return config.preset == preset_vs && config.stage == 0U &&
		config.round == 0U && config.stock == stock_vs_rules &&
		config.extends_gained == 0U && config.cpu_timer == 0U &&
		config.round_speed == default_round_speed(rank, 0U) &&
		config.bullet_speed == default_bullet_speed(rank) &&
		config.p1_spell == 1U && config.cpu_spell == 1U &&
		config.p1_gauge == 64U && config.cpu_gauge == 64U &&
		config.boss_level == default_boss_level(rank, 0U, 0U) &&
		config.cpu_damage == 0U;
}

std::expected<GameInit, Error> prepare_new_classic_practice_init(
	GameInit selected_resident,
	const formats::ReplayPracticeConfig &config,
	const GameContent &content
)
{
	if(!valid_base_config(config) ||
		selected_resident.game_mode != game_mode::vs_1p_cpu ||
		selected_resident.is_cpu != std::array<std::uint8_t, 2U>{0U, 1U}) {
		return std::unexpected(Error{
			ErrorCode::invalid_value,
			"New Classic Practice configuration is invalid",
		});
	}
	selected_resident.story_stage = config.stage;
	selected_resident.story_lives = config.stock == stock_vs_rules ? 0U :
		config.stock;
	selected_resident.runtime_snapshot.reset();
	selected_resident.compact_runtime_snapshot.reset();
	selected_resident.round_start_snapshot.reset();
	selected_resident.practice_rules.reset();

	auto baseline = Game::create(
		selected_resident, content,
		GameOptions{.input_protocol = InputProtocol::recorded_replay}
	);
	if(!baseline) {
		return std::unexpected(baseline.error());
	}
	const auto observation = baseline->round_start_observation();
	auto init = observation.start;
	init.compact_runtime_snapshot = CompactRuntimeSnapshot{
		.round_reset_seed = observation.effective_reset_seed,
		.random_seed = observation.runtime.random_seed,
		.first_formation = observation.runtime.formation_type_ring[0U],
		.players = observation.runtime.players,
	};
	init.round_start_snapshot = observation.round;
	init.practice_rules = PracticeRules{
		.uses_stock = config.stock != stock_vs_rules,
		.retry_cpu_timer = static_cast<CpuTimerPolicy>(config.cpu_timer),
		.retry_cpu_damage = config.cpu_damage,
	};
	init.round_start_snapshot->state.round_id = config.round;
	init.round_start_snapshot->state.rounds_won = {};
	init.round_start_snapshot->state.score = {};
	init.round_start_snapshot->state.round_speed = config.round_speed;
	init.round_start_snapshot->state.bullet_base_speed = config.bullet_speed;
	init.round_start_snapshot->state.gauge_attack_level = {
		config.p1_spell, config.cpu_spell,
	};
	init.round_start_snapshot->state.boss_attack_level = config.boss_level;
	init.round_start_snapshot->state.cpu_hit_damage_additional =
		config.cpu_damage;
	init.round_start_snapshot->state.extends_gained =
		config.extends_gained >= 2U ? 0xFFU : config.extends_gained;
	const auto safety = practice_cpu_safety_frames(
		static_cast<CpuTimerPolicy>(config.cpu_timer),
		init.rank,
		config.stage,
		config.round,
		init.is_cpu
	);
	if(config.initial_cpu_safety_frames != safety) {
		return std::unexpected(Error{
			ErrorCode::invalid_value,
			"New Classic Practice CPU safety does not match its policy",
		});
	}
	init.round_start_snapshot->state.cpu_safety_frames = {safety, safety};
	init.compact_runtime_snapshot->players[0U].gauge_avail =
		static_cast<std::uint16_t>(config.p1_gauge) * 16U;
	init.compact_runtime_snapshot->players[1U].gauge_avail =
		static_cast<std::uint16_t>(config.cpu_gauge) * 16U;

	if(auto validated = Game::create(
		init, content,
		GameOptions{.input_protocol = InputProtocol::recorded_replay}
	); !validated) {
		return std::unexpected(validated.error());
	}
	return init;
}

NewClassicPracticeMenu::NewClassicPracticeMenu(
	GameInit selected_resident
) noexcept :
	selected_resident_(std::move(selected_resident))
{
	config_.preset = preset_story;
	config_.stage = fixed_stage();
	config_.round = 0U;
	reset_defaults();
}

std::uint8_t NewClassicPracticeMenu::fixed_stage() const noexcept
{
	const auto p1 = static_cast<std::uint8_t>(
		(selected_resident_.paletted_character[0U] - 1) / 2
	);
	const auto cpu = static_cast<std::uint8_t>(
		(selected_resident_.paletted_character[1U] - 1) / 2
	);
	if(cpu == 7U) {
		return 7U;
	}
	if(cpu == 8U) {
		return 8U;
	}
	if(p1 < stage_six_opponent.size() && cpu == stage_six_opponent[p1]) {
		return 6U;
	}
	return 0xFFU;
}

void NewClassicPracticeMenu::refresh_safety() noexcept
{
	config_.initial_cpu_safety_frames = practice_cpu_safety_frames(
		static_cast<CpuTimerPolicy>(config_.cpu_timer),
		selected_resident_.rank,
		config_.stage,
		config_.round,
		{0U, 1U}
	);
}

void NewClassicPracticeMenu::reset_defaults() noexcept
{
	const auto fixed = fixed_stage();
	if(config_.preset == preset_vs) {
		config_.stage = 0U;
		config_.round = 0U;
		config_.cpu_timer = 0U;
		config_.p1_spell = 1U;
		config_.cpu_spell = 1U;
		config_.cpu_damage = 0U;
		config_.stock = stock_vs_rules;
		config_.extends_gained = 0U;
	} else {
		if(fixed != 0xFFU) {
			config_.stage = fixed;
		} else if(config_.stage >= 6U) {
			config_.stage = 0U;
		}
		config_.cpu_timer = 1U;
		config_.p1_spell = static_cast<std::uint8_t>(config_.stage + 1U);
		config_.cpu_spell = static_cast<std::uint8_t>(config_.stage + 1U);
		config_.cpu_damage = default_cpu_damage(config_.stage);
		config_.stock = 2U;
		config_.extends_gained = 0U;
	}
	config_.round_speed = default_round_speed(
		selected_resident_.rank, config_.round
	);
	config_.bullet_speed = default_bullet_speed(selected_resident_.rank);
	config_.p1_gauge = 64U;
	config_.cpu_gauge = 64U;
	config_.boss_level = default_boss_level(
		selected_resident_.rank, config_.stage, config_.round
	);
	refresh_safety();
}

std::uint8_t NewClassicPracticeMenu::row_count() const noexcept
{
	return page_ == Page::main ? 6U : 12U;
}

void NewClassicPracticeMenu::step_value(bool forward) noexcept
{
	if(page_ == Page::main) {
		switch(selected_) {
		case 0U:
			config_.preset = static_cast<std::uint8_t>(1U - config_.preset);
			reset_defaults();
			return;
		case 1U:
			if(config_.preset == preset_story && fixed_stage() == 0xFFU) {
				config_.stage = forward ?
					static_cast<std::uint8_t>((config_.stage + 1U) % 6U) :
					static_cast<std::uint8_t>(config_.stage == 0U ? 5U :
						config_.stage - 1U);
				reset_defaults();
			}
			return;
		case 2U:
			if(config_.preset == preset_story) {
				config_.round = forward ?
					static_cast<std::uint8_t>((config_.round + 1U) % 6U) :
					static_cast<std::uint8_t>(config_.round == 0U ? 5U :
						config_.round - 1U);
				reset_defaults();
			}
			return;
		default: return;
		}
	}

	switch(selected_) {
	case 0U:
		config_.cpu_timer = forward ?
			static_cast<std::uint8_t>((config_.cpu_timer + 1U) % 3U) :
			static_cast<std::uint8_t>(config_.cpu_timer == 0U ? 2U :
				config_.cpu_timer - 1U);
		break;
	case 1U:
		if(forward && config_.round_speed < 0x7FU) {
			++config_.round_speed;
		} else if(!forward && config_.round_speed > 0U) {
			--config_.round_speed;
		}
		break;
	case 2U:
		if(forward && config_.bullet_speed < 0x7FU) {
			++config_.bullet_speed;
		} else if(!forward && config_.bullet_speed > 0U) {
			--config_.bullet_speed;
		}
		break;
	case 3U:
		config_.p1_spell = forward ?
			static_cast<std::uint8_t>(config_.p1_spell == 16U ? 1U :
				config_.p1_spell + 1U) :
			static_cast<std::uint8_t>(config_.p1_spell == 1U ? 16U :
				config_.p1_spell - 1U);
		break;
	case 4U:
		config_.cpu_spell = forward ?
			static_cast<std::uint8_t>(config_.cpu_spell == 16U ? 1U :
				config_.cpu_spell + 1U) :
			static_cast<std::uint8_t>(config_.cpu_spell == 1U ? 16U :
				config_.cpu_spell - 1U);
		break;
	case 5U:
		config_.p1_gauge = forward ?
			static_cast<std::uint8_t>(config_.p1_gauge == 0xFFU ? 64U :
				config_.p1_gauge + 1U) :
			static_cast<std::uint8_t>(config_.p1_gauge == 64U ? 0xFFU :
				config_.p1_gauge - 1U);
		break;
	case 6U:
		config_.cpu_gauge = forward ?
			static_cast<std::uint8_t>(config_.cpu_gauge == 0xFFU ? 64U :
				config_.cpu_gauge + 1U) :
			static_cast<std::uint8_t>(config_.cpu_gauge == 64U ? 0xFFU :
				config_.cpu_gauge - 1U);
		break;
	case 7U:
		if(forward && config_.boss_level < 15U) {
			++config_.boss_level;
		} else if(!forward && config_.boss_level > 0U) {
			--config_.boss_level;
		}
		break;
	case 8U:
		config_.cpu_damage = forward ?
			static_cast<std::uint8_t>((config_.cpu_damage + 1U) % 4U) :
			static_cast<std::uint8_t>(config_.cpu_damage == 0U ? 3U :
				config_.cpu_damage - 1U);
		break;
	case 9U:
		if(config_.preset == preset_story) {
			config_.stock = forward ?
				static_cast<std::uint8_t>((config_.stock + 1U) % 5U) :
				static_cast<std::uint8_t>(config_.stock == 0U ? 4U :
					config_.stock - 1U);
			if(config_.stock > 2U && config_.extends_gained <
				config_.stock - 2U) {
				config_.extends_gained = static_cast<std::uint8_t>(
					config_.stock - 2U
				);
			}
		}
		break;
	case 10U:
		if(config_.preset == preset_story) {
			config_.extends_gained = forward ?
				static_cast<std::uint8_t>((config_.extends_gained + 1U) % 3U) :
				static_cast<std::uint8_t>(config_.extends_gained == 0U ? 2U :
					config_.extends_gained - 1U);
			config_.stock = std::min<std::uint8_t>(
				config_.stock,
				static_cast<std::uint8_t>(2U + config_.extends_gained)
			);
		}
		break;
	default: break;
	}
	refresh_safety();
}

NewClassicPracticeStatus NewClassicPracticeMenu::consume(
	NewClassicPracticeInput input
) noexcept
{
	if(status_ != NewClassicPracticeStatus::running) {
		return status_;
	}
	if(input_locked_) {
		input_locked_ = input.raw_any;
		return status_;
	}
	if(input.cancel) {
		if(page_ == Page::advanced) {
			page_ = Page::main;
			selected_ = 3U;
			input_locked_ = true;
		} else {
			status_ = NewClassicPracticeStatus::canceled;
		}
		return status_;
	}
	if(input.up || input.down) {
		const auto count = row_count();
		selected_ = input.up ?
			static_cast<std::uint8_t>(selected_ == 0U ? count - 1U :
				selected_ - 1U) :
			static_cast<std::uint8_t>((selected_ + 1U) % count);
		input_locked_ = true;
		return status_;
	}
	if(input.left || input.right) {
		step_value(input.right);
		input_locked_ = true;
		return status_;
	}
	if(!input.confirm) {
		return status_;
	}
	if(page_ == Page::main && selected_ == 3U) {
		page_ = Page::advanced;
		selected_ = 0U;
		input_locked_ = true;
	} else if(page_ == Page::main && selected_ == 4U) {
		reset_defaults();
		input_locked_ = true;
	} else if(page_ == Page::main && selected_ == 5U) {
		refresh_safety();
		status_ = NewClassicPracticeStatus::started;
	} else if(page_ == Page::advanced && selected_ == 11U) {
		page_ = Page::main;
		selected_ = 3U;
		input_locked_ = true;
	}
	return status_;
}

void NewClassicPracticeMenu::draw() noexcept
{
	constexpr auto layer = present::ComposedPixelLayer::text;
	canvas_.fill_rect(72U, 32U, 496U, 336U, 0U, layer);
	canvas_.draw_text_centered(
		page_ == Page::main ? "VS Setup" : "Advanced Settings",
		320U, 48U, 2U, 6U, layer
	);
	const auto &labels = page_ == Page::main ?
		std::span<const std::string_view>(main_labels) :
		std::span<const std::string_view>(advanced_labels);
	const auto value = [&](std::size_t row) -> std::string {
		if(page_ == Page::main) {
			switch(row) {
			case 0U: return config_.preset == preset_vs ? "VS" : "Practice";
			case 1U: return decimal(static_cast<std::uint16_t>(config_.stage + 1U));
			case 2U: return decimal(static_cast<std::uint16_t>(config_.round + 1U)) +
				(config_.round == 5U ? "+" : "");
			default: return {};
			}
		}
		switch(row) {
		case 0U: return config_.cpu_timer == 0U ? "VS Rules" :
			config_.cpu_timer == 1U ? "Story" : "Infinite";
		case 1U: return q4(config_.round_speed);
		case 2U: return q4(config_.bullet_speed);
		case 3U: return decimal(config_.p1_spell);
		case 4U: return decimal(config_.cpu_spell);
		case 5U: return decimal(config_.p1_gauge);
		case 6U: return decimal(config_.cpu_gauge);
		case 7U: return decimal(static_cast<std::uint16_t>(
			config_.boss_level >= 15U ? 16U : config_.boss_level + 1U
		));
		case 8U: return decimal(config_.cpu_damage);
		case 9U: return config_.stock == stock_vs_rules ? "VS Rules" :
			decimal(config_.stock);
		case 10U: return config_.preset == preset_vs ? "VS Rules" :
			decimal(config_.extends_gained);
		default: return {};
		}
	};
	const auto top = page_ == Page::main ? 104U : 84U;
	const auto step = page_ == Page::main ? 38U : 22U;
	for(std::size_t row = 0U; row < labels.size(); ++row) {
		const bool selected = row == selected_;
		const auto color = static_cast<std::uint8_t>(selected ? 6U : 7U);
		canvas_.draw_text(selected ? ">" : " ", 104U, top + row * step,
			1U, color, layer);
		canvas_.draw_text(labels[row], 128U, top + row * step, 1U, color, layer);
		const auto text = value(row);
		if(!text.empty()) {
			canvas_.draw_text(text, 392U, top + row * step, 1U,
				color, layer);
		}
	}
	canvas_.draw_text_centered(
		"Arrows: Select/Change  Z: Confirm  Esc: Back",
		320U, 344U, 1U, 5U, layer
	);
}

present::SourceSurface NewClassicPracticeMenu::compose_over(
	present::SourceSurface base
) noexcept
{
	canvas_.copy_from(base);
	draw();
	return canvas_.surface();
}

NewClassicPracticeStatus NewClassicPracticeMenu::status() const noexcept
{
	return status_;
}

const formats::ReplayPracticeConfig &NewClassicPracticeMenu::config() const noexcept
{
	return config_;
}

bool NewClassicPracticeMenu::practice_active() const noexcept
{
	return !new_classic_practice_is_exact_vs_default(
		config_, selected_resident_.rank
	);
}

} // namespace th03::app

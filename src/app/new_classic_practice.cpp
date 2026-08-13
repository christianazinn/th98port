#include "th03/app/new_classic_practice.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace th03::app {
namespace {

constexpr std::uint8_t preset_vs = 0U;
constexpr std::uint8_t preset_story = 1U;
constexpr std::uint8_t stock_vs_rules = 0xFFU;

constexpr present::Index4 practice_color_header = 10U;
constexpr present::Index4 practice_color_footer = practice_color_header;
constexpr present::Index4 practice_color_label = 13U;
constexpr present::Index4 practice_color_selected = 14U;
constexpr present::Index4 practice_color_disabled = 11U;
constexpr present::Index4 practice_color_value = 15U;

constexpr std::int32_t practice_header_top = 32;
constexpr std::int32_t practice_footer_top = 320;
constexpr std::int32_t practice_main_label_left = 240;
constexpr std::int32_t practice_main_value_left = 328;
constexpr std::int32_t practice_advanced_label_left = 112;
constexpr std::int32_t practice_advanced_value_left = 336;
constexpr std::int32_t practice_cursor_gap = 16;
constexpr std::int32_t replay_numeric_cell_width = 12;
// palette_black_out(1): one alignment boundary, 17 PaletteTone values at one
// VSync apiece, then the final tone-0 publication after the loop.
constexpr std::uint8_t fade_boundary_count = 19U;
constexpr present::Rect full_screen{
	0, 0, present::graphics_width, present::graphics_height,
};

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
	default: result = 0x60U; break;
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
	} else if(rank == 1U) {
		level = static_cast<std::uint16_t>(stage + (round * 2U));
	} else if(rank == 2U) {
		level = static_cast<std::uint16_t>(stage + (round * 2U) + 2U);
	} else {
		level = static_cast<std::uint16_t>(stage + (round * 2U) + 8U);
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

[[nodiscard]] bool put_fixed_n(
	const NewClassicMenuFont &font,
	present::Raster &raster,
	std::int32_t left,
	std::int32_t top,
	std::string_view text,
	std::size_t count,
	present::Index4 color
) noexcept
{
	const auto size = std::min(text.size(), count);
	for(std::size_t index = 0U; index < size; ++index) {
		const auto character = text[index];
		const auto inset = character == '1' ? 2 : 0;
		if(!font.put_n(raster, left + inset, top, text.substr(index), 1U, color)) {
			return false;
		}
		left += replay_numeric_cell_width;
	}
	return true;
}

[[nodiscard]] bool put_fixed_q4(
	const NewClassicMenuFont &font,
	present::Raster &raster,
	std::int32_t left,
	std::int32_t top,
	std::string_view text,
	present::Index4 color
) noexcept
{
	if(text.size() != 6U || !put_fixed_n(
		font, raster, left, top, text, 1U, color
	)) {
		return false;
	}
	left += replay_numeric_cell_width;
	if(!font.put_n(raster, left, top, text.substr(1U), 1U, color)) {
		return false;
	}
	left += font.width_n(text.substr(1U), 1U);
	return put_fixed_n(font, raster, left, top, text.substr(2U), 4U, color);
}

[[nodiscard]] present::Page other_page(present::Page page) noexcept
{
	return page == present::Page::zero ? present::Page::one :
		present::Page::zero;
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
	NewClassicPracticeMenu(std::move(selected_resident), {})
{}

NewClassicPracticeMenu::NewClassicPracticeMenu(
	GameInit selected_resident,
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets
) noexcept :
	selected_resident_(std::move(selected_resident)),
	patch_assets_(std::move(patch_assets)),
	composed_pixels_(present::graphics_page_pixels)
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

bool NewClassicPracticeMenu::row_enabled(std::uint8_t row) const noexcept
{
	if(page_ == Page::main) {
		if(row == 1U) {
			return config_.preset == preset_story && fixed_stage() == 0xFFU;
		}
		if(row == 2U) {
			return config_.preset == preset_story;
		}
		return true;
	}
	if(row == 9U || row == 10U) {
		return config_.preset == preset_story;
	}
	return true;
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
	consume_surface_changed_ = false;
	if(status_ == NewClassicPracticeStatus::start_fade) {
		advance_start_fade();
		return status_;
	}
	if(status_ == NewClassicPracticeStatus::cancel_wait_release) {
		if(cancel_release_first_poll_) {
			cancel_release_first_poll_ = false;
			return status_;
		}
		if(!input.raw_any) {
			status_ = NewClassicPracticeStatus::canceled;
		}
		return status_;
	}
	if(status_ != NewClassicPracticeStatus::running) {
		return status_;
	}
	if(input_locked_) {
		input_locked_ = input.raw_any;
		return status_;
	}
	if(input.up || input.down) {
		const auto count = row_count();
		selected_ = input.up ?
			static_cast<std::uint8_t>(selected_ == 0U ? count - 1U :
				selected_ - 1U) :
			static_cast<std::uint8_t>((selected_ + 1U) % count);
	} else if(input.left || input.right) {
		if(row_enabled(selected_)) {
			// Source priority is Left before Right when both bits are set.
			step_value(!input.left);
		}
	} else if(input.confirm) {
		if(page_ == Page::main && selected_ == 3U) {
			page_ = Page::advanced;
			selected_ = 0U;
		} else if(page_ == Page::main && selected_ == 4U) {
			reset_defaults();
		} else if(page_ == Page::main && selected_ == 5U) {
			refresh_safety();
			status_ = NewClassicPracticeStatus::start_fade;
			fade_boundary_ = 0U;
			palette_tone_ = 100;
		} else if(page_ == Page::advanced && selected_ == 11U) {
			page_ = Page::main;
			selected_ = 3U;
		}
	} else if(input.cancel) {
		if(page_ == Page::advanced) {
			page_ = Page::main;
			selected_ = 3U;
		} else {
			// The patch restores TLSL.RGB immediately, then renders one final
			// curves-only frame through the retained selector bridge.
			status_ = NewClassicPracticeStatus::cancel_frame;
			consume_surface_changed_ = true;
		}
	}
	input_locked_ = input.raw_any;
	return status_;
}

void NewClassicPracticeMenu::advance_start_fade() noexcept
{
	if(fade_boundary_ != 0U && fade_boundary_ <= 17U) {
		const auto pair = static_cast<std::int16_t>(fade_boundary_ - 1U);
		palette_tone_ = static_cast<std::int16_t>(100 - (pair * 6));
		consume_surface_changed_ = true;
	}
	++fade_boundary_;
	if(fade_boundary_ < fade_boundary_count) {
		return;
	}
	palette_tone_ = 0;
	consume_surface_changed_ = true;
	status_ = NewClassicPracticeStatus::started;
}

void NewClassicPracticeMenu::mark_background_published() noexcept
{
	background_published_ = true;
	if(status_ == NewClassicPracticeStatus::cancel_frame) {
		// The extra frame is now visible. The caller next reproduces the
		// source's frame-delay/input-mode release loop without redrawing it.
		background_published_ = false;
		cancel_release_first_poll_ = true;
		status_ = NewClassicPracticeStatus::cancel_wait_release;
	}
}

bool NewClassicPracticeMenu::draw() noexcept
{
	if(!patch_assets_ ||
		!overlay_raster_.set_access_page(present::Page::zero) ||
		!overlay_raster_.clear(full_screen, 0U)) {
		return false;
	}
	const auto &font = patch_assets_->menu_font;
	if(!font.put_centered(
		overlay_raster_, 320, practice_header_top,
		page_ == Page::main ? "VS SETUP" : "ADVANCED SETTINGS",
		practice_color_header
	) || !font.put_centered(
		overlay_raster_, 320, practice_footer_top,
		"Arrows: Select / Change   Z: Confirm   Esc: Back",
		practice_color_footer
	)) {
		return false;
	}
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
		case 7U:
			return decimal(static_cast<std::uint16_t>(
				config_.boss_level >= 15U ? 16U : config_.boss_level + 1U
			)) + "  raw " + decimal(config_.boss_level);
		case 8U: return decimal(config_.cpu_damage);
		case 9U: return config_.stock == stock_vs_rules ? "VS Rules" :
			decimal(config_.stock);
		case 10U: return config_.preset == preset_vs ? "VS Rules" :
			decimal(config_.extends_gained);
		default: return {};
		}
	};
	for(std::size_t row = 0U; row < labels.size(); ++row) {
		const bool selected = row == selected_;
		const bool command = page_ == Page::main ? row >= 3U : row == 11U;
		const auto enabled = row_enabled(static_cast<std::uint8_t>(row));
		auto label_left = page_ == Page::main ?
			practice_main_label_left : practice_advanced_label_left;
		const auto value_left = page_ == Page::main ?
			practice_main_value_left : practice_advanced_value_left;
		const auto top = page_ == Page::main ?
			(row <= 3U ? 96 + static_cast<std::int32_t>(row * 16U) :
				176 + static_cast<std::int32_t>((row - 4U) * 16U)) :
			(row < 11U ? 64 + static_cast<std::int32_t>(row * 16U) : 256);
		if(command) {
			label_left = 320 - (font.width(labels[row]) / 2);
		}
		const auto label_color = static_cast<present::Index4>(
			selected ? practice_color_selected :
			(command ? practice_color_header : practice_color_label)
		);
		const auto value_color = static_cast<present::Index4>(
			!enabled ? practice_color_disabled :
			(selected ? practice_color_selected : practice_color_value)
		);
		if(selected && !font.put(
			overlay_raster_, label_left - practice_cursor_gap, top, ">",
			practice_color_selected
		)) {
			return false;
		}
		if(!font.put(
			overlay_raster_, label_left, top, labels[row], label_color
		)) {
			return false;
		}
		const auto text = value(row);
		if(!text.empty()) {
			const bool fixed_q4 = page_ == Page::advanced &&
				(row == 1U || row == 2U);
			if(fixed_q4 ? !put_fixed_q4(
				font, overlay_raster_, value_left, top, text, value_color
			) : !font.put(
				overlay_raster_, value_left, top, text, value_color
			)) {
				return false;
			}
		}
	}
	return true;
}

present::SourceSurface NewClassicPracticeMenu::compose_over(
	present::SourceSurface base,
	bool base_is_retained_selection_frame
) noexcept
{
	if(!background_published_) {
		return base;
	}
	if(!patch_assets_ || composed_pixels_.size() != base.pixels.size() ||
		!draw()) {
		return base;
	}
	std::ranges::copy(base.pixels, composed_pixels_.begin());
	const auto overlay = overlay_raster_.graphics(present::Page::zero);
	for(std::size_t index = 0U; index < overlay.size(); ++index) {
		if(overlay[index] != 0U) {
			composed_pixels_[index] = {
				present::ComposedPixelLayer::graphics, overlay[index],
			};
		}
	}
	if(base_is_retained_selection_frame) {
		// The retained selector already performed the source page flip after
		// drawing the curves. The overlay belongs to that published page.
		published_page_ = base.shown_page;
		published_page_initialized_ = true;
	} else if(!published_page_initialized_) {
		published_page_ = other_page(base.shown_page);
		published_page_initialized_ = true;
	} else {
		published_page_ = other_page(published_page_);
	}
	auto palette = base.graphics_palette;
	auto reduced_palette = base.reduced_flash_palette;
	// The Cancel input is sampled after the current Practice frame has already
	// been drawn. Its overlay therefore keeps the Practice palette for this
	// publication; TLSL colors return on the following curves-only frame when
	// mark_background_published() clears background_published_.
	palette.colors[practice_color_selected] = {2U, 14U, 15U};
	palette.colors[practice_color_disabled] = {8U, 8U, 8U};
	reduced_palette.colors[practice_color_selected] = {2U, 14U, 15U};
	reduced_palette.colors[practice_color_disabled] = {8U, 8U, 8U};
	return {
		.pixels = std::span<
			const present::ComposedPixel, present::graphics_page_pixels
		>(composed_pixels_.data(), composed_pixels_.size()),
		.graphics_palette = palette,
		.shown_page = published_page_,
		.raster_mode = base.raster_mode,
		.effective_scroll_y = base.effective_scroll_y,
		.palette_tone = palette_tone_,
		.reduced_flash_palette = reduced_palette,
		.oracle_hashes = {},
		.host_overlay = false,
		.reduced_flash_applied = false,
	};
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

bool NewClassicPracticeMenu::has_patch_assets() const noexcept
{
	return patch_assets_ != nullptr;
}

bool NewClassicPracticeMenu::consume_surface_changed() const noexcept
{
	return consume_surface_changed_;
}

} // namespace th03::app

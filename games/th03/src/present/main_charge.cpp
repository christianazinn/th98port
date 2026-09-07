#include "th03/present/main_extra_charge.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "th03/core/numeric.hpp"
#include "th03/present/main_static.hpp"
#include "th03/present/vectors.hpp"

namespace th03::present {
namespace {

enum class Character : std::uint8_t {
	reimu,
	mima,
	marisa,
	ellen,
	kotohime,
	kana,
	rikako,
	chiyuri,
	yumemi,
};

constexpr std::array<Sprite16HorizontalClip, 2U> playfield_clips{{
	{0, 319},
	{320, 639},
}};
constexpr GrcgClipRect full_grcg_clip{0, 0, 639, 199};
constexpr std::array<ReimuChargePoint, 4U> reimu_points{{
	ReimuChargePoint::node_0,
	ReimuChargePoint::node_2,
	ReimuChargePoint::node_4,
	ReimuChargePoint::node_6,
}};
constexpr std::array<KanaChargePoint, 4U> kana_points{{
	KanaChargePoint::node_0,
	KanaChargePoint::node_4,
	KanaChargePoint::node_8,
	KanaChargePoint::node_12,
}};

[[nodiscard]] constexpr std::optional<std::uint8_t> player_index(
	PlayerId player
) noexcept
{
	if(player == PlayerId::p1) {
		return std::uint8_t{0U};
	}
	if(player == PlayerId::p2) {
		return std::uint8_t{1U};
	}
	return std::nullopt;
}

[[nodiscard]] constexpr std::optional<Character> character_index(
	std::int8_t paletted_character
) noexcept
{
	if(paletted_character < 1 || paletted_character > 18) {
		return std::nullopt;
	}
	return static_cast<Character>((paletted_character - 1) / 2);
}

[[nodiscard]] constexpr std::int16_t wrap_word(std::int32_t value) noexcept
{
	return exact::from_bits<std::int16_t>(static_cast<std::uint16_t>(value));
}

[[nodiscard]] constexpr std::int16_t shift_right(
	std::int16_t value,
	unsigned count
) noexcept
{
	return exact::arithmetic_shift_right(value, count);
}

[[nodiscard]] constexpr std::int16_t foreground_x(
	const MainPhaseContext &context,
	std::int16_t raw_x,
	std::uint8_t player
) noexcept
{
	return wrap_word(
		static_cast<std::int32_t>(shift_right(raw_x, 4U)) +
		static_cast<std::int32_t>(main_playfield_border) +
		static_cast<std::int32_t>(player * main_playfield_bordered_width) +
		context.playfield_fg_shift_x[player]
	);
}

[[nodiscard]] constexpr Sprite16AtlasRect source_rect(
	std::uint8_t player,
	std::uint16_t left,
	std::uint16_t raw_top,
	std::uint16_t width,
	std::uint16_t raw_height
) noexcept
{
	return {
		static_cast<std::uint16_t>(player * 320U + left),
		raw_top,
		width,
		raw_height,
	};
}

[[nodiscard]] bool put(
	Raster &raster,
	const Sprite16Staging &staging,
	Sprite16AtlasRect source,
	Point position,
	Sprite16HorizontalClip clip,
	Sprite16DrawStyle style = {}
) noexcept
{
	return sprite16_put(raster, staging, source, position, clip, style);
}

[[nodiscard]] bool render_reimu(
	Raster &raster,
	const Sprite16Staging &staging,
	const MainPhaseContext &context,
	std::uint8_t player,
	const std::array<ReimuChargeGroupFact, MainChargeShotView::reimu_group_count> &groups,
	const std::array<std::array<RawPoint, 4U>, MainChargeShotView::reimu_group_count> &points
) noexcept
{
	constexpr std::array<std::size_t, 4U> order{{3U, 2U, 1U, 0U}};
	constexpr std::array<std::uint8_t, 4U> phases{{0U, 1U, 2U, 3U}};
	constexpr std::array<std::optional<Index4>, 4U> colors{{
		std::nullopt, Index4{6U}, Index4{5U}, Index4{2U},
	}};
	for(std::size_t group = 0U; group < groups.size(); ++group) {
		if(groups[group].state == 0U) {
			continue;
		}
		for(const auto point : order) {
			const auto phase = static_cast<std::uint16_t>(
				(phases[point] + context.round_or_result_frame) & 3U
			);
			const auto &position = points[group][point];
			if(!put(
				raster,
				staging,
				source_rect(player, static_cast<std::uint16_t>(phase * 48U), 56U, 48U, 24U),
				{
					wrap_word(static_cast<std::int32_t>(foreground_x(
						context, position.x, player
					)) - 24),
					wrap_word(static_cast<std::int32_t>(shift_right(position.y, 4U)) - 8),
				},
				playfield_clips[player],
				{.monochrome_color = colors[point]}
			)) {
				return false;
			}
		}
	}
	return true;
}

[[nodiscard]] bool render_mima(
	Raster &raster,
	const Sprite16Staging &staging,
	const MainPhaseContext &context,
	std::uint8_t player,
	const MimaChargeStateFact &state,
	const std::array<MimaChargeNodeFact, MainChargeShotView::mima_node_count> &nodes
) noexcept
{
	if(state.active == 0U) {
		return true;
	}
	for(std::size_t node = 0U; node < nodes.size(); ++node) {
		if(nodes[node].active == 0U) {
			continue;
		}
		if(!put(
			raster,
			staging,
			source_rect(
				player,
				static_cast<std::uint16_t>(128U + (node % 3U) * 32U),
				0U,
				32U,
				8U
			),
			{
				wrap_word(static_cast<std::int32_t>(foreground_x(
					context, nodes[node].center_x, player
				)) - 16),
				wrap_word(static_cast<std::int32_t>(shift_right(
					nodes[node].center_y, 4U
				)) + 8),
			},
			playfield_clips[player]
		)) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] bool render_marisa(
	Raster &raster,
	const MainPhaseContext &context,
	std::uint8_t player,
	const MarisaChargeStateFact &state,
	const std::array<MarisaChargeNodeFact, MainChargeShotView::marisa_node_count> &nodes
) noexcept
{
	if(state.active == 0U) {
		return true;
	}
	constexpr std::array<GrcgTile, 3U> tiles{{
		{{0xAAU, 0x55U, 0x00U, 0xAAU}},
		{{0x55U, 0xAAU, 0x00U, 0xFFU}},
		{{0x55U, 0xFFU, 0x55U, 0xFFU}},
	}};
	constexpr std::array<std::size_t, 3U> begin{{11U, 6U, 3U}};
	constexpr std::array<std::size_t, 3U> end{{7U, 4U, 1U}};
	for(std::size_t band = 0U; band < tiles.size(); ++band) {
		std::int16_t last_left = 0;
		for(std::size_t node = begin[band];; --node) {
			const auto left = foreground_x(context, nodes[node].center_x, player);
			if(last_left != left && !grcg_boxfill(
				raster,
				{wrap_word(static_cast<std::int32_t>(left) - 2), 8},
				{
					wrap_word(static_cast<std::int32_t>(left) + 1),
					wrap_word(static_cast<std::int32_t>(shift_right(
						nodes[node].center_y, 5U
					)) + 8),
				},
				full_grcg_clip,
				tiles[band]
			)) {
				return false;
			}
			last_left = left;
			if(node == end[band]) {
				break;
			}
		}
	}
	const auto white = GrcgTile::solid(15U);
	if(!white) {
		return false;
	}
	const auto left = foreground_x(context, nodes[0U].center_x, player);
	return grcg_boxfill(
		raster,
		{wrap_word(static_cast<std::int32_t>(left) - 2), 8},
		{
			wrap_word(static_cast<std::int32_t>(left) + 1),
			wrap_word(static_cast<std::int32_t>(shift_right(
				nodes[0U].center_y, 5U
			)) + 8),
		},
		full_grcg_clip,
		*white
	);
}

[[nodiscard]] bool render_ellen(
	Raster &raster,
	const Sprite16Staging &staging,
	const MainPhaseContext &context,
	std::uint8_t player,
	const std::array<EllenChargeNodeFact, MainChargeShotView::ellen_node_count> &nodes
) noexcept
{
	for(const auto &node : nodes) {
		if(node.state != 1U) {
			continue;
		}
		const auto phase = static_cast<std::uint16_t>(
			(static_cast<std::uint8_t>(node.angle + 0x10U) >> 5U) * 16U
		);
		if(!put(
			raster,
			staging,
			source_rect(player, static_cast<std::uint16_t>(128U + phase), 0U, 16U, 8U),
			{
				wrap_word(static_cast<std::int32_t>(foreground_x(
					context, node.center_x, player
				)) - 8),
				wrap_word(static_cast<std::int32_t>(shift_right(node.center_y, 4U)) + 8),
			},
			playfield_clips[player]
		)) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] bool render_kotohime(
	Raster &raster,
	const Sprite16Staging &staging,
	const MainPhaseContext &context,
	std::uint8_t player,
	const KotohimeChargeFact &fact
) noexcept
{
	return fact.active == 0U || put(
		raster,
		staging,
		source_rect(player, 64U, 56U, 96U, 16U),
		{
			wrap_word(static_cast<std::int32_t>(foreground_x(
				context, fact.center_x, player
			)) - 48),
			wrap_word(static_cast<std::int32_t>(shift_right(fact.center_y, 4U)) + 8),
		},
		playfield_clips[player]
	);
}

[[nodiscard]] bool render_kana(
	Raster &raster,
	const Sprite16Staging &staging,
	const MainPhaseContext &context,
	std::uint8_t player,
	const KanaChargeStateFact &state,
	const std::array<std::array<RawPoint, 4U>, MainChargeShotView::kana_group_count> &points
) noexcept
{
	if(state.state == 0U) {
		return true;
	}
	constexpr std::array<std::size_t, 4U> order{{3U, 2U, 1U, 0U}};
	constexpr std::array<std::uint16_t, 4U> raw_tops{{8U, 24U, 40U, 56U}};
	for(const auto &group : points) {
		for(const auto point : order) {
			if(!put(
				raster,
				staging,
				source_rect(player, 0U, raw_tops[point], 32U, 16U),
				{
					wrap_word(static_cast<std::int32_t>(foreground_x(
						context, group[point].x, player
					)) - 16),
					shift_right(group[point].y, 4U),
				},
				playfield_clips[player],
				{.overlap = Sprite16Overlap::or_planes}
			)) {
				return false;
			}
		}
	}
	return true;
}

[[nodiscard]] bool render_rikako(
	Raster &raster,
	const Sprite16Staging &staging,
	const MainPhaseContext &context,
	std::uint8_t player,
	const RikakoChargeStateFact &state,
	const std::array<RikakoChargeNodeFact, MainChargeShotView::rikako_node_count> &nodes
) noexcept
{
	if(state.state == 0U) {
		return true;
	}
	for(const auto &node : nodes) {
		if(!put(
			raster,
			staging,
			source_rect(player, 0U, 8U, 32U, 16U),
			{
				wrap_word(static_cast<std::int32_t>(foreground_x(
					context, node.center_x, player
				)) - 16),
				shift_right(node.center_y, 4U),
			},
			playfield_clips[player]
		)) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] bool render_chiyuri(
	Raster &raster,
	const Sprite16Staging &staging,
	const MainPhaseContext &context,
	std::uint8_t player,
	const std::array<ChiyuriChargeNodeFact, MainChargeShotView::chiyuri_node_count> &nodes
) noexcept
{
	for(std::size_t reverse = nodes.size(); reverse > 0U; --reverse) {
		const auto &node = nodes[reverse - 1U];
		if(node.state == 0U) {
			return true;
		}
		if(node.state != 1U) {
			continue;
		}
		if(!put(
			raster,
			staging,
			source_rect(
				player,
				static_cast<std::uint16_t>((node.frame & 1U) * 32U),
				8U,
				32U,
				24U
			),
			{
				wrap_word(static_cast<std::int32_t>(foreground_x(
					context, node.center_x, player
				)) - 16),
				wrap_word(static_cast<std::int32_t>(shift_right(node.center_y, 4U)) - 8),
			},
			playfield_clips[player]
		)) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] constexpr std::int16_t polar(
	std::int16_t center,
	std::uint16_t length,
	std::int16_t ratio
) noexcept
{
	return wrap_word(
		static_cast<std::int32_t>(center) + exact::arithmetic_shift_right(
			static_cast<std::int32_t>(length) * ratio,
			8U
		)
	);
}

[[nodiscard]] bool yumemi_ring(
	Raster &raster,
	const Sprite16Staging &staging,
	const StaffFlakeTrigTables &trig,
	const MainPhaseContext &context,
	std::uint8_t player,
	const YumemiChargeFact &fact,
	std::uint16_t length,
	std::uint8_t angle,
	Sprite16AtlasRect source
) noexcept
{
	const auto center_x = wrap_word(static_cast<std::int32_t>(fact.center_x) - (16 << 4));
	const auto center_y = wrap_word(static_cast<std::int32_t>(fact.center_y) - (16 << 4));
	auto cursor = static_cast<std::uint8_t>(fact.frame * 2U);
	for(std::size_t i = 0U; i < 16U; ++i) {
		const auto y_angle = static_cast<std::uint8_t>(cursor + angle);
		const auto x = polar(center_x, length, trig.cosine_q8[cursor]);
		const auto y = polar(center_y, length, trig.sine_q8[y_angle]);
		if(!put(
			raster,
			staging,
			source,
			{
				foreground_x(context, x, player),
				wrap_word(static_cast<std::int32_t>(shift_right(y, 4U)) + 16),
			},
			playfield_clips[player]
		)) {
			return false;
		}
		cursor = static_cast<std::uint8_t>(cursor + 0x10U);
	}
	return true;
}

[[nodiscard]] bool render_yumemi(
	Raster &raster,
	const Sprite16Staging &staging,
	const StaffFlakeTrigTables &trig,
	const MainPhaseContext &context,
	std::uint8_t player,
	const YumemiChargeFact &fact
) noexcept
{
	if(fact.state == 0U) {
		return true;
	}
	const auto parity_left = static_cast<std::uint16_t>((fact.frame & 1U) * 32U);
	if(fact.state == 1U) {
		return put(
			raster,
			staging,
			source_rect(player, parity_left, 40U, 32U, 16U),
			{
				wrap_word(static_cast<std::int32_t>(foreground_x(
					context, fact.center_x, player
				)) - 16),
				shift_right(fact.center_y, 4U),
			},
			playfield_clips[player]
		);
	}
	if(fact.frame >= 64U) {
		return true;
	}
	const auto source = source_rect(player, parity_left, 56U, 32U, 16U);
	auto ring = [&](std::uint16_t length, std::uint8_t angle) noexcept {
		return yumemi_ring(
			raster, staging, trig, context, player, fact, length, angle, source
		);
	};
	if(fact.frame < 8U) {
		return ring(static_cast<std::uint16_t>(fact.frame * 128U), 0U);
	}
	if(fact.frame < 16U) {
		return ring(static_cast<std::uint16_t>((fact.frame - 7U) * 128U), 32U);
	}
	if(fact.frame < 24U) {
		return ring(static_cast<std::uint16_t>((fact.frame - 15U) * 128U), 224U);
	}
	if(fact.frame < 32U) {
		return ring(static_cast<std::uint16_t>((fact.frame - 24U) * 128U), 0U);
	}
	if(fact.frame < 40U) {
		return ring(static_cast<std::uint16_t>((fact.frame - 31U) * 128U), 32U);
	}
	if(fact.frame < 48U) {
		return ring(static_cast<std::uint16_t>((fact.frame - 39U) * 128U), 224U);
	}
	if(fact.frame < 56U) {
		const auto length = static_cast<std::uint16_t>((fact.frame - 47U) * 128U);
		if(!ring(length, 32U) || !ring(length, 224U)) {
			return false;
		}
	}
	return ring(static_cast<std::uint16_t>((fact.frame - 48U) * 128U), 0U);
}

} // namespace

void MainExtraChargePresenter::observe(const MainChargeShotView &view) noexcept
{
	if(!ready()) {
		return;
	}
	const auto player = player_index(view.player());
	const auto character = character_index(view.paletted_character());
	if(!player || !character) {
		status_ = MainExtraChargeStatus::invalid_observation;
		return;
	}
	const auto context = view.context();
	bool rendered = false;
	switch(*character) {
	case Character::reimu: {
		std::array<ReimuChargeGroupFact, MainChargeShotView::reimu_group_count> groups{};
		std::array<std::array<RawPoint, 4U>, MainChargeShotView::reimu_group_count> points{};
		for(std::size_t group = 0U; group < groups.size(); ++group) {
			const auto fact = view.reimu_group(static_cast<ReimuChargeGroupSlot>(group));
			if(!fact) {
				status_ = MainExtraChargeStatus::invalid_observation;
				return;
			}
			groups[group] = *fact;
			for(std::size_t point = 0U; point < reimu_points.size(); ++point) {
				const auto position = view.reimu_point(
					static_cast<ReimuChargeGroupSlot>(group), reimu_points[point]
				);
				if(!position) {
					status_ = MainExtraChargeStatus::invalid_observation;
					return;
				}
				points[group][point] = *position;
			}
		}
		rendered = render_reimu(*raster_, *staging_, context, *player, groups, points);
		break;
	}
	case Character::mima: {
		const auto state = view.mima_state();
		std::array<MimaChargeNodeFact, MainChargeShotView::mima_node_count> nodes{};
		if(!state) {
			status_ = MainExtraChargeStatus::invalid_observation;
			return;
		}
		for(std::size_t node = 0U; node < nodes.size(); ++node) {
			const auto fact = view.mima_node(static_cast<MimaChargeNodeSlot>(node));
			if(!fact) {
				status_ = MainExtraChargeStatus::invalid_observation;
				return;
			}
			nodes[node] = *fact;
		}
		rendered = render_mima(*raster_, *staging_, context, *player, *state, nodes);
		break;
	}
	case Character::marisa: {
		const auto state = view.marisa_state();
		std::array<MarisaChargeNodeFact, MainChargeShotView::marisa_node_count> nodes{};
		if(!state) {
			status_ = MainExtraChargeStatus::invalid_observation;
			return;
		}
		for(std::size_t node = 0U; node < nodes.size(); ++node) {
			const auto fact = view.marisa_node(static_cast<MarisaChargeNodeSlot>(node));
			if(!fact) {
				status_ = MainExtraChargeStatus::invalid_observation;
				return;
			}
			nodes[node] = *fact;
		}
		rendered = render_marisa(*raster_, context, *player, *state, nodes);
		break;
	}
	case Character::ellen: {
		std::array<EllenChargeNodeFact, MainChargeShotView::ellen_node_count> nodes{};
		for(std::size_t node = 0U; node < nodes.size(); ++node) {
			const auto fact = view.ellen_node(static_cast<EllenChargeNodeSlot>(node));
			if(!fact) {
				status_ = MainExtraChargeStatus::invalid_observation;
				return;
			}
			nodes[node] = *fact;
		}
		rendered = render_ellen(*raster_, *staging_, context, *player, nodes);
		break;
	}
	case Character::kotohime: {
		const auto fact = view.kotohime();
		if(!fact) {
			status_ = MainExtraChargeStatus::invalid_observation;
			return;
		}
		rendered = render_kotohime(*raster_, *staging_, context, *player, *fact);
		break;
	}
	case Character::kana: {
		const auto state = view.kana_state();
		std::array<std::array<RawPoint, 4U>, MainChargeShotView::kana_group_count> points{};
		if(!state) {
			status_ = MainExtraChargeStatus::invalid_observation;
			return;
		}
		for(std::size_t group = 0U; group < points.size(); ++group) {
			for(std::size_t point = 0U; point < kana_points.size(); ++point) {
				const auto fact = view.kana_point(
					static_cast<KanaChargeGroupSlot>(group), kana_points[point]
				);
				if(!fact) {
					status_ = MainExtraChargeStatus::invalid_observation;
					return;
				}
				points[group][point] = *fact;
			}
		}
		rendered = render_kana(*raster_, *staging_, context, *player, *state, points);
		break;
	}
	case Character::rikako: {
		const auto state = view.rikako_state();
		std::array<RikakoChargeNodeFact, MainChargeShotView::rikako_node_count> nodes{};
		if(!state) {
			status_ = MainExtraChargeStatus::invalid_observation;
			return;
		}
		for(std::size_t node = 0U; node < nodes.size(); ++node) {
			const auto fact = view.rikako_node(static_cast<RikakoChargeNodeSlot>(node));
			if(!fact) {
				status_ = MainExtraChargeStatus::invalid_observation;
				return;
			}
			nodes[node] = *fact;
		}
		rendered = render_rikako(*raster_, *staging_, context, *player, *state, nodes);
		break;
	}
	case Character::chiyuri: {
		std::array<ChiyuriChargeNodeFact, MainChargeShotView::chiyuri_node_count> nodes{};
		for(std::size_t node = 0U; node < nodes.size(); ++node) {
			const auto fact = view.chiyuri_node(static_cast<ChiyuriChargeNodeSlot>(node));
			if(!fact) {
				status_ = MainExtraChargeStatus::invalid_observation;
				return;
			}
			nodes[node] = *fact;
		}
		rendered = render_chiyuri(*raster_, *staging_, context, *player, nodes);
		break;
	}
	case Character::yumemi: {
		const auto fact = view.yumemi();
		if(!fact) {
			status_ = MainExtraChargeStatus::invalid_observation;
			return;
		}
		rendered = render_yumemi(
			*raster_, *staging_, trig_, context, *player, *fact
		);
		break;
	}
	}
	if(!rendered) {
		status_ = MainExtraChargeStatus::draw_failed;
	}
}

} // namespace th03::present

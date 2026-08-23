#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <new>
#include <optional>
#include <sstream>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "th03/app/new_classic_language.hpp"
#include "th03/app/resources.hpp"

namespace {

namespace fs = std::filesystem;
using th03::app::MainResourceArchive;
using th03::app::MainResourceDecoder;
using th03::app::MainResourceScene;
using th03::app::MainResourceSpec;
using th03::app::ResourceLoadErrorCode;

constexpr std::array scenes{
	MainResourceScene::classic_audio,
	MainResourceScene::op_title_menu,
	MainResourceScene::op_music_room,
	MainResourceScene::direct_vs,
	MainResourceScene::selection_handoff,
	MainResourceScene::mainl_stage_transition,
	MainResourceScene::mainl_win,
	MainResourceScene::mainl_continue_gameover,
	MainResourceScene::mainl_cutscene,
	MainResourceScene::mainl_staff,
	MainResourceScene::mainl_registration,
};

struct ResourceAudit final {
	MainResourceSpec spec{};
	std::string_view validation = "not-checked";
	std::string_view source = "not-checked";
	std::optional<ResourceLoadErrorCode> error{};
};

struct SceneAudit final {
	MainResourceScene scene = MainResourceScene::classic_audio;
	std::size_t resources = 0U;
	std::size_t original = 0U;
	std::size_t override = 0U;
	std::size_t failed = 0U;
};

struct NativeSurface final {
	std::string_view id{};
	std::string_view owner{};
	std::string_view domain{};
	std::string_view coverage{};
};

constexpr std::array native_surfaces{
	NativeSurface{
		"new-classic-menu-semantics", "op-and-mainl-ui", "T3LNG001",
		"New Classic title, Option, Replay, Practice, input, online, and system text",
	},
	NativeSurface{
		"music-room-titles", "op_music_room", "T3GTL001",
		"19 source-constrained track-title rows",
	},
	NativeSurface{
		"music-room-comments", "op_music_room", "T3GTL001",
		"380 fixed DAT rows: 19 tracks by 20 title/comment rows",
	},
	NativeSurface{
		"stage-titles-and-names", "mainl_stage_transition", "T3GTL001",
		"18 title rows and nine character-name rows",
	},
	NativeSurface{
		"character-selection-names", "selection_handoff", "T3LNG001",
		"nine New Classic center-list character-name rows",
	},
	NativeSurface{
		"character-selection-stat-labels", "selection_handoff", "T3LNG001",
		"three New Classic stat labels inside preserved SLWIN artwork",
	},
	NativeSurface{
		"registration-names", "mainl_registration", "T3GTL001",
		"No Entry plus nine character-name rows",
	},
	NativeSurface{
		"round-result-labels", "main_round_result", "T3GTL001",
		"five Clear Bonus result labels",
	},
	NativeSurface{
		"gameplay-hud-banners", "main_round_result", "T3GTL001",
		"Lose, Win, Stage, Round, and Start image-text cels",
	},
	NativeSurface{
		"continue-gameover-image-text", "mainl_continue_gameover", "T3GTL001",
		"Continue question, credit, Yes, No, and Game Over title regions",
	},
	NativeSurface{
		"win-quotes", "mainl_win", "T3GTL001",
		"297 fixed DAT rows: nine characters by 11 blocks by three rows",
	},
	NativeSurface{
		"scripted-dialogue-glyph-runs", "mainl_cutscene", "T3CTP001",
		"semantic glyph runs in all 28 authenticated scripts",
	},
};

[[nodiscard]] std::string_view name(MainResourceArchive value) noexcept
{
	switch(value) {
	case MainResourceArchive::yume1: return "YUME1.DAT";
	case MainResourceArchive::yume2: return "YUME2.DAT";
	}
	return "invalid";
}

[[nodiscard]] std::string_view name(MainResourceScene value) noexcept
{
	switch(value) {
	case MainResourceScene::classic_audio: return "classic_audio";
	case MainResourceScene::op_title_menu: return "op_title_menu";
	case MainResourceScene::op_music_room: return "op_music_room";
	case MainResourceScene::direct_vs: return "direct_vs";
	case MainResourceScene::selection_handoff: return "selection_handoff";
	case MainResourceScene::mainl_stage_transition: return "mainl_stage_transition";
	case MainResourceScene::mainl_win: return "mainl_win";
	case MainResourceScene::mainl_continue_gameover:
		return "mainl_continue_gameover";
	case MainResourceScene::mainl_cutscene: return "mainl_cutscene";
	case MainResourceScene::mainl_staff: return "mainl_staff";
	case MainResourceScene::mainl_registration: return "mainl_registration";
	}
	return "invalid";
}

[[nodiscard]] std::string_view name(MainResourceDecoder value) noexcept
{
	switch(value) {
	case MainResourceDecoder::enedat: return "enedat";
	case MainResourceDecoder::rgb48: return "rgb48";
	case MainResourceDecoder::bfnt_gaiji: return "bfnt-gaiji";
	case MainResourceDecoder::bfnt_superimpose: return "bfnt-superimpose";
	case MainResourceDecoder::cdg: return "cdg";
	case MainResourceDecoder::mrs: return "mrs";
	case MainResourceDecoder::pi: return "pi";
	case MainResourceDecoder::shift_jis_blocks: return "shift-jis-blocks";
	case MainResourceDecoder::cutscene_script: return "cutscene-script";
	case MainResourceDecoder::pmd_music: return "pmd-music";
	case MainResourceDecoder::pmd_effect_bank: return "pmd-effect-bank";
	}
	return "invalid";
}

[[nodiscard]] std::string_view name(ResourceLoadErrorCode value) noexcept
{
	switch(value) {
	case ResourceLoadErrorCode::invalid_resource_key: return "invalid-resource-key";
	case ResourceLoadErrorCode::duplicate_logical_role:
		return "duplicate-logical-role";
	case ResourceLoadErrorCode::invalid_content_root: return "invalid-content-root";
	case ResourceLoadErrorCode::path_escape: return "path-escape";
	case ResourceLoadErrorCode::case_mismatch: return "case-mismatch";
	case ResourceLoadErrorCode::missing: return "missing";
	case ResourceLoadErrorCode::not_directory: return "not-directory";
	case ResourceLoadErrorCode::not_regular_file: return "not-regular-file";
	case ResourceLoadErrorCode::size_mismatch: return "size-mismatch";
	case ResourceLoadErrorCode::checksum_mismatch: return "checksum-mismatch";
	case ResourceLoadErrorCode::read_failed: return "read-failed";
	}
	return "invalid";
}

void append_fnv(std::uint64_t &hash, std::string_view value) noexcept
{
	for(const auto character : value) {
		hash ^= static_cast<std::uint8_t>(character);
		hash *= 1099511628211ULL;
	}
	hash ^= 0U;
	hash *= 1099511628211ULL;
}

void append_fnv(std::uint64_t &hash, std::uint64_t value) noexcept
{
	for(std::size_t byte = 0U; byte < 8U; ++byte) {
		hash ^= static_cast<std::uint8_t>(value >> (byte * 8U));
		hash *= 1099511628211ULL;
	}
}

[[nodiscard]] std::uint64_t manifest_fingerprint(
	std::span<const MainResourceSpec> manifest
) noexcept
{
	auto hash = std::uint64_t{14695981039346656037ULL};
	for(const auto &spec : manifest) {
		append_fnv(hash, spec.logical_role);
		append_fnv(hash, spec.filename);
		append_fnv(hash, static_cast<std::uint64_t>(spec.key.role));
		append_fnv(hash, static_cast<std::uint64_t>(spec.key.character));
		append_fnv(hash, static_cast<std::uint64_t>(spec.key.palette));
		append_fnv(hash, static_cast<std::uint64_t>(spec.key.stage_background));
		append_fnv(hash, static_cast<std::uint64_t>(spec.key.cutscene_script));
		append_fnv(hash, static_cast<std::uint64_t>(spec.key.cutscene_picture));
		append_fnv(hash, static_cast<std::uint64_t>(spec.key.cutscene_music));
		append_fnv(hash, static_cast<std::uint64_t>(spec.key.staff_cdg));
		append_fnv(hash, static_cast<std::uint64_t>(spec.key.registration_rank));
		append_fnv(hash, static_cast<std::uint64_t>(spec.key.music_room_track));
		append_fnv(hash, static_cast<std::uint64_t>(spec.scene));
		append_fnv(hash, static_cast<std::uint64_t>(spec.archive));
		append_fnv(hash, static_cast<std::uint64_t>(spec.decoder));
		append_fnv(hash, static_cast<std::uint64_t>(spec.expected_bytes));
		append_fnv(hash, spec.expected_fnv1a64);
	}
	return hash;
}

[[nodiscard]] std::string hex64(std::uint64_t value)
{
	std::ostringstream output{};
	output << std::uppercase << std::hex << std::setw(16) << std::setfill('0')
		<< value;
	return output.str();
}

[[nodiscard]] SceneAudit &scene_audit(
	std::array<SceneAudit, scenes.size()> &audits,
	MainResourceScene scene
) noexcept
{
	for(auto &audit : audits) {
		if(audit.scene == scene) {
			return audit;
		}
	}
	return audits.front();
}

[[nodiscard]] std::expected<std::vector<ResourceAudit>, std::string_view>
audit_resources(
	std::span<const MainResourceSpec> manifest,
	const std::optional<fs::path> &data,
	const std::optional<fs::path> &overlay
)
{
	std::vector<ResourceAudit> result{};
	result.reserve(manifest.size());
	if(!data) {
		for(const auto &spec : manifest) {
			result.push_back({.spec = spec});
		}
		return result;
	}
	th03::app::MainResourceLoader loader{*data};
	std::vector<bool> overridden(manifest.size(), false);
	if(overlay) {
		auto loaded = th03::app::load_new_classic_language_overlay(*overlay);
		if(!loaded) {
			return std::unexpected("language-overlay");
		}
		for(std::size_t index = 0U; index < manifest.size(); ++index) {
			overridden[index] = loaded->contains(manifest[index].filename);
		}
		if(!loader.install_authenticated_overrides(
			std::move(*loaded).release_members()
		)) {
			return std::unexpected("language-overlay-install");
		}
	}
	for(std::size_t index = 0U; index < manifest.size(); ++index) {
		const auto &spec = manifest[index];
		auto loaded = loader.load(spec.key);
		if(!loaded) {
			result.push_back({
				.spec = spec,
				.validation = "error",
				.source = "unavailable",
				.error = loaded.error().code,
			});
			continue;
		}
		result.push_back({
			.spec = spec,
			.validation = "ok",
			.source = overridden[index] ?
				"authenticated-language-override" : "original",
		});
	}
	return result;
}

void emit_json(
	std::span<const ResourceAudit> resources,
	bool validated,
	std::uint64_t fingerprint
)
{
	std::array<SceneAudit, scenes.size()> scene_totals{};
	for(std::size_t index = 0U; index < scenes.size(); ++index) {
		scene_totals[index].scene = scenes[index];
	}
	std::size_t loaded = 0U;
	std::size_t failed = 0U;
	std::size_t overrides = 0U;
	for(const auto &resource : resources) {
		auto &scene = scene_audit(scene_totals, resource.spec.scene);
		++scene.resources;
		if(resource.validation == "ok") {
			++loaded;
			if(resource.source == "authenticated-language-override") {
				++overrides;
				++scene.override;
			} else {
				++scene.original;
			}
		} else if(resource.validation == "error") {
			++failed;
			++scene.failed;
		}
	}
	std::cout << "{\n  \"format\": \"T3CDA001\",\n"
		<< "  \"mode\": \"" << (validated ? "validated" : "manifest")
		<< "\",\n  \"manifest_valid\": true,\n"
		<< "  \"manifest_fnv1a64\": \"" << hex64(fingerprint) << "\",\n"
		<< "  \"resource_count\": " << resources.size() << ",\n"
		<< "  \"loaded_count\": " << loaded << ",\n"
		<< "  \"override_count\": " << overrides << ",\n"
		<< "  \"failure_count\": " << failed << ",\n"
		<< "  \"scenes\": [\n";
	for(std::size_t index = 0U; index < scene_totals.size(); ++index) {
		const auto &scene = scene_totals[index];
		std::cout << "    {\"scene\": \"" << name(scene.scene)
			<< "\", \"resources\": " << scene.resources
			<< ", \"original\": " << scene.original
			<< ", \"authenticated_override\": " << scene.override
			<< ", \"failed\": " << scene.failed << '}';
		std::cout << (index + 1U == scene_totals.size() ? "\n" : ",\n");
	}
	std::cout << "  ],\n  \"resources\": [\n";
	for(std::size_t index = 0U; index < resources.size(); ++index) {
		const auto &resource = resources[index];
		std::cout << "    {\"scene\": \"" << name(resource.spec.scene)
			<< "\", \"logical_role\": \"" << resource.spec.logical_role
			<< "\", \"archive\": \"" << name(resource.spec.archive)
			<< "\", \"filename\": \"" << resource.spec.filename
			<< "\", \"decoder\": \"" << name(resource.spec.decoder)
			<< "\", \"expected_bytes\": " << resource.spec.expected_bytes
			<< ", \"expected_fnv1a64\": \""
			<< hex64(resource.spec.expected_fnv1a64)
			<< "\", \"validation\": \"" << resource.validation
			<< "\", \"source\": \"" << resource.source << '"';
		if(resource.error) {
			std::cout << ", \"error\": \"" << name(*resource.error) << '"';
		}
		std::cout << '}'
			<< (index + 1U == resources.size() ? "\n" : ",\n");
	}
	std::cout << "  ],\n  \"native_semantic_surfaces\": [\n";
	for(std::size_t index = 0U; index < native_surfaces.size(); ++index) {
		const auto &surface = native_surfaces[index];
		std::cout << "    {\"id\": \"" << surface.id
			<< "\", \"owner\": \"" << surface.owner
			<< "\", \"domain\": \"" << surface.domain
			<< "\", \"coverage\": \"" << surface.coverage
			<< "\", \"source_backing_required\": true}"
			<< (index + 1U == native_surfaces.size() ? "\n" : ",\n");
	}
	std::cout << "  ]\n}\n";
}

void usage()
{
	std::cout
		<< "usage: th03-content-audit [--data <extracted-root>]\n"
		<< "       [--language-overlay <TH3EN.DAT>]\n\n"
		<< "Without --data, emits the deterministic production manifest only.\n"
		<< "With --data, validates every resource through MainResourceLoader.\n";
}

[[nodiscard]] int fail(std::string_view stage)
{
	std::cerr << "status=error\nstage=" << stage << '\n';
	return 1;
}

} // namespace

int main(int argc, char **argv)
{
	if(argc == 2 && std::string_view(argv[1]) == "--help") {
		usage();
		return 0;
	}
	std::optional<fs::path> data{};
	std::optional<fs::path> overlay{};
	if((argc & 1) == 0) {
		usage();
		return 2;
	}
	for(int index = 1; index < argc; index += 2) {
		if(std::string_view(argv[index + 1]).empty()) {
			return 2;
		}
		const auto option = std::string_view(argv[index]);
		if(option == "--data" && !data) {
			data.emplace(argv[index + 1]);
		} else if(option == "--language-overlay" && !overlay) {
			overlay.emplace(argv[index + 1]);
		} else {
			usage();
			return 2;
		}
	}
	if(overlay && !data) {
		return fail("language-overlay-requires-data");
	}
	try {
		auto manifest = th03::app::main_resource_manifest();
		if(auto valid = th03::app::validate_main_resource_manifest(manifest);
			!valid) {
			return fail("manifest");
		}
		auto audited = audit_resources(manifest, data, overlay);
		if(!audited) {
			return fail(audited.error());
		}
		emit_json(*audited, data.has_value(), manifest_fingerprint(manifest));
		const auto failed = std::ranges::any_of(*audited, [](const auto &resource) {
			return resource.validation == "error";
		});
		if(failed) {
			std::cerr << "status=error\nstage=resource-validation\n";
			return 1;
		}
		return 0;
	} catch(const std::bad_alloc &) {
		return fail("allocation");
	}
}

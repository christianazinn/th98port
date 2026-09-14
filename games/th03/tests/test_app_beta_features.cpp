#include "th03/app/classic/classic_cli.hpp"
#include "th03/app/classic/classic_runner.hpp"
#include "th03/app/new_classic/beta_features.hpp"
#include "th03/app/new_classic/persistence/new_classic_settings.hpp"
#include "th03/app/new_classic/persistence/new_classic_settings_store.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

namespace fs = std::filesystem;

using th03::app::ClassicCliErrorCode;
using th03::app::ClassicLanguage;
using th03::app::ClassicRunnerErrorCode;
using th03::app::ClassicRunnerRequest;
using th03::app::ExperienceProfile;
using th03::app::NewClassicSettings;
using th03::app::new_classic_settings_file_bytes;
using th03::dopamine_test_v1_ruleset;
using th03::stock_gameplay_rules;

std::size_t checks = 0U;
std::size_t failures = 0U;

void expect(bool condition, std::string_view message)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << message << '\n';
	}
}

class TemporaryDirectory final {
public:
	TemporaryDirectory()
	{
		const auto suffix = std::chrono::steady_clock::now()
			.time_since_epoch().count();
		path_ = fs::temp_directory_path() /
			("th03-beta-features-" + std::to_string(suffix));
		std::error_code error;
		fs::create_directories(path_, error);
		valid_ = !error;
	}

	~TemporaryDirectory()
	{
		if(valid_) {
			std::error_code ignored;
			fs::remove_all(path_, ignored);
		}
	}

	[[nodiscard]] bool valid() const noexcept { return valid_; }
	[[nodiscard]] const fs::path &path() const noexcept { return path_; }

private:
	fs::path path_{};
	bool valid_ = false;
};

[[nodiscard]] bool write_bytes(
	const fs::path &path,
	std::span<const std::byte> bytes
)
{
	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	if(!output) {
		return false;
	}
	output.write(
		reinterpret_cast<const char *>(bytes.data()),
		static_cast<std::streamsize>(bytes.size())
	);
	return static_cast<bool>(output);
}

[[nodiscard]] std::vector<std::byte> read_bytes(const fs::path &path)
{
	std::ifstream input(path, std::ios::binary | std::ios::ate);
	if(!input) {
		return {};
	}
	const auto end = input.tellg();
	if(end < 0) {
		return {};
	}
	std::vector<std::byte> bytes(static_cast<std::size_t>(end));
	input.seekg(0, std::ios::beg);
	if(!bytes.empty()) {
		input.read(
			reinterpret_cast<char *>(bytes.data()),
			static_cast<std::streamsize>(bytes.size())
		);
	}
	return input ? bytes : std::vector<std::byte>{};
}

void test_arrange_cli_feature_gate()
{
	constexpr std::array arguments{
		std::string_view{"--new-classic"},
		std::string_view{"--arrange"},
		std::string_view{"dopamine-test-v1"},
		std::string_view{"--data"}, std::string_view{"data"},
		std::string_view{"--main"}, std::string_view{"MAIN.EXE"},
		std::string_view{"--mainl"}, std::string_view{"MAINL.EXE"},
		std::string_view{"--op"}, std::string_view{"OP.EXE"},
		std::string_view{"--font"}, std::string_view{"FONT.ROM"},
	};
	const auto parsed = th03::app::parse_classic_cli(arguments);
#if TH03_BETA_FEATURES
	expect(parsed && parsed->launch.gameplay_ruleset == dopamine_test_v1_ruleset,
		"beta CLI accepts the explicit Dopamine Arrange ruleset");
#else
	expect(!parsed && parsed.error().code == ClassicCliErrorCode::invalid_arrange,
		"default CLI rejects the unavailable Dopamine Arrange ruleset");
#endif
}

void test_runner_feature_gate()
{
	ClassicRunnerRequest request{};
	request.experience = ExperienceProfile::new_classic;
	request.gameplay_ruleset = dopamine_test_v1_ruleset;
	request.content.data_root = "data";
	request.content.main_executable = "MAIN.EXE";
	request.content.mainl_executable = "MAINL.EXE";
	request.content.op_executable = "OP.EXE";
	request.content.font_rom = "FONT.ROM";
	const auto loaded = th03::app::ClassicRunner::load(std::move(request));
#if TH03_BETA_FEATURES
	expect(!loaded && loaded.error().code != ClassicRunnerErrorCode::invalid_request,
		"beta runner admits Arrange before content validation");
#else
	expect(!loaded && loaded.error().code == ClassicRunnerErrorCode::invalid_request,
		"default runner rejects direct Arrange activation");
#endif
}

void test_settings_feature_gate()
{
	TemporaryDirectory temporary;
	expect(temporary.valid(), "beta settings temporary directory creates");
	if(!temporary.valid()) {
		return;
	}
	const auto settings_path = temporary.path() / "PODD.CFG";
	const auto classic_path = temporary.path() / "YUME.CFG";
	NewClassicSettings unavailable{};
	unavailable.options.language = ClassicLanguage::spanish_test;
	unavailable.gameplay_ruleset = dopamine_test_v1_ruleset;
	const auto encoded = th03::app::encode_new_classic_settings(unavailable);
	expect(encoded.has_value(),
		"raw settings codec keeps the experimental wire values valid");
	if(!encoded) {
		return;
	}
	expect(write_bytes(settings_path, *encoded),
		"experimental settings fixture writes");
	const auto loaded = th03::app::load_new_classic_settings_store(
		settings_path, classic_path
	);
#if TH03_BETA_FEATURES
	expect(loaded && loaded->settings == unavailable,
		"beta settings preserve explicit Spanish and Arrange selections");
#else
	expect(loaded && loaded->settings.options.language ==
		ClassicLanguage::english &&
		stock_gameplay_rules(loaded->settings.gameplay_ruleset),
		"default settings migrate unavailable selections to English and Stock");
#endif
	const auto persisted = read_bytes(settings_path);
	const auto decoded = persisted.size() == new_classic_settings_file_bytes ?
		th03::app::decode_new_classic_settings(persisted) :
		std::expected<NewClassicSettings, th03::app::NewClassicSettingsError>{
			std::unexpected(th03::app::NewClassicSettingsError{})
		};
#if TH03_BETA_FEATURES
	expect(decoded && *decoded == unavailable,
		"beta settings leave the explicit feature document untouched");
#else
	expect(decoded && decoded->options.language == ClassicLanguage::english &&
		stock_gameplay_rules(decoded->gameplay_ruleset),
		"default settings rewrite unavailable selections canonically");
#endif

	const auto committed = th03::app::commit_new_classic_settings_store(
		settings_path, unavailable
	);
	expect(committed.has_value(), "feature-gated settings commit succeeds");
	const auto committed_bytes = read_bytes(settings_path);
	const auto committed_settings =
		committed_bytes.size() == new_classic_settings_file_bytes ?
			th03::app::decode_new_classic_settings(committed_bytes) :
			std::expected<NewClassicSettings, th03::app::NewClassicSettingsError>{
				std::unexpected(th03::app::NewClassicSettingsError{})
			};
#if TH03_BETA_FEATURES
	expect(committed_settings && *committed_settings == unavailable,
		"beta settings commit retains the explicit feature selections");
#else
	expect(committed_settings &&
		committed_settings->options.language == ClassicLanguage::english &&
		stock_gameplay_rules(committed_settings->gameplay_ruleset),
		"default settings commit cannot reactivate unavailable selections");
#endif
}

} // namespace

int main()
{
	static_assert(th03::app::new_classic_beta_features_enabled ==
		(TH03_BETA_FEATURES != 0));
	test_arrange_cli_feature_gate();
	test_runner_feature_gate();
	test_settings_feature_gate();
	if(failures != 0U) {
		std::cerr << "Beta feature checks: " << checks
			<< ", failures: " << failures << '\n';
		return 1;
	}
	std::cout << "Beta feature checks: " << checks << '\n';
	return 0;
}

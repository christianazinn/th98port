#include "replay_extension_provider.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

int main(int argc, char **argv)
{
	if(argc != 2) {
		std::cerr << "usage: th03-replay-extension-provider-probe <output>\n";
		return 2;
	}
	auto provider =
		th03::app::launcher_support::product_replay_extension_provider();
	if(!provider.begin && !provider.finish) {
		std::cout << "provider=absent\n";
		return 0;
	}
	if(!provider.begin || !provider.finish) {
		std::cerr << "provider=incomplete\n";
		return 1;
	}
	th03::formats::ReplayOpaqueExtension extension{};
	if(!provider.begin(provider.context, extension)) {
		std::cerr << "provider=begin-failed\n";
		return 1;
	}
	std::vector<std::byte> replay(896U);
	constexpr std::string_view magic = "T3RPLY14";
	for(std::size_t index = 0U; index < magic.size(); ++index) {
		replay[index] = static_cast<std::byte>(magic[index]);
	}
	if(!provider.finish(provider.context, replay, extension)) {
		std::cerr << "provider=finish-failed\n";
		return 1;
	}
	std::transform(
		extension.begin(), extension.end(), replay.begin() + 790U,
		[](std::uint8_t value) { return static_cast<std::byte>(value); }
	);
	std::ofstream output(argv[1], std::ios::binary | std::ios::trunc);
	output.write(
		reinterpret_cast<const char *>(replay.data()),
		static_cast<std::streamsize>(replay.size())
	);
	if(!output) {
		std::cerr << "provider=write-failed\n";
		return 1;
	}
	std::cout << "provider=complete\n";
	return 0;
}

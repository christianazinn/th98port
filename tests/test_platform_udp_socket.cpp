#include "th03/platform/udp_socket.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <string_view>
#include <thread>

namespace {

std::size_t checks = 0U;
std::size_t failures = 0U;

void expect(bool condition, std::string_view message)
{
	++checks;
	if(!condition) {
		++failures;
		std::fprintf(stderr, "FAIL: %.*s\n",
			static_cast<int>(message.size()), message.data());
	}
}

void test_endpoint_and_loopback()
{
	using namespace th03::platform;
	const auto tailscale = parse_udp_ipv4_endpoint("100.64.12.34:31415");
	expect(tailscale && tailscale->address ==
		std::array<std::uint8_t, 4U>{100U, 64U, 12U, 34U} &&
		tailscale->port == 31415U &&
		format_udp_ipv4_endpoint(*tailscale) == "100.64.12.34:31415",
		"IPv4 endpoint parser round-trips a Tailscale address");
	for(const auto invalid : {
		std::string_view{""}, std::string_view{"localhost:3"},
		std::string_view{"1.2.3:4"}, std::string_view{"1.2.3.999:4"},
		std::string_view{"1.2.3.4:0"}, std::string_view{"1.2.3.4:65536"},
	}) {
		expect(!parse_udp_ipv4_endpoint(invalid),
			"malformed or unusable endpoint fails closed");
	}
	auto receiver = UdpSocket::bind(0U);
	auto sender = UdpSocket::bind(0U);
	expect(receiver && sender && (*receiver)->local_endpoint().port != 0U &&
		(*sender)->local_endpoint().port != 0U,
		"portable UDP sockets bind nonblocking ephemeral ports");
	if(!receiver || !sender) {
		return;
	}
	constexpr std::array payload{
		std::byte{0x54U}, std::byte{0x33U}, std::byte{0x4EU}, std::byte{0x50U}
	};
	const auto target = UdpIpv4Endpoint{
		.address = {127U, 0U, 0U, 1U},
		.port = (*receiver)->local_endpoint().port,
	};
	expect((*sender)->send(target, payload).has_value(),
		"loopback datagram sends through the portable owner");
	std::array<std::byte, 64U> output{};
	std::optional<UdpReceivedDatagram> received{};
	for(std::size_t attempt = 0U; attempt < 100U && !received; ++attempt) {
		const auto result = (*receiver)->receive(output);
		expect(static_cast<bool>(result),
			"nonblocking loopback receive remains operational");
		if(!result) {
			break;
		}
		received = *result;
		if(!received) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}
	expect(received && received->size == payload.size() &&
		received->sender.address ==
			std::array<std::uint8_t, 4U>{127U, 0U, 0U, 1U} &&
		received->sender.port == (*sender)->local_endpoint().port &&
		std::equal(payload.begin(), payload.end(), output.begin()),
		"loopback receive retains exact bytes and sender endpoint");
}

} // namespace

int main()
{
	test_endpoint_and_loopback();
	if(failures != 0U) {
		std::fprintf(stderr, "%zu of %zu checks failed\n", failures, checks);
		return 1;
	}
	std::printf("all %zu portable UDP socket checks passed\n", checks);
	return 0;
}

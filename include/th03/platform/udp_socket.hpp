#ifndef TH03_PLATFORM_UDP_SOCKET_HPP
#define TH03_PLATFORM_UDP_SOCKET_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace th03::platform {

struct UdpIpv4Endpoint final {
	std::array<std::uint8_t, 4U> address{};
	std::uint16_t port = 0U;

	[[nodiscard]] friend constexpr bool operator==(
		const UdpIpv4Endpoint &,
		const UdpIpv4Endpoint &
	) = default;
};

enum class UdpSocketErrorCode : std::uint8_t {
	invalid_endpoint,
	platform_startup_failed,
	socket_create_failed,
	nonblocking_failed,
	bind_failed,
	local_endpoint_failed,
	send_failed,
	receive_failed,
	output_too_small,
};

struct UdpSocketError final {
	UdpSocketErrorCode code = UdpSocketErrorCode::invalid_endpoint;
	int native_code = 0;
};

struct UdpReceivedDatagram final {
	UdpIpv4Endpoint sender{};
	std::size_t size = 0U;
};

[[nodiscard]] std::expected<UdpIpv4Endpoint, UdpSocketError>
parse_udp_ipv4_endpoint(std::string_view text) noexcept;
[[nodiscard]] std::string format_udp_ipv4_endpoint(
	UdpIpv4Endpoint endpoint
);

class UdpSocket final {
public:
	[[nodiscard]] static std::expected<
		std::unique_ptr<UdpSocket>,
		UdpSocketError
	> bind(std::uint16_t port);

	~UdpSocket();
	UdpSocket(const UdpSocket &) = delete;
	UdpSocket &operator=(const UdpSocket &) = delete;
	UdpSocket(UdpSocket &&) = delete;
	UdpSocket &operator=(UdpSocket &&) = delete;

	[[nodiscard]] std::expected<void, UdpSocketError> send(
		UdpIpv4Endpoint recipient,
		std::span<const std::byte> payload
	) noexcept;
	[[nodiscard]] std::expected<
		std::optional<UdpReceivedDatagram>,
		UdpSocketError
	> receive(std::span<std::byte> output) noexcept;
	[[nodiscard]] UdpIpv4Endpoint local_endpoint() const noexcept;

private:
	struct Impl;
	explicit UdpSocket(std::unique_ptr<Impl> impl) noexcept;
	std::unique_ptr<Impl> impl_;
};

} // namespace th03::platform

#endif

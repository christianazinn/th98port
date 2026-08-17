#include "th03/platform/udp_socket.hpp"

#include <charconv>
#include <bit>
#include <limits>
#include <utility>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace th03::platform {
namespace {

#if defined(_WIN32)
using NativeSocket = SOCKET;
constexpr NativeSocket invalid_socket = INVALID_SOCKET;
[[nodiscard]] int last_error() noexcept { return WSAGetLastError(); }
[[nodiscard]] bool would_block(int error) noexcept
{
	return error == WSAEWOULDBLOCK;
}
void close_socket(NativeSocket socket) noexcept { closesocket(socket); }
#else
using NativeSocket = int;
constexpr NativeSocket invalid_socket = -1;
[[nodiscard]] int last_error() noexcept { return errno; }
[[nodiscard]] bool would_block(int error) noexcept
{
	return error == EAGAIN || error == EWOULDBLOCK;
}
void close_socket(NativeSocket socket) noexcept { close(socket); }
#endif

[[nodiscard]] UdpSocketError error(
	UdpSocketErrorCode code,
	int native = 0
) noexcept
{
	return UdpSocketError{.code = code, .native_code = native};
}

[[nodiscard]] sockaddr_in native_endpoint(UdpIpv4Endpoint endpoint) noexcept
{
	sockaddr_in result{};
	result.sin_family = AF_INET;
	result.sin_port = htons(endpoint.port);
	const std::uint32_t address =
		(static_cast<std::uint32_t>(endpoint.address[0U]) << 24U) |
		(static_cast<std::uint32_t>(endpoint.address[1U]) << 16U) |
		(static_cast<std::uint32_t>(endpoint.address[2U]) << 8U) |
		static_cast<std::uint32_t>(endpoint.address[3U]);
	result.sin_addr.s_addr = htonl(address);
	return result;
}

[[nodiscard]] UdpIpv4Endpoint endpoint_from_native(
	const sockaddr_in &endpoint
) noexcept
{
	const auto address = ntohl(endpoint.sin_addr.s_addr);
	return UdpIpv4Endpoint{
		.address = {
			static_cast<std::uint8_t>(address >> 24U),
			static_cast<std::uint8_t>(address >> 16U),
			static_cast<std::uint8_t>(address >> 8U),
			static_cast<std::uint8_t>(address),
		},
		.port = ntohs(endpoint.sin_port),
	};
}

} // namespace

struct UdpSocket::Impl final {
	NativeSocket socket = invalid_socket;
	UdpIpv4Endpoint local{};
#if defined(_WIN32)
	bool winsock_started = false;
#endif

	~Impl()
	{
		if(socket != invalid_socket) {
			close_socket(socket);
		}
#if defined(_WIN32)
		if(winsock_started) {
			WSACleanup();
		}
#endif
	}
};

std::expected<UdpIpv4Endpoint, UdpSocketError>
parse_udp_ipv4_endpoint(std::string_view text) noexcept
{
	const auto colon = text.rfind(':');
	if(colon == std::string_view::npos || colon == 0U ||
		colon + 1U == text.size()) {
		return std::unexpected(error(UdpSocketErrorCode::invalid_endpoint));
	}
	UdpIpv4Endpoint result{};
	auto address = text.substr(0U, colon);
	for(std::size_t index = 0U; index < result.address.size(); ++index) {
		const auto dot = address.find('.');
		const auto part = index + 1U == result.address.size() ?
			address : address.substr(0U, dot);
		if(part.empty() || (index + 1U != result.address.size() &&
			dot == std::string_view::npos)) {
			return std::unexpected(error(UdpSocketErrorCode::invalid_endpoint));
		}
		unsigned value = 0U;
		const auto parsed = std::from_chars(
			part.data(), part.data() + part.size(), value
		);
		if(parsed.ec != std::errc{} || parsed.ptr != part.data() + part.size() ||
			value > 255U) {
			return std::unexpected(error(UdpSocketErrorCode::invalid_endpoint));
		}
		result.address[index] = static_cast<std::uint8_t>(value);
		address = index + 1U == result.address.size() ?
			std::string_view{} : address.substr(dot + 1U);
	}
	if(!address.empty()) {
		return std::unexpected(error(UdpSocketErrorCode::invalid_endpoint));
	}
	unsigned port = 0U;
	const auto port_text = text.substr(colon + 1U);
	const auto parsed = std::from_chars(
		port_text.data(), port_text.data() + port_text.size(), port
	);
	if(parsed.ec != std::errc{} ||
		parsed.ptr != port_text.data() + port_text.size() ||
		port == 0U || port > std::numeric_limits<std::uint16_t>::max()) {
		return std::unexpected(error(UdpSocketErrorCode::invalid_endpoint));
	}
	result.port = static_cast<std::uint16_t>(port);
	return result;
}

std::string format_udp_ipv4_endpoint(UdpIpv4Endpoint endpoint)
{
	return std::to_string(endpoint.address[0U]) + "." +
		std::to_string(endpoint.address[1U]) + "." +
		std::to_string(endpoint.address[2U]) + "." +
		std::to_string(endpoint.address[3U]) + ":" +
		std::to_string(endpoint.port);
}

std::expected<std::unique_ptr<UdpSocket>, UdpSocketError>
UdpSocket::bind(std::uint16_t port)
{
	auto impl = std::make_unique<Impl>();
#if defined(_WIN32)
	WSADATA data{};
	const auto startup = WSAStartup(MAKEWORD(2, 2), &data);
	if(startup != 0) {
		return std::unexpected(error(
			UdpSocketErrorCode::platform_startup_failed, startup
		));
	}
	impl->winsock_started = true;
#endif
	impl->socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(impl->socket == invalid_socket) {
		return std::unexpected(error(
			UdpSocketErrorCode::socket_create_failed, last_error()
		));
	}
#if defined(_WIN32)
	u_long nonblocking = 1U;
	const auto nonblocking_command = std::bit_cast<long>(
		static_cast<unsigned long>(FIONBIO)
	);
	if(ioctlsocket(impl->socket, nonblocking_command, &nonblocking) != 0) {
		return std::unexpected(error(
			UdpSocketErrorCode::nonblocking_failed, last_error()
		));
	}
#else
	const auto flags = fcntl(impl->socket, F_GETFL, 0);
	if(flags < 0 || fcntl(impl->socket, F_SETFL, flags | O_NONBLOCK) < 0) {
		return std::unexpected(error(
			UdpSocketErrorCode::nonblocking_failed, last_error()
		));
	}
#endif
	const auto requested = native_endpoint({
		.address = {0U, 0U, 0U, 0U},
		.port = port,
	});
#if defined(_WIN32)
	const auto requested_size = static_cast<int>(sizeof(requested));
#else
	const auto requested_size = static_cast<socklen_t>(sizeof(requested));
#endif
	if(::bind(impl->socket, reinterpret_cast<const sockaddr *>(&requested),
		requested_size) != 0) {
		return std::unexpected(error(
			UdpSocketErrorCode::bind_failed, last_error()
		));
	}
	sockaddr_in local{};
#if defined(_WIN32)
	int local_size = static_cast<int>(sizeof(local));
#else
	socklen_t local_size = static_cast<socklen_t>(sizeof(local));
#endif
	if(getsockname(impl->socket, reinterpret_cast<sockaddr *>(&local),
		&local_size) != 0) {
		return std::unexpected(error(
			UdpSocketErrorCode::local_endpoint_failed, last_error()
		));
	}
	impl->local = endpoint_from_native(local);
	return std::unique_ptr<UdpSocket>(new UdpSocket(std::move(impl)));
}

UdpSocket::UdpSocket(std::unique_ptr<Impl> impl) noexcept :
	impl_(std::move(impl))
{}

UdpSocket::~UdpSocket() = default;

std::expected<void, UdpSocketError> UdpSocket::send(
	UdpIpv4Endpoint recipient,
	std::span<const std::byte> payload
) noexcept
{
	if(recipient.port == 0U || payload.empty() ||
		payload.size() > static_cast<std::size_t>(
			std::numeric_limits<int>::max()
		)) {
		return std::unexpected(error(UdpSocketErrorCode::invalid_endpoint));
	}
	const auto native = native_endpoint(recipient);
#if defined(_WIN32)
	const auto payload_size = static_cast<int>(payload.size());
	const auto endpoint_size = static_cast<int>(sizeof(native));
#else
	const auto payload_size = payload.size();
	const auto endpoint_size = static_cast<socklen_t>(sizeof(native));
#endif
	const auto sent = sendto(
		impl_->socket,
		reinterpret_cast<const char *>(payload.data()),
		payload_size,
		0,
		reinterpret_cast<const sockaddr *>(&native),
		endpoint_size
	);
	if(sent < 0 || static_cast<std::size_t>(sent) != payload.size()) {
		return std::unexpected(error(
			UdpSocketErrorCode::send_failed, last_error()
		));
	}
	return {};
}

std::expected<std::optional<UdpReceivedDatagram>, UdpSocketError>
UdpSocket::receive(std::span<std::byte> output) noexcept
{
	if(output.empty() || output.size() > static_cast<std::size_t>(
		std::numeric_limits<int>::max())) {
		return std::unexpected(error(UdpSocketErrorCode::output_too_small));
	}
	sockaddr_in sender{};
#if defined(_WIN32)
	int sender_size = static_cast<int>(sizeof(sender));
#else
	socklen_t sender_size = static_cast<socklen_t>(sizeof(sender));
#endif
#if defined(_WIN32)
	const auto output_size = static_cast<int>(output.size());
#else
	const auto output_size = output.size();
#endif
	const auto received = recvfrom(
		impl_->socket,
		reinterpret_cast<char *>(output.data()),
		output_size,
		0,
		reinterpret_cast<sockaddr *>(&sender),
		&sender_size
	);
	if(received < 0) {
		const auto native = last_error();
		if(would_block(native)) {
			return std::optional<UdpReceivedDatagram>{};
		}
		return std::unexpected(error(
			UdpSocketErrorCode::receive_failed, native
		));
	}
	return std::optional<UdpReceivedDatagram>{UdpReceivedDatagram{
		.sender = endpoint_from_native(sender),
		.size = static_cast<std::size_t>(received),
	}};
}

UdpIpv4Endpoint UdpSocket::local_endpoint() const noexcept
{
	return impl_->local;
}

} // namespace th03::platform

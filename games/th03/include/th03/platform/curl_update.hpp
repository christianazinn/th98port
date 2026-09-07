#ifndef TH03_PLATFORM_CURL_UPDATE_HPP
#define TH03_PLATFORM_CURL_UPDATE_HPP

#include "th03/app/update/update_acquisition.hpp"

namespace th03::platform {

// Returns the libcurl absolute-HTTPS adapter used by Linux and macOS packages.
// It is inert until a host worker calls the returned synchronous transport.
[[nodiscard]] app::UpdateTransport curl_update_transport() noexcept;

} // namespace th03::platform

#endif

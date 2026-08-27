#ifndef TH03_PLATFORM_WINDOWS_UPDATE_HPP
#define TH03_PLATFORM_WINDOWS_UPDATE_HPP

#include "th03/app/update/update_acquisition.hpp"

namespace th03::platform {

// Returns the WinHTTP absolute-HTTPS adapter. It is inert until a host worker
// calls the returned synchronous transport.
[[nodiscard]] app::UpdateTransport windows_update_transport() noexcept;

} // namespace th03::platform

#endif

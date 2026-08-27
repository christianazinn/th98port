#pragma once

#if defined(TH03_ENABLE_UPDATE_HOST)

#include <expected>
#include <optional>

#include "th03/app/update/update_bootstrap.hpp"
#include "th03/app/update/update_task.hpp"

namespace th03::app::launcher_support {

[[nodiscard]] std::expected<void, int> initialize_update_host();

[[nodiscard]] std::expected<void, int> acknowledge_update_health();

[[nodiscard]] const std::optional<UpdateRuntimeContext> &
current_update_runtime_context() noexcept;

} // namespace th03::app::launcher_support

#endif

#pragma once

#include "th03/app/new_classic/replay/new_classic_replay.hpp"

namespace th03::app::launcher_support {

// The normal public build returns an empty provider. Release builders may
// substitute an application-owned implementation at configure time; libth03
// continues to treat the resulting V14 bytes as opaque.
[[nodiscard]] NewClassicReplayExtensionProvider
product_replay_extension_provider() noexcept;

} // namespace th03::app::launcher_support

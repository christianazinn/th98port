#ifndef TH03_APP_NEW_CLASSIC_NETPLAY_POST_MATCH_INTERNAL_HPP
#define TH03_APP_NEW_CLASSIC_NETPLAY_POST_MATCH_INTERNAL_HPP

#include <utility>

#include "th03/app/new_classic/netplay/new_classic_netplay_post_match.hpp"

namespace th03::app {

struct NewClassicNetplayPostMatchTestAccess final {
	[[nodiscard]] static std::expected<
		std::unique_ptr<NewClassicNetplayPostMatch>,
		NewClassicNetplayPostMatchError
	> create(
		NewClassicNetplayPostMatchRequest request,
		std::span<const LoadedMainResource> resources,
		const present::AnkGlyphProvider &font
	)
	{
		return NewClassicNetplayPostMatch::create_prepared(
			std::move(request), resources, font, false
		);
	}
};

} // namespace th03::app

#endif

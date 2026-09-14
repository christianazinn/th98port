#ifndef TH03_APP_NEW_CLASSIC_BETA_FEATURES_HPP
#define TH03_APP_NEW_CLASSIC_BETA_FEATURES_HPP

#ifndef TH03_BETA_FEATURES
#define TH03_BETA_FEATURES 0
#endif

#if TH03_BETA_FEATURES != 0 && TH03_BETA_FEATURES != 1
#error "TH03_BETA_FEATURES must be 0 or 1"
#endif

namespace th03::app {

inline constexpr bool new_classic_beta_features_enabled =
	TH03_BETA_FEATURES != 0;

} // namespace th03::app

#endif

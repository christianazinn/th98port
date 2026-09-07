#ifndef TH03_APP_MAIN_STAGE_HANDOFF_INTERNAL_HPP
#define TH03_APP_MAIN_STAGE_HANDOFF_INTERNAL_HPP

#include <utility>

#include "th03/present/raster.hpp"

namespace th03::app {

class ClassicMainlStageTransition;
class MainProcess;
struct ClassicMainlStageTestAccess;
struct MainProcessStoryAccess;
struct MainProcessTestAccess;

// Source-private capability proving that MAINL completed the visible splash,
// both fades, and the hidden six-PI staging transaction. It deliberately owns
// graphics-page state only: MAIN creates a fresh Tram and installs GAMEFT.
class MainStageHandoff final {
public:
	MainStageHandoff(MainStageHandoff &&) noexcept = default;
	MainStageHandoff &operator=(MainStageHandoff &&) noexcept = default;
	MainStageHandoff(const MainStageHandoff &) = delete;
	MainStageHandoff &operator=(const MainStageHandoff &) = delete;

private:
	friend class ClassicMainlStageTransition;
	friend class MainProcess;
	friend struct ClassicMainlStageTestAccess;
	friend struct MainProcessStoryAccess;
	friend struct MainProcessTestAccess;

	explicit MainStageHandoff(present::Raster raster) noexcept :
		raster_(std::move(raster))
	{}

	present::Raster raster_{};
};

} // namespace th03::app

#endif

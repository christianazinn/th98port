#pragma once

namespace th05::core {

struct State;
struct Presentation;
struct CheetoTemplate;

// th05/main/bullet/cheeto_u.cpp. The source overlays its template and seven
// heads onto the active custom-entity storage; callers supply the transient
// intent instead of maintaining a second State-resident template. Allocation
// is source-void, and update reports only a reached source arithmetic fault.
void stage4_cheetos_add(State &, const CheetoTemplate &) noexcept;
[[nodiscard]] bool stage4_cheetos_update(State &) noexcept;

// Shinki reuses cheeto_u.cpp against B6BallState's template and first seven
// records. Its trails are the same source BSS as Stage 4's and deliberately
// remain State-resident outside either custom pool.
void stage6_cheetos_add(State &, const CheetoTemplate &) noexcept;
[[nodiscard]] bool stage6_cheetos_update(State &) noexcept;

// th05/main/bullet/b4balls_add.cpp and b4balls_update.cpp.
// Add/reset are source-void; update reports a refused concrete
// shot or revenge-bullet operation, not a ball's HP result.
void stage4_b4balls_reset(State &) noexcept;
void stage4_b4balls_add(State &) noexcept;
[[nodiscard]] bool stage4_b4balls_update(State &, Presentation &) noexcept;

// th05/main/bullet/b4balls_render.cpp's reached gameplay write. The native
// presentation has no tiny-sprite primitive, but this tail must retain the
// selective damaged-frame clear for live non-flash balls.
void stage4_b4balls_render_tail(State &) noexcept;

} // namespace th05::core

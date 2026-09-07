# New Classic private-beta test checklist

Status: manual acceptance checklist for the current standalone and hosted
packages. Automated tests cover deterministic state, formats, retained-menu
state machines, transport bounds, and persistence. The items below are the
human-visible and real-network cases that still benefit from direct testing.

When reporting a failure, include the platform, build revision or package
manifest, exact menu path, whether the game was focused, and the newest PoDD
Port log. For netplay, preserve both peers' logs.

## A. Short release gate

Run these first on every candidate:

1. Launch with a previously saved non-1x window size. The opening animations
   and title must use one stable window size.
2. On Lunatic, open `HiScore`. Confirm the retained hub appears and can enter
   Local Scores, Personal Bests, and Online Scores; repeat once on Normal.
3. Open `Replay`. Confirm Local Replays, Online Replays, Downloaded Replays,
   Upload Replay, and My Replays appear only when their real capability/data
   is available. Cancel from every child must return exactly one level.
4. Play Stage 1 Story for a few seconds, pause, Resume, pause again, Restart,
   then quit without saving. Relaunch and confirm no interrupted-replay Save
   prompt appears.
5. Play one saved V14 from Stage 1 and one later checkpoint. The Stage 1
   character interstitial and every later stage interstitial must appear with
   both fade halves; the selected round must begin at its start without a
   long black scheduler catch-up.
6. Complete one online netplay match, save on both peers, and replay both files
   while holding Shot through the ending. Neither player may be ejected from
   Save Replay when the other finishes first.

## B. Startup, title, and settings

- Let both startup logo slots complete, then skip each independently on later
  launches. Confirm the reserved project-team slot remains intentionally
  available.
- Verify Windowed, Borderless Fullscreen, 1x/2x/3x/Auto scale, 8:5/4:3 aspect,
  and Nearest/Linear filtering persist after restart.
- Minimize and restore during title, gameplay, and replay playback. The source
  simulation must not stall or race ahead, and the newest complete frame must
  appear once on restore.
- Traverse every Option category. Long labels and their one-pixel reciprocal
  shadow must stay inside the restored rows during open, navigation, cancel,
  and contraction animations.
- Test Music and SFX independently at 0%, 5%, 50%, 85%, and 100%. The values
  are linear audible-amplitude controls; 85% must sound close to full volume,
  not close to silence. Test Mute Away.
- With Story Autofire Off, Left Shift must do nothing. With Autofire On, Left
  Shift must provide Charge and Shot. Verify keyboard, controller assignment,
  Input Test, KeyConfig, and JoyConfig survive restart.
- Change language and Reduced Flash, leave Option, re-enter it, and start a
  game. Confirm translated rows redraw immediately and flash behavior changes
  without corrupting the title backing page.
- Enter sealed Classic through `Option -> System`; Classic must remain stock
  and returning must create a clean New Classic title.

## C. Story, Practice, and presentation regressions

- Start Story at the earliest possible inputs while holding Shot through
  startup/title/character select; the deterministic first-character seed case
  should reproduce the accepted seed-34 route.
- Drive the combo counter above 99, let it reset, and confirm the hundreds
  digit is erased in both interactive play and exported replay video.
- Exercise Pause Resume, Save Replay and Exit, Restart, Quit/Forfeit-relevant
  rows, and a fresh Pause after Resume. The opening frame, slowdown timing, and
  deferred gameplay frame must each be counted once.
- Enter Practice, navigate both pages, start, retry, and cancel back to the
  retained selector. Confirm animated curves, palette, and three-VSync cadence.
- Reach Stage 8 dialogue/cutscene in live play and replay playback. Confirm no
  stale title/flavor text leaks into dialogue, no misplaced graphic appears,
  and no crash occurs at the light-switch sequence.
- Complete a 1CC and a Game Over route. Check registration, Continue, staff/
  ending boundaries, playtime, clear bonus, local score persistence, and
  return-to-title music.

## D. Replay and video paths

- `TH3R01.RPY`: launch Stage 9 rounds 1 through 4. Each later round must start
  at round frame zero with no long black/unresponsive period or post-load
  catch-up.
- `TH3R06.RPY`: browse, inspect all detail pages, play from Stage 1 and at
  least one later checkpoint, and finish the replay. This is the known genuine
  V14 WR fixture.
- Confirm V12 files remain unselectable by the V14-only Replay Patch browser.
- During replay gameplay hold Shot for 4x progression; keep holding it through
  interstitials/cutscenes and confirm recorded wait/cancel input is consumed at
  source cadence without leaving the playback stranded.
- Test replay Pause, abort, Game Over, full clear, staff, and title return.
  Gameplay BGM must stop/restart at the correct process boundary.
- Save Story, local VS, and online netplay replays. Reopen details and verify
  mode label, local character, local score, slowdown, Autofire, splits, and
  date. Netplay must say `Vs Mode Netplay`.
- Render a full replay with the GUI at 1920x1200 MKV, 3x nearest scaling,
  NVENC, and 85% SFX. Confirm progress/ETA, cancellation, transactional output,
  opening/terminal boundaries, audio sync, and the cleared combo hundreds
  digit. Repeat a short software-encoded render.

## E. Online boards and replay libraries

- Open HiScore on Easy, Normal, Hard, and Lunatic. Each rank must open the hub,
  and Online Scores must show that rank rather than Normal's cache.
- Check every unlocked character and the aggregate board. Public ranking is
  best per profile; equal scores keep the older entry first. Reviewed legacy
  records appear on the same board.
- Open Personal Bests and compare score, stage, playtime, and 1CC count to the
  local score file.
- Download an online replay, restart, and Watch it from Online Replays and
  Downloaded Replays. Corrupt or replace the downloaded file and confirm it is
  rejected rather than trusted from cache.
- Upload an eligible finalized Story replay directly from the numbered slot.
  Confirm >5% slowdown is rejected, exactly 5% is accepted, and Autofire plus
  slowdown are visible in details.
- Open My Replays with no submissions, one submission, and several
  submissions. It must remain separate from the public best-per-profile list,
  adopt a newly completed refresh without restarting the product, and reuse
  Download/Watch.
- Save a completed two-win Stock netplay replay on each peer, upload each local
  file, and refresh My Replays. Each account must see its own character and
  score with the `Vs Mode Netplay` label, Download/Watch must work, and neither
  row may appear on a public Story or Overall board. A peer must not be able to
  upload the other recorder's file, and duplicate bytes must be rejected.
- Test offline startup and a transient service failure. Local scores/replays
  must remain usable, and nested failure screens must return one level.

## F. Account profile and multiple devices

- Delete/reset the beta identity, enter account-backed Online Match, and
  confirm first use requires a nametag before matchmaking. Cancel must return
  to the exact setup screen without opening a socket.
- Set a short one-line name, rename it, and verify the account UUID remains
  unchanged. Then use a two-line name such as `Resident of Fantasy Who Runs
  Through Time` and confirm whole-word wrapping above the unchanged
  `- Character -` line on both peers' VS intros.
- Test the geometric limit `28 characters + space + 28 characters`, a
  29-character word, leading/trailing/repeated spaces, and a name requiring a
  third line. Only the first case should be accepted.
- Generate `Link Another Device`, redeem the ten-character code on a second
  Windows/Linux machine within ten minutes, and confirm both devices see the
  same UUID and latest mutable nametag while retaining distinct local bearer
  credentials. Reusing the code must fail.
- Rename on either linked device, reconnect on the other, and confirm future
  matches use the new server profile. A match already paired must keep its
  frozen old snapshot.
- Pair two accounts, including equal visible nametags. Both peers must derive
  the same identity digest and show the correct P1/P2 names; the UUID, not the
  visible string, distinguishes the accounts.

## G. Netplay lifecycle

Run at least Windows/Windows and Windows/Linux over the room service. Preserve
both logs for failures.

- Quick Match, Create Room, and Join Room: test match, cancel, invalid code,
  unavailable service, direct ICE, Relay Only, and Direct UDP/Tailscale.
- Confirm title music continues throughout Online Match setup. After pairing,
  it must stop/fade promptly, WIN.M must play to completion, then the ordinary
  character-VS screen must fade in/out before frame zero.
- Select the same character/palette. P1 keeps the choice and P2 moves to the
  opposite palette. Verify fresh nonzero seeds across several matches.
- Test both peers' movement, Shot, Bomb, Autofire, and conditional Shift
  Charge/Shot. Only the ordinary Story control set is active locally.
- Pause from each peer and simultaneously. The first Resume shows `Waiting
  for Opponent`; both then see synchronized cyan 3/2/1 before gameplay resumes.
  Repeat several times late in a round.
- Navigate to Forfeit, confirm it, use Q, cross two forfeits, close the window,
  and simulate peer loss. Only a normal winner route may enter MAINL and Save
  Replay; all incomplete routes must write nothing.
- Complete a two-round match with prediction/rollback activity. Verify result
  scene, WIN music, synchronized terminal proof, post-match report, both Save
  Replay owners, and both local replay projections.
- Replay both saved sides. Each list/summary must show that machine's local
  character and score while portable gameplay remains canonical P1/P2.

## H. Platform and update package checks

- Build and launch the manual GitHub Actions artifacts on Windows, Linux,
  macOS Intel, and macOS Apple Silicon when available. Confirm bundled license
  and build manifests.
- Exercise update opt-out, explicit check, Up to date, Restart to apply,
  interrupted trial rollback, and successful health activation only with a
  signed staging package. Default/unconfigured builds must remain offline.
- On each platform verify the AppData/config/log/replay locations use PoDD/TH03
  naming and contain no obsolete `Classic` product branding.

Pixel-oracle expansion is intentionally deferred until these functional
surfaces stabilize. It remains a separate source-publication gate rather than
a substitute for the real-network and human-visible checks above.

# New Classic netplay nametags and replay identity

Status: proposed product contract, 2026-08-20. Implementation is deliberately
blocked on the small set of product decisions at the end of this document.

## Current facts

The existing online identity is deliberately opaque. The service `users` table
stores a UUID, a salted token hash, and creation time. It has no public profile
or display-name field. Replay and leaderboard rows currently display the
eight-character name saved inside each V13 replay; that is replay metadata,
not an account name.

The room service authenticates each peer's UUID, but passes only seat and ICE
signaling facts to the clients. The schema-3 `T3NLC001` match contract is 32
bytes and rejects its final four reserved bytes unless they are zero. It has no
player identity or presentation text.

The netplay VS splash reuses the ordinary MAINL stage owner. Its English layout
draws each character's two-line title and name at x=80/336 and y=276, 292, and
308 using bold PC-98 8x16 graphics text. The splash finishes before frame-zero
network proof, so an added nametag is presentation metadata: it must agree on
both peers, but it does not belong to simulation, rollback, input, slowdown, or
audio identity.

V13 has no spare header bytes that can be safely reused. Its fixed 622-byte
header, summary, checkpoint reservations, flags, and zero-reserved fields are
already validated by existing readers. Installation-local `.PODD` sidecars
currently remember only that a replay was netplay and which player was local;
they are digest-bound but do not travel with a copied or downloaded replay.

V13 does, however, explicitly permit data after the declared logical input
stream. Existing readers ignore an unrecognized tail, and Replay Patch's
optional `T3RACC1` accelerator already lives in that domain. Therefore account
identity does not require V14.

## Recommended product behavior

Use one public **account nametag**, not an independently editable per-match
alias. This keeps matchmaking, future match history, and moderation coherent,
and avoids an impersonation surface that adds no useful game behavior.

The first version should use 1..24 printable ASCII characters. Trim leading and
trailing spaces, reject control bytes and repeated/ambiguous whitespace, and
preserve case for display. Nametags need not be globally unique: the immutable
account UUID is the identity, while two players may legitimately choose the
same visible name. A small reserved-name/moderation list is sufficient for the
first beta; a generalized Unicode moderation system is not.

Twenty-four fixed-width PC-98 cells occupy 192 pixels and fit independently in
both 224-pixel player columns. This is materially longer than V13's eight-byte
replay name without requiring a new font. Full Unicode should wait until the
localized font pipeline has a distributable glyph/fallback owner; silently
replacing unsupported characters with `?` would make two distinct accounts
look identical.

On first use of account-backed Online Match, require the player to choose a
nametag after automatic credential enrollment and before entering matchmaking.
Expose later edits under `Option -> Online & Privacy`. The server is
authoritative for account-backed rooms; a cached local copy only avoids a blank
UI while a refresh is in flight.

The Worker should snapshot both authenticated profiles when it pairs a room
and send the pair to both clients over the authenticated room WebSocket. The
Durable Object must not accept a client-supplied account UUID or account
nametag as authority. Direct UDP has no account authority, so it may use each
side's locally configured nametag and must mark both identities as local/
unverified.

Render the nametag as a centered fourth 8x16 row beneath the source character
title/name block, using one bounded accent color shared by both sides. Keep the
character portrait, title, and localized character name intact. Do not replace
source character text with account text. The exact y coordinate and color
should be chosen from authenticated comparison images before implementation;
the candidate row is y=328..343, which remains above `Next Dream` in the
current splash composition.

## Network contract

Add a distinct fixed, bounded identity snapshot rather than inflating gameplay
input packets. A proposed `T3NID001` value contains:

- identity kind (`account` or `local`);
- both 16-byte UUID values, zero for local/direct identities;
- both length-prefixed 24-byte ASCII nametag slots;
- the exact room/match identity needed to prevent reuse across a different
  pairing; and
- a digest over the complete value.

The account-backed room service supplies this value from authenticated server
state. Both peers include its digest in the pre-frame match agreement. The
value never enters per-frame traffic, gameplay state, or rollback snapshots.
Changing it requires a new pre-game contract version but not a gameplay
ruleset version.

## Replay compatibility

Add one fixed, versioned `T3NID001` block immediately after V13's declared
logical input stream. If `T3RACC1` is also present, its packed records and final
footer remain after the identity block so old accelerator readers continue to
find their footer at end-of-file.

The block snapshots:

- P1 and P2 account UUIDs, or zero UUIDs for direct/local play;
- P1 and P2 match-time nametags;
- account/local flags;
- the final netplay transcript digest; and
- a digest binding the extension to the canonical logical V13 bytes.

This extension is descriptive evidence, not deterministic replay authority.
The ordinary V13 parser and verifier continue to derive gameplay and scores
solely from the frozen V13 stream. A malformed identity block invalidates only
the enhanced identity view; it must not reinterpret gameplay. Online services
may impose a stricter policy when accepting a future VS submission.

The V13 writer must learn to preserve/emit recognized typed tails before this
ships. Today it writes only the logical V13 document, and its neutral document
retains only parsed accelerator presentation facts. Exact downloads already
use the byte-preserving store path; edited/local saves need the new typed
extension owner.

The installation-local `.PODD` view remains useful for which peer was local.
That is installation-specific presentation and must not be embedded as a
portable claim. The portable extension always stores canonical P1/P2 order.

## Privacy and rename behavior

Account UUIDs are random opaque identifiers, but embedding them makes public
replays linkable to the same account. Names are snapshots: renaming an account
changes future matches only. Old replays continue to display the names used at
match time.

The service should expose only the public nametag and opaque UUID. Tokens,
token hashes, IP-derived abuse signals, and transport addresses never enter a
match contract or replay. Deleting/resetting local credentials does not rewrite
old replays.

## Implementation order

1. Add the profile column/migration, strict validation, authenticated read and
   update routes, and account-creation default state.
2. Add in-app first-use/edit UI and the platform-neutral profile cache.
3. Have room/Quick Match snapshot and deliver both authenticated profiles;
   define the local/direct fallback.
4. Freeze `T3NID001`, bind its digest into the pre-game agreement, and add
   malformed/cross-room/cross-account tests.
5. Render comparison images, select the accepted placement/color, and attach
   the bounded row only to the New Classic netplay intro.
6. Add the optional V13 tail, byte-preserving replay/download behavior, detail
   display, replay-renderer support, and old-reader compatibility tests.

## Blocking product decisions

The recommended defaults are deliberately concrete. Implementation needs only
these confirmations:

1. Use one account nametag (recommended), or allow a separate per-match alias?
2. Accept the 1..24 printable-ASCII first version (recommended), or block until
   the project has a broader distributable Unicode font?
3. Allow duplicate case-preserving names tied to unique UUIDs (recommended),
   or require globally unique case-insensitive names?
4. Permit public replay files to expose both opaque account UUIDs (recommended
   for future match history), or store only match-time names until a later
   replay version?

The V13-tail approach, direct-IP unverified policy, snapshot-on-rename behavior,
and preservation of source character text do not require further decisions
unless one of the four answers changes their premise.

# New Classic netplay nametags and replay identity

Status: approved product contract, 2026-08-20. The portable layout validator,
account-profile service, strict paired snapshot, T3NID001 peer agreement, and
VS-title substitution are implemented. The retained Online Profile owner now
enforces first-use naming, later rename, and separate-device link-code flows.
Production deployment of the profile migration/routes remains the final
service gate.

## Current facts

The account identity is deliberately opaque. The service `users` table stores
a UUID, mutable public nametag, salted credential hashes, and timestamps.
Every linked device receives a separate credential for the same UUID. Replay
and leaderboard rows still display the eight-character name saved inside each
V14 replay; that is replay metadata, not yet the account-name snapshot.

The room service authenticates each peer's UUID and T3ROOM3 now freezes both
UUID/nametag pairs before peer negotiation. Schema-4 `T3NLC001` carries the
SHA-256 of the fixed T3NID001 value; it carries no raw presentation text and
adds nothing to per-frame traffic.

The netplay VS splash reuses the ordinary MAINL stage owner. Its English layout
draws each character's two-line title and name at x=80/336 and y=276, 292, and
308 using bold PC-98 8x16 graphics text. The splash finishes before frame-zero
network proof, so an added nametag is presentation metadata: it must agree on
both peers, but it does not belong to simulation, rollback, input, slowdown, or
audio identity.

V14 has no spare header bytes that can be safely reused. Its fixed 622-byte
header, summary, checkpoint reservations, flags, and zero-reserved fields are
already validated by existing readers. Installation-local `.PODD` sidecars
currently remember only that a replay was netplay and which player was local;
they are digest-bound but do not travel with a copied or downloaded replay.

V14 does, however, explicitly permit data after the declared logical input
stream. Existing readers ignore an unrecognized tail, and Replay Patch's
optional `T3RACC1` accelerator already lives in that domain. Therefore account
identity does not require V14.

## Recommended product behavior

Use one public **account nametag**, not an independently editable per-match
alias. This keeps matchmaking, future match history, and moderation coherent,
and avoids an impersonation surface that adds no useful game behavior.

The first version uses printable ASCII with exact geometry validation. Reject
leading, trailing, and repeated spaces rather than silently normalizing them;
reject controls; preserve case for display. Deterministically wrap at word
boundaries into at most two 28-cell rows and reject any word wider than 28
cells or any name requiring a third row. Nametags need not be globally unique:
the immutable account UUID is the identity, while two players may legitimately
choose the same visible name.

Each 224-pixel player column contains 28 fixed-width 8-pixel cells. Two full
rows display 56 cells; because one source space becomes the line break, the
theoretical largest accepted input is 57 bytes (`28 + space + 28`). This is a
geometric maximum, not a promise that every 57-byte sequence fits: validation
is the deterministic wrap itself. Full Unicode should wait until the localized
font pipeline has a distributable glyph/fallback owner; silently replacing
unsupported characters with `?` would make distinct accounts look identical.

On first use of account-backed Online Match, require the player to choose a
nametag after automatic credential enrollment and before entering matchmaking.
Expose later edits at `Online Match -> Online Profile`; an eventual
`Option -> Online & Privacy` alias may open the same retained owner. The server
is authoritative for account-backed rooms; a cached local copy only avoids a
blank UI while a refresh is in flight.

The Worker should snapshot both authenticated profiles when it pairs a room
and send the pair to both clients over the authenticated room WebSocket. The
Durable Object must not accept a client-supplied account UUID or account
nametag as authority. Direct UDP has no account authority, so it may use each
side's locally configured nametag and must mark both identities as local/
unverified.

Replace the two localized character-title rows with the centered wrapped
nametag and retain the source `- Character -` line unchanged beneath it. The
portrait and character identity therefore remain unambiguous while the more
prominent title area identifies the remote player. Ordinary Story/VS and
account-free direct play retain the source titles unless a later local-alias
policy explicitly opts in.

## Network contract

The distinct fixed, bounded identity snapshot does not inflate gameplay input
packets. `T3NID001` contains:

- identity kind (`account` or `local`);
- both 16-byte UUID values, zero for local/direct identities;
- both length-prefixed 57-byte ASCII nametag slots;
- the exact room/match identity needed to prevent reuse across a different
  pairing; and
- a digest over the complete value.

The account-backed room service supplies this value from authenticated server
state. Both peers include its digest in the pre-frame match agreement. The
value never enters per-frame traffic, gameplay state, or rollback snapshots.
Changing it requires a new pre-game contract version but not a gameplay
ruleset version.

## Replay compatibility

Add one fixed, versioned `T3NID001` block immediately after V14's declared
logical input stream. If `T3RACC1` is also present, its packed records and final
footer remain after the identity block so old accelerator readers continue to
find their footer at end-of-file.

The block snapshots:

- P1 and P2 account UUIDs, or zero UUIDs for direct/local play;
- P1 and P2 match-time nametags;
- account/local flags;
- the final netplay transcript digest; and
- a digest binding the extension to the canonical logical V14 bytes.

This extension is descriptive evidence, not deterministic replay authority.
The ordinary V14 parser and verifier continue to derive gameplay and scores
solely from the frozen V14 stream. A malformed identity block invalidates only
the enhanced identity view; it must not reinterpret gameplay. Online services
may impose a stricter policy when accepting a future VS submission.

The V14 writer must learn to preserve/emit recognized typed tails before this
ships. Today it writes only the logical V14 document, and its neutral document
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

One UUID may own multiple device credentials. Linking uses a short-lived,
single-use code created by an already authenticated device; redeeming it issues
a new random bearer token for the same UUID. Tokens are never copied between
machines. Secondary credentials are independently listable/revocable, while
renaming the profile leaves every credential and the UUID stable.

## Implementation order

1. ~~Add the profile column/migration, strict validation, authenticated read and
   update routes, and account-creation default state.~~
2. Add in-app first-use/edit UI. **Complete.** A persistent profile cache is
   intentionally deferred; account-backed launch fetches server authority.
3. ~~Have room/Quick Match snapshot and deliver both authenticated profiles;
   define the local/direct fallback.~~
4. Freeze `T3NID001`, bind its digest into the pre-game agreement, and add
   malformed/cross-room/cross-account tests. **Complete.**
5. ~~Render comparison images, select the accepted placement/color, and attach
   the bounded rows only to the New Classic netplay intro.~~
6. Add the optional V14 tail, byte-preserving replay/download behavior, detail
   display, replay-renderer support, and old-reader compatibility tests.

## Approved decisions

- One changeable account nametag; no per-match alias.
- Printable ASCII now, validated against two 28-cell rows without word splits.
- Duplicate case-preserving names; the UUID is the stable identity.
- Public typed V14 tails may expose both opaque account UUIDs.
- Character names remain unchanged; nametags replace the two title rows.
- Multiple devices receive separate revocable credentials for the same UUID.

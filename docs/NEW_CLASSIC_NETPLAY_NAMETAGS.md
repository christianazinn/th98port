# New Classic netplay nametags and replay identity

Status: approved product contract, revised 2026-08-22 for the final public V14
layout. Account profiles, paired identity negotiation, VS-title substitution,
and portable replay fallback names are implemented.

## Product contract

Each account has one changeable public nametag and one immutable opaque UUID.
Nametags need not be globally unique. The service UUID is the durable identity;
ordinary UI does not expose it. Each linked device receives an independent,
revocable credential for the same UUID.

Nametags use printable ASCII for now. Validation rejects leading, trailing, or
repeated spaces and deterministically wraps whole words into at most two
28-cell rows. A word may not exceed 28 cells. The largest possible source is
57 bytes (`28 + space + 28`), although not every 57-byte sequence fits. Unicode
waits for a distributable localized font and fallback-glyph owner.

The MAINL netplay splash replaces the two character-title rows with the
centered wrapped nametag and retains the source `- Character -` line. Ordinary
Story and local VS retain source character titles.

## Network authority

The room service snapshots both authenticated UUID/nametag pairs when it pairs
a match. Both peers bind that fixed snapshot into their pre-frame agreement;
identity never enters gameplay state, rollback snapshots, per-frame traffic,
slowdown accounting, or audio state. Direct UDP has no server authority and
may use local names with zero UUIDs.

Tokens, token hashes, IP-derived abuse signals, and transport addresses never
enter a match contract or replay. Linking another device uses a short-lived,
single-use code created by an authenticated device and issues a new credential
for the same UUID.

## V14 replay representation

The public V14 header is 896 bytes. Its 274-byte identity extension stores:

- ruleset, recording flags, recorder role, and recorder source;
- P1 and P2 16-byte opaque account UUIDs;
- a 16-byte match ID;
- a length byte and fixed zero-padded 57-byte fallback nametag slot for each
  player; and
- 106 zero-reserved bytes.

The serialized nametags are portable match-time/offline fallbacks. A connected
viewer should resolve each nonzero UUID and display that account's latest
public nametag. If lookup is unavailable, the account is unlinked, or a public
consumer such as a static scoreboard cannot query the profile service, it may
display the stored fallback. This preserves portability without making an old
name the durable account identity.

Replay Patch and converted historical recordings have zero UUIDs, zero match
ID, and empty fallback slots. Netplay writers serialize the canonical P1/P2
snapshot negotiated before frame zero. Installation-local metadata may still
record which peer was local, but the portable replay's `recorder_role` already
provides the stable P1/P2 projection needed for score and character summaries.

Fallback names are descriptive evidence, not simulation authority. Parsers
validate their exact printable-ASCII/two-line geometry and zero padding, while
gameplay verification continues to derive inputs, scores, splits, and terminal
state from the deterministic replay stream. Authentication secrets are never
replay data.

## Rename and privacy behavior

Renaming an account changes the name returned for every replay bearing its
UUID. The replay fallback remains the name captured at that match and is used
only when current profile resolution is unavailable. Public replays therefore
remain linkable by opaque UUID; this is intentional for profiles, personal
bests, match history, and future ratings.

## Approved decisions

- One mutable public account nametag; no independent per-match alias.
- Printable ASCII now, with exact two-by-28-cell geometry validation.
- Duplicate visible names are allowed; the UUID is the stable identity.
- The final public V14 is a single 896-byte layout; no earlier 768-byte beta
  layout is accepted.
- Current profile names take precedence over serialized fallback snapshots.
- Character names remain unchanged; nametags replace only the two title rows.
- Multiple devices receive separate revocable credentials for one UUID.

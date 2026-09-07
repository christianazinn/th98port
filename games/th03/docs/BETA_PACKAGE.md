# PoDD Port beta build

This package is an unsigned development build of the native TH03 port. It is
intended for private beta testing, not as a finished installer.

The package contains no original game, font, music, graphics, or replay data.
It does include `PODDUI/`, the independently generated, repo-owned New Classic
menu artwork. Keep that directory beside the installed executables; standalone
content instead stages the same files beside `AZINN.DAT` as `content/PODDUI`.
Run `th03-app` with the same external `--data`, `--main`, `--mainl`, `--op`, and
`--font` inputs documented in the repository README and NETPLAY_TESTING guide.
`th03-replay-video`, `th03rv`, `th03-replay-check`, and `th03-headless` are
included as command-line tools. Consult the repository documentation or use
`--help` where the tool supports it.

`podd-bootstrap` is the data-free, stable launch owner used by the standalone
and future installed updater. With no local activation state it launches the
packaged `th03-app` unchanged; it never contacts a network or activates a
package by itself.

`BUILD-MANIFEST.txt` records the exact source revision and SHA-256 of every
packaged file. Dependency license texts are under `licenses/`.

Manual hosted builds also emit a sibling `.t3pkg`. This is the uncompressed,
strictly validated inert updater input specified by `UPDATE_PACKAGE.md`; it is
not the tester-facing archive and does not activate itself.

The macOS builds are neither code-signed nor notarized. Testers must approve
the downloaded build through the normal macOS security UI. The Linux build
targets a current x86-64 Ubuntu userspace. The Windows build targets x86-64.

Do not report the absence of original data as a bug. For crashes, attach the
PoDD Port log from the platform's normal application-data directory together
with the build manifest and exact reproduction steps.

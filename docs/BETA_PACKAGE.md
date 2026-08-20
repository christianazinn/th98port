# PoDD Port beta build

This package is an unsigned development build of the native TH03 port. It is
intended for private beta testing, not as a finished installer.

The package contains no original game, font, music, graphics, or replay data.
Run `th03-app` with the same external `--data`, `--main`, `--mainl`, `--op`, and
`--font` inputs documented in the repository README and NETPLAY_TESTING guide.
`th03-replay-video`, `th03rv`, `th03-replay-check`, and `th03-headless` are
included as command-line tools. Consult the repository documentation or use
`--help` where the tool supports it.

`BUILD-MANIFEST.txt` records the exact source revision and SHA-256 of every
packaged file. Dependency license texts are under `licenses/`.

The macOS builds are neither code-signed nor notarized. Testers must approve
the downloaded build through the normal macOS security UI. The Linux build
targets a current x86-64 Ubuntu userspace. The Windows build targets x86-64.

Do not report the absence of original data as a bug. For crashes, attach the
PoDD Port log from the platform's normal application-data directory together
with the build manifest and exact reproduction steps.

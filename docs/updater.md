# Startup updates

With Wi-Fi connected, the launcher checks the latest public GitHub Release at
https://api.github.com/repos/Raihwy3/eyes-of-gold/releases/latest before starting
the engine. B skips the check. Offline, unavailable, rate-limited or malformed
responses leave normal startup available. A check has a short timeout and runs
on a separate thread, so the startup UI remains responsive.

A newer release offers A: Install or B: Not now, also available by touch. A
verified download is staged on the SD card. Only `eyes-of-gold-launcher.3dsx`
and `eyes-of-gold-launcher.smdh` are replaced. ROMs, saves, options and converted
assets are left alone. The downloaded ZIP remains as `launcher-update.zip`.
The update applies on the next launch; A exits after installation, while B can
continue running the already loaded build. A journal and backups allow an
interrupted file replacement to be rolled back on the next launch.

An empty `no-update-check.flag` beside the launcher disables automatic checks.
Recovery still runs when checks are disabled.

## Publishing

Set `NATIVE_RELEASE_TAG` in `native-engine/include/native_version.h` to the
release tag before building. Supported tags are `vMAJOR.MINOR.PATCH` and
`vMAJOR.MINOR.PATCH-rREVISION`. The installed build compares this tag with the latest public release, so it
will not offer to replace itself with the same version.
Drafts, prereleases, equal versions and older versions are ignored. Mark the
release as Latest when publishing. The title can still say Prototype; the
GitHub prerelease checkbox must be off for startup updates.

Upload the install ZIP as a release asset named `eyes-of-gold-launcher*.zip`
or `EoG_*.zip`. It must contain the standard `3ds/eyes-of-gold-native-game/`
launcher pair. Source archives are not update packages. GitHub must provide
its `sha256:` asset digest; downloads must match that digest and the declared
size, which is limited to 8 MiB. Both ZIP CRCs and launcher headers are checked.
HTTPS certificate and hostname verification remain enabled, including redirects.

## Building

Install devkitPro packages `3ds-curl`, `3ds-mbedtls` and `3ds-zlib` alongside
`3ds-dev`. `EOG_UPDATE_PORTLIBS` may point to an alternative portlibs prefix.
The build also accepts an ignored local prefix at
`local/update-deps/opt/devkitpro/portlibs/3ds`. Dependencies are not downloaded
automatically by a build. Python host checks additionally need system curl,
MiniZip, zlib and OpenSSL development libraries and the `openssl` command.

`native-engine/tools/check_updater.py` exercises the real parser, transport and
installer with memory sanitizers, a local TLS server and disposable directories.
The 3DS Wi-Fi and startup UI paths still require hardware testing.

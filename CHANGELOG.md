# ECHO Changelog

All notable changes to this project are documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

Versions are identified by firmware version code (`YYYYMMDDNN`), matching the
`firmware_echo_<device>_<versioncode>.bin` artifacts attached to each release.

---

## [2026092901] — 2026-09-29

### Removed
- `docs/ha_config/` — Home Assistant dashboard YAML left over from the HA integration that was
  removed from the firmware. It documented a feature that no longer exists. `FORK_NOTICE.md` no
  longer lists the deleted `HaWebhook` and `HaSettingsActivity` sources.
- `echo_nofindmy` PlatformIO environment — it only set a `NO_FINDMY` define that no source file
  reads, so it built the same firmware as `echo_x3` under a different name.

### Fixed
- **Calendar sync over HTTPS.** `CalendarActivity::syncCalendar()` gave `esp_http_client` no
  server-verification option, and this SDK build has `CONFIG_ESP_TLS_INSECURE` off, so every
  `https://` ICS URL (Google, iCloud) was refused at the TLS layer. It now attaches the ESP-IDF CA
  bundle, as `OtaUpdater` already does.
- **Calendar sync no longer overwrites the cache with an error page.** A non-200 response (401,
  404, 500...) used to be saved as `calendar.ics` and parsed, wiping the event list. Only a 200
  body is cached now.
- **Calendar sync can no longer reboot the device on low memory.** The 32 KB download buffer is
  allocated with `nothrow` and the sync is skipped if it is unavailable, instead of `abort()`. The
  body is written to SD straight from that buffer, without a second 32 KB `String` copy.
- **OPDS grid: failed covers are not refetched on every redraw.** Covers download during render,
  so an unreachable cover host stalled each key press for the full connect timeout per cover. A
  failed cover is now remembered until the next feed load.

### Changed
- `platformio.ini`: the simulator library is pinned to `crosspoint-simulator@09e28c9`. Its
  unpinned HEAD tracks CrossPoint develop and no longer matched this fork's HAL
  (`verifyPowerButtonWakeup`), so a clean `pio run -e simulator` failed. The simulator env also
  defines `CROSSINK_VERSION`, which the stub OTA updater uses to pick the cancellable
  `installUpdate()` signature.

## [2026091201] — 2026-09-12

### Added
- **Cloud Library: a WebDAV *client* for cloud storage (Koofr, Nextcloud, ownCloud).** This is the
  mirror image of the WebDAV *server* in `src/network/WebDAVHandler.cpp`, which exposes the local SD
  card on the LAN — this one reaches out to a remote drive and pulls books down from it.
  - `lib/WebDavParser/` — a streaming, namespace- and case-agnostic parser for a PROPFIND
    `multistatus` body, built on the expat parser already linked for OPDS. It extracts only `href`,
    `resourcetype/collection`, `displayname` and `getcontentlength`, and never buffers the whole
    response. Also holds the percent-encode/decode and path helpers the rest of the feature shares.
    Covered by `test/run_webdav_parser_test.sh`.
  - `src/network/WebDavClient.{h,cpp}` — issues `PROPFIND` with `Depth: 1` over HTTP Basic auth.
    Arduino's `HTTPClient::sendRequest()` is what makes a non-standard verb possible;
    `HTTPC_FORCE_FOLLOW_REDIRECTS` keeps the verb across a 301, which the default policy would
    silently downgrade to `GET`. The collection's own entry is filtered out of the listing and the
    rest is sorted folders-first.
  - `src/activities/browser/CloudLibraryActivity.{h,cpp}` — the browser, laid out like the local
    file browser (same list, icons, path line, and Back/Confirm behaviour). Only `.epub`, `.txt`
    and `.xtc` are listed. Selecting a book downloads it to the SD card root and reboots straight
    into the reader, which is also how the WiFi session's heap fragmentation gets cleared.
  - `src/activities/settings/CloudLibrarySettingsActivity.{h,cpp}` — **Settings → System →
    Cloud Library**, with WebDAV URL, Username, Password and Root Folder.
  - Settings live in `CrossPointSettings` as `cloudWebdavUrl` / `cloudWebdavUsername` /
    `cloudWebdavPassword` / `cloudWebdavRootPath`, registered in `SettingsList.h` so the web UI
    gets a **Cloud Library** card and `JsonSettingsIO`'s generic loop persists them. The password
    is stored obfuscated, like the other credential fields.
  - A **Cloud Library** home-screen entry appears once a WebDAV URL is configured.
  - Entries per folder are capped at 64; a larger folder is shown capped, with `64+` on the path line.

### Fixed
- **Home carousel cache could hide a newly configured Cloud Library.** The Lyra carousel bakes the
  home menu into cached frames, but `buildCarouselCacheKey()` keyed only on book covers and stats
  files, so a menu-composition change was invisible until the snapshot was rebuilt for some other
  reason. The key now includes whether a Cloud Library is configured. (The same staleness still
  applies to the OPDS and Bookmarks entries and to a language change — pre-existing, untouched here,
  since fixing those means threading menu state through the shared cache path.)

### Security
- **Stored secrets are no longer sent to the browser.** `GET /api/settings` returned the plaintext
  value of every string setting, so an obfuscated credential would have been rendered into the
  settings page's HTML. Obfuscated string settings now report `secret: true` and an empty value;
  the field renders as an empty password box marked `(unchanged)` and only overwrites the device
  when retyped — the contract the OPDS and Wi-Fi password fields already followed.

## [2026072404] — 2026-07-24

### Changed
- **Home menu: proper calendar icon.** The Calendar entry used the `Chart` glyph (three bars),
  which read as a bar chart, not a calendar. Added a dedicated 32×32 `CalendarIcon`
  (`src/components/icons/calendar.h`), a `Calendar` value in the `UIIcon` enum, and the mapping
  in `LyraTheme::iconForName` (the only theme that draws home-menu icons; Classic/RoundedRaff
  ignore them). The Home entry now uses it.
- **Home menu: "File Transfer" renamed to "Transfer & Config"** (`STR_TRANSFER_AND_CONFIG`),
  since that entry opens the web-server / browser-configuration screen, not just file transfer.
  Same action and icon; label only.

## [2026072403] — 2026-07-24

### Fixed
- **Button-config labels in the web UI now match the device.** The settings web page groups
  by `category`, and every button setting used `STR_CAT_CONTROLS`, so they appeared flat under
  one "Controls" section — producing duplicate, context-free labels ("Long-press Action" for
  both the side and power buttons, "Orientation Aware" for both side and front). The labels
  themselves were already identical to the device (both come from `I18N.get(nameId)`); what was
  missing was the grouping. The button settings are now categorised `STR_POWER_BUTTON` /
  `STR_FRONT_BUTTONS` / `STR_SIDE_BUTTONS`, so the web renders three sections mirroring the
  device's submenus. Device-safe: `category` is read only by the web API
  (`CrossPointWebServer.cpp`), while the device builds its menus by name, so on-device
  behaviour is unchanged. Tilt page-turn stays under "Controls".

### Changed
- CI: `pio check` and `pio run` are retried (3×) to ride out the transient
  "Failed to install Python dependencies into penv" that PlatformIO can hit while populating
  its internal venv on a fresh runner. Real cppcheck defects / compile errors still fail.

## [2026072402] — 2026-07-24

### Added
- **"Configure from browser" entry in Settings > System.** A dedicated, discoverable entry
  (`STR_CONFIGURE_FROM_BROWSER`) that opens the existing web-server screen — WiFi QR, URL QR,
  IP and hotspot name — instead of that flow being hidden inside "File transfer". Reuses the
  existing QR/screen code; no QR logic was duplicated. English + Italian strings added; the
  other 21 languages inherit English via the standard `gen_i18n` fallback.
- **Calendar URL and dictionary text size are now configurable from the web UI.** Exposed via
  the schema-driven `/api/settings` (read + write). They add nothing to the on-device menus,
  which keep their dedicated Calendar / Dictionary activities.

### Changed
- **Hotspot SSID and mDNS hostname renamed** from `CrossPoint-Reader` / `crosspoint` to
  `echo-x3-XXXX` / `echo-x4-XXXX` (4-hex MAC suffix), matching the station hostname set in
  `WifiSelectionActivity`. This also fixes a latent inconsistency where the STA-mode mDNS
  registered `crosspoint` while the hostname was `echo-x3-XXXX`.
- **Italian translations completed.** 135 keys that were missing from `italian.yaml` (and so
  fell back to English on-device) are now translated; the file is aligned with `english.yaml`.

## [2026072401] — 2026-07-24

### Fixed
- **cppcheck job in CI.** It had been failing on every run, including before the workflow
  rework, on 1 high and 8 low defects (`pio check` runs with `--fail-on-defect low`, so the
  stylistic ones were blocking too). All are resolved with real code changes, not
  suppressions.
  - `CalendarActivity.cpp` **[high, uninitdata]**: the 32 KB ICS download buffer was
    allocated with `new char[N]` and handed to `String()`/`parseIcs()` with only the
    received prefix written. It is now value-initialized (`new char[N]()`) — the zeroing
    runs once per manual calendar sync, not in a loop.
  - `CalendarActivity.cpp`: `received` no longer has a never-read initializer or an
    over-wide scope; it is declared where it is assigned.
  - `CalendarActivity.cpp`: the line parser now tracks the line end as a pointer and strips
    a trailing `\r` under an `lineEnd != p` guard. This drops the `lineLen > 0` condition
    cppcheck read as always-true while keeping the empty-line case safe, and it fixes a
    latent off-by-one where the old post-strip length left the parser one byte short and
    produced a spurious empty-line iteration for every `\r\n`-terminated line.
  - Raw loops replaced with the algorithm cppcheck suggested: `std::find_if` in
    `DictionaryLookupActivity`, `DailyReadingLog` and `DictionarySettingsActivity`,
    `std::transform` (with a `reserve()`) for the dictionary history list, and a range
    `insert` in `IfFoundActivity`.

## [2026072302] — 2026-07-23

### Removed
- **Lexend Deca is no longer built into the firmware** — it now ships as an SD card font
  family, installable over WiFi from the font download screen where it appears as
  "Lexend Deca". This frees 766 KB of flash: usage drops from **94.1% to 82.4%**. ChareInk,
  Bitter and the Inter UI font are unchanged, and all nine hyphenation languages are retained.
  Controlled by the new `OMIT_LEXENDDECA_FONT` build flag, set for `echo_x3` and `echo_x4`.

  Devices that had Lexend Deca selected keep their stored setting and render in ChareInk at
  the same size until Lexend Deca is installed from SD. `FONT_FAMILY` enum values are
  deliberately unchanged, so a device set to Bitter or ChareInk keeps that choice across the
  upgrade. The font picker no longer maps its position directly onto the stored enum value —
  the two diverge once a built-in family is omitted, so `buildFontFamilySetting()` and
  `FontSelectionActivity` now translate between them explicitly.

### Changed
- Release workflow now publishes releases directly instead of creating drafts, so
  `/releases/latest` — the endpoint `OtaUpdater` polls — is updated as soon as a tag is pushed.
- `AGENTS.md` gained a "Flash Budget" section documenting the 96% ceiling and how to audit
  per-symbol flash usage.

### Fixed
- `test/run_hyphenation_eval.sh` could not compile: the generated tries include
  `Epub/hyphenation/SerializedHyphenationTrie.h`, which needs `lib/Epub` on the include path.
  Added it. The suite now runs (English 99.10%, Italian 99.99%).

---

## [2026072301] — 2026-07-23

### Fixed
- **GitHub Actions release workflow** — "Compile Release" built the upstream CrossPoint variants
  (`tiny`/`xlarge`/`no_emoji`) instead of `echo_x3`/`echo_x4`, so it never produced ECHO firmware,
  and attached assets named `firmware-<variant>-v<version>.bin` which `OtaUpdater` does not
  recognise. It now builds both ECHO environments and attaches
  `firmware_echo_x3_<versioncode>.bin` / `firmware_echo_x4_<versioncode>.bin`, matching the names
  the OTA client looks for.
- Release version injection wrote `upstream_base`, which the ECHO environments never read, and did
  so unanchored so it hit both the `[crosspoint]` and `[echo]` sections. It now updates only
  `version` inside `[echo]`, which is what `ECHO_VERSION` — and therefore the OTA version
  comparison — is built from.
- The release tag filter was `*`, so any tag (including leftovers like `firmware` or
  `sd-fonts-m1-b4-r3`) triggered a release. It is now `v*`, and the release reuses the pushed tag
  instead of creating a second `v`-prefixed one.
- Removed the release catalog steps. They committed `docs/catalog` to the repository's default
  branch as `uxjulia <julia@uxj.io>`, which fails on a protected branch and targeted `master` while
  development happens on `main`. Nothing in the firmware reads that catalog — `OtaUpdater` queries
  the GitHub releases API directly.
- CI ran a bare `pio run` (only `default_envs`, i.e. `echo_x3`) but uploaded `tiny`/`xlarge`/
  `no_emoji` artifacts with `if-no-files-found: error`, failing on every push. The
  release-candidate workflow attached the same non-existent variants. Both now build and publish
  `echo_x3` and `echo_x4`.

### Added
- **Automatic emergency recovery** — if `/force_update.bin` is present on the SD card root at boot,
  ECHO validates and flashes it unattended, then restarts. The file is renamed to
  `force_update.bin.applied` before flashing, so an interrupted or failed flash can never boot-loop
  reflashing. A corrupt image is rejected before any write. Implemented in `main.cpp` and a new
  auto-flash mode of `SdFirmwareUpdateActivity`; reuses the existing validated flash path.

### Changed
- Repository renamed `FindMyInk` → `Echo`. The OTA endpoint now points at
  `https://api.github.com/repos/filippocobelli/Echo/releases/latest`. Devices running an older
  build keep polling the old repository name and must be updated once over USB or from the SD card.
- Removed all `CrossInk`/`vcodex` branding from the firmware: build macros renamed
  `CROSSINK_*` → `ECHO_*`, HTTP User-Agent is now `ECHO-ESP32-<version>`, the web UI, the settings
  version label, the simulator window title and the smoke-test environment variables all read ECHO.
  Font download URLs still point at `uxjulia/crossink-fonts`, which is the real upstream host.
- `README.md` rewritten from scratch; `docs/images/logo_b.svg` and `logo_f.svg` added.
- Repository layout: `HA/` moved to `docs/ha_config/`, `GOVERNANCE.md` removed (it described
  CrossPoint Reader's community process, not this fork's).

### Removed
- `.env` — leftover from the abandoned Home Assistant integration. Nothing in the build or the
  firmware read it.

---

## [2026070701] — 2026-07-07

### Added
- **OTA firmware updates** from GitHub releases, with distinct error states for no WiFi, download
  failure and flash failure.
- **StarDict dictionaries** from the SD card (`/dictionaries/<name>/`), with a bounded-RAM
  binary-search index built on first use, a standalone Dictionary app, in-reader word lookup and
  lookup history. Uncompressed `.dict` only.
- **Favorites** — long-press a book in Recent Books to pin it to the top of the library.
- **Per-book stats corrections** — adjust recorded reading time for a given day, override the
  displayed start date, reset a single book's history.
- **Reading Dashboard sleep screen** — daily goal, streak, today's total, last book, latest
  achievement.
- **WebDAV** — the built-in web server mounts the SD card as a network drive.
- **OPDS cover grid** — two-column view with covers cached under `/.crosspoint/opds_covers/`.
- **Liang hyphenation** with language auto-detection: en, it, de, es, fr, ru, pl, sv, uk.
- **Move finished books** to a `/Read/` folder from the reader menu.
- Extended long-press actions in the library, and a configurable auto page turn interval.

### Changed
- `BookReadingStats` on-disk format bumped to v4 (adds a display-only start-date override).

---

## [2026060201] — 2026-06-02

### Added
- **Calendar app** — iCal (`.ics`) feed over WiFi with an offline cache on the SD card.
- **KOReader Sync** register/log-in flow with account-aware settings.
- ECHO-branded web settings interface.

### Fixed
- WiFi settings page crash — the modem needs ~300 ms to settle after disconnect before
  `WiFi.scanNetworks()` is safe on a cold start.
- WiFi connection timeout with a correct password — `WiFi.disconnect(true, true)` was forcing a
  full modem power cycle that `WiFi.begin()` could not recover from in time.
- Standby battery drain, via the rebase onto the newer upstream base.

### Changed
- Rebased on upstream CrossInk 1.3.1 (288 commits, 769 files). Brings adjustable line spacing,
  chapter/book time-left estimates, nearby stats sync over ESP-NOW, EPUB superscript/subscript,
  the Minimal sleep screen, the Recent Books cover grid, and collapsible settings submenus.

### Removed
- **Home Assistant webhook** integration — out of scope per `SCOPE.md` (active connectivity and
  background WiFi tasks).

---

## [2026052601] — 2026-05-26

### Added
- ECHO logo, variant B (boot) and variant F (sleep outline), and the tagline "Find your ECHO."
- **"If found, please return me"** — reads `/if_found.txt` from the SD card root, scrollable, bold.
- Reading Stats visibility toggle; normalised ON/OFF toggles across settings.
- 24 UI languages.

### Changed
- Project renamed FindMyInk → ECHO.
- `FORK_NOTICE.md` added documenting the full fork chain and per-component provenance.

---

## [2026052001] — 2026-05-20

### Added
- CrossInk typography: ChareInk, Bitter, Lexend Deca for body text, Inter for the UI.
- `tiny` build variant with emoji support.
- Reading Stats, heatmap and achievements (from cpr-vcodex).
- EPUB bookmarks, daily goal and streak.
- Stats export/import as JSON.

### Changed
- Base firmware: CrossInk 1.2.9.1.
- PlatformIO environments renamed to `echo_x3` / `echo_x4` / `simulator`.

---

## [2026050101] — 2026-05-01

### Added
- Initial fork from cpr-vcodex (franssjz).
- PlatformIO environments `echo_x3` and `echo_x4`.
- Version string `1.0.0-echo`.

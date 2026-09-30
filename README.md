[🇮🇹 Italiano](README.it.md)

# ECHO

<p align="center"><img src="docs/images/mockups/echo_hero.png" alt="ECHO on Xteink X3 and X4"></p>

<p align="center"><sub>Device photo: Xteink. ECHO is an independent project, not affiliated with Xteink.</sub></p>

<img src="docs/images/logo_b.svg" width="80">

Open-source e-reader firmware for the Xteink X3 and X4.

> ECHO is a fork of [cpr-vcodex](https://github.com/franssjz/cpr-vcodex)
> by franssjz, itself based on
> [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader).
> See [FORK_NOTICE.md](FORK_NOTICE.md) for full attribution.

## Download

Grab the firmware for your device from the [**v1.0.1 release**](https://github.com/filippocobelli/Echo/releases/tag/v1.0.1)
(all versions are on the [Releases](https://github.com/filippocobelli/Echo/releases) page).

| Device | File |
|--------|------|
| Xteink X3 | `firmware_echo_x3_<version>.bin` |
| Xteink X4 | `firmware_echo_x4_<version>.bin` |

## Flash

Web flasher: https://crosspointreader.com/#flash-tools (requires Chrome or Edge — WebSerial).

- **X3:** after flashing, unplug the USB cable and plug it back in. Do **not** press Reset.
- **X4:** after flashing, press Reset, then immediately hold Power for 3–5 seconds.
- **Recovery:** copy a known-good firmware to the SD card root, renamed to `force_update.bin`,
  then reboot. At boot ECHO detects the file, validates it, flashes it unattended and restarts —
  no menu navigation needed, which is the point when the UI won't come up. The image is validated
  before anything is written, so a corrupt file is rejected instead of bricking the device, and
  the file is renamed before flashing so an interrupted flash can never boot-loop. You can also
  flash a hand-picked `.bin` interactively from **Settings > System > SD Card Firmware Update**.
  If the device cannot boot at all, reflash over USB from the web flasher.

## Devices

| Device | Display | Resolution | Role |
|--------|---------|------------|------|
| Xteink X3 | 3.68" SSD1677 | 528×792 | ⭐ Primary |
| Xteink X4 | 4.26" SSD1680 | 800×480 | Secondary |

Both run an ESP32-C3 (single-core RISC-V at 160 MHz, no PSRAM, ~380 KB usable RAM) with a microSD
slot over SPI, 2.4 GHz WiFi and BLE. Build and flash the binary matching your device.

## Features

### 📚 Reading
- EPUB 2/3, TXT and XTC, with table of contents, bookmarks and go-to-percent
- CrossInk typography: ChareInk, Bitter, Lexend Deca for body text, Inter for the UI
- Adjustable line spacing (70%–200%), font size from 9pt to 16pt, emoji support
- Liang hyphenation for justified text — en, it, de, es, fr, ru, pl, sv, uk
- Bionic Reading, Guide Dots, forced paragraph indents
- Favorites: long-press a book in the library to pin it to the top
- Reading stats, heatmap, achievements, configurable daily goal and streak
- Move finished books to a `/Read/` folder from the reader menu
- Auto page turn with a configurable interval
- "If found, please return me" — reads `/if_found.txt` from the SD card root

### 🗓 Calendar
- iCal (`.ics`) feed over WiFi, URL configurable on-device
- Month view (today highlighted, a dot under days with events) with the agenda of upcoming events below
- Left / Right change month, Up / Down scroll the agenda
- Cached to the SD card, so it opens with no network

### 📖 Dictionary
- StarDict dictionaries read from `/dictionaries/` on the SD card
- Monolingual and bilingual dictionaries
- In-reader word lookup with history, plus a standalone Dictionary app
- Uncompressed `.dict` only — `.dict.dz` is not supported

### 🌐 Network
- OPDS browser, with a list view or a two-column cover grid
- Cloud Library: browse and download books from a WebDAV drive (Koofr, Nextcloud, ownCloud)
  - Koofr: use an *app password* and enter it **without spaces**
- Calibre-Web send-to-device
- WebDAV — mount the SD card as a network drive from Finder, Explorer or iOS Files
- OTA firmware updates from this repository's GitHub releases
- KOReader Sync for reading position across devices
- Web settings UI and WebSocket-based fast uploads from a browser

### 💤 Sleep screen
- Reading Dashboard: daily goal progress, streak, today's total, last book, latest achievement
- Custom images from the SD card, book cover mode, sequential or shuffle

### 🔧 System
- Emergency recovery: drop `force_update.bin` on the SD root and reboot — flashes unattended
- SD card firmware update, interactive, from the settings menu
- exFAT and FAT32 SD cards
- 24 UI languages
- Runtime X3/X4 detection

## Screenshots

Captured from the ECHO simulator at native resolution (528×792, Xteink X3), with sample data and public-domain books.

<table>
  <tr>
    <td align="center"><img src="docs/images/screenshots/01-boot.png" width="220" alt="Boot screen"><br><sub>Boot screen</sub></td>
    <td align="center"><img src="docs/images/screenshots/02-home-lyra-carousel.png" width="220" alt="Home, Lyra Carousel theme"><br><sub>Home — Lyra Carousel theme</sub></td>
    <td align="center"><img src="docs/images/screenshots/03-reading.png" width="220" alt="Reading page"><br><sub>Reading page</sub></td>
  </tr>
  <tr>
    <td align="center"><img src="docs/images/screenshots/04-calendar.png" width="220" alt="Calendar"><br><sub>Calendar agenda from an iCal feed</sub></td>
    <td align="center"><img src="docs/images/screenshots/05-sleep-reading-dashboard.png" width="220" alt="Reading Dashboard sleep screen"><br><sub>Sleep screen — Reading Dashboard</sub></td>
    <td align="center"><img src="docs/images/screenshots/06-cloud-library.png" width="220" alt="Cloud Library"><br><sub>Cloud Library over WebDAV</sub></td>
  </tr>
</table>

## Building from source

Requirements: PlatformIO, Python 3.8+, SDL2 for the simulator (`brew install sdl2`).

```bash
git clone --recursive https://github.com/filippocobelli/Echo
cd Echo

# Simulator
pio run -e simulator
.pio/build/simulator/program

# Device
pio run -e echo_x3   # Xteink X3
pio run -e echo_x4   # Xteink X4
```

Firmware lands in `.pio/build/<env>/firmware.bin`. Local PlatformIO overrides belong in
`platformio.local.ini`, which is not tracked.

## Credits

| Project | Author | What we used |
|---------|--------|-------------|
| [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader) | crosspoint-reader | Base firmware, EPUB engine, UI framework, HAL |
| [cpr-vcodex](https://github.com/franssjz/cpr-vcodex) | franssjz | Direct fork base — analytics, achievements, stats |
| [CrossInk](https://github.com/uxjulia/CrossInk) | uxjulia | Typography, fonts, build variants, simulator |

Fonts: ChareInk (M. Ramsey, freeware), Bitter (Sol Matas, OFL), Lexend Deca
(Bonnie Shaver-Troup, OFL), Inter (Rasmus Andersson, OFL).

## License

MIT — see [LICENSE](LICENSE).

# FORK_NOTICE — ECHO Firmware

ECHO is a fork. This file documents all upstream sources used.

---

## Direct fork chain

```
CrossPoint Reader  (crosspoint-reader/crosspoint-reader)
       ↓
  cpr-vcodex       (franssjz/cpr-vcodex)
       ↓
    ECHO           (this repo)
```

ECHO is based on cpr-vcodex at the state of its `master` branch.
Typography, build system, and simulator integration come from CrossInk (`uxjulia/CrossInk`).

---

## Upstream sources

### cpr-vcodex (franssjz/cpr-vcodex)
- **License**: MIT
- **What**: Base firmware — all core reading, EPUB parsing, UI framework, settings, WiFi, OTA
- **Taken as**: Direct git fork

### CrossPoint Reader (crosspoint-reader/crosspoint-reader)
- **License**: MIT
- **What**: Original firmware that cpr-vcodex is itself forked from
- **Taken as**: Indirect (via cpr-vcodex)

### CrossInk (uxjulia/CrossInk)
- **License**: MIT
- **What**: Reader fonts (ChareInk, Lexend Deca, Bitter), UI font (Inter), font pipeline scripts, pre-commit hook, build variants (tiny/xlarge/no_emoji), simulator integration pattern
- **Taken as**: Manual port into ECHO build system

### crosspoint-simulator (uxjulia/crosspoint-simulator)
- **License**: MIT
- **What**: SDL2 simulator for macOS development
- **Taken as**: PlatformIO `lib_deps` of the `simulator` env, pinned to a commit in `platformio.ini`

---

## Fonts

| Font | Author | License | Used for |
|------|--------|---------|---------|
| ChareInk | M. Ramsey | Freeware | Primary reader font |
| Bitter | Sol Matas | OFL | Alternative reader font |
| Lexend Deca | Bonnie Shaver-Troup | OFL | Sans-serif reader font |
| Inter | Rasmus Andersson | OFL | UI labels and small text |

---

## ECHO-exclusive code

The following files were written from scratch for ECHO and are not derived from any upstream:

- `src/activities/apps/IfFoundActivity.h` / `.cpp` — "If Found" app
- `src/activities/apps/CalendarActivity.h` / `.cpp` — iCal calendar app
- `src/activities/settings/CalendarSettingsActivity.h` / `.cpp` — Calendar settings screen
- Boot and sleep screen ECHO logo (in `BootActivity.cpp`, `SleepActivity.cpp`)

These files are licensed under MIT, like the rest of the repository.

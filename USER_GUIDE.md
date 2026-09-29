# ECHO User Guide

Welcome to the **ECHO** firmware. This guide outlines the hardware controls, navigation, and reading features of the device.

- [ECHO User Guide](#echo-user-guide)
  - [1. Hardware Overview](#1-hardware-overview)
    - [Button Layout](#button-layout)
  - [2. Power & Startup](#2-power--startup)
    - [Power On / Off](#power-on--off)
    - [First Launch](#first-launch)
  - [3. Screens](#3-screens)
    - [3.1 Home Screen](#31-home-screen)
    - [3.2 Reading Mode](#32-reading-mode)
    - [3.3 Browse Files Screen](#33-browse-files-screen)
    - [3.4 Recent Books Screen](#34-recent-books-screen)
    - [3.5 File Transfer Screen](#35-file-transfer-screen)
    - [3.6 Settings](#36-settings)
      - [3.6.1 Display](#361-display)
      - [3.6.2 Reader](#362-reader)
      - [3.6.3 Controls](#363-controls)
      - [3.6.4 System](#364-system)
      - [3.6.5 OPDS Servers (Multiple Libraries)](#365-opds-servers-multiple-libraries)
      - [3.6.6 Cloud Library (WebDAV)](#366-cloud-library-webdav)
      - [3.6.7 Web Settings (WiFi + OPDS)](#367-web-settings-wifi--opds)
      - [3.6.8 KOReader Sync Quick Setup](#368-koreader-sync-quick-setup)
    - [3.7 Sleep Screen](#37-sleep-screen)
    - [3.8 Calendar](#38-calendar)
    - [3.9 If Found, Please Return Me](#39-if-found-please-return-me)
    - [3.10 Dictionary](#310-dictionary)
  - [4. Reading Mode](#4-reading-mode)
    - [Page Turning](#page-turning)
    - [Chapter Navigation](#chapter-navigation)
    - [System Navigation](#system-navigation)
    - [Supported Languages](#supported-languages)
  - [5. Chapter Selection Screen](#5-chapter-selection-screen)
  - [6. Current Limitations & Roadmap](#6-current-limitations--roadmap)
  - [7. Troubleshooting Issues & Escaping Bootloop](#7-troubleshooting-issues--escaping-bootloop)


## 1. Hardware Overview

The device uses the standard buttons on Xteink X3 and X4 (in the same layout as the manufacturer firmware, by default):

### Button Layout
| Location        | Buttons                                              |
| --------------- | ---------------------------------------------------- |
| **Bottom Edge** | **Back**, **Confirm**, **Left**, **Right**           |
| **Right Side**  | **Power**, **Volume Up**, **Volume Down**, **Reset** |

Button layout can be customized in the **[Controls Settings](#363-controls)**.

### Taking a Screenshot
When the Power Button and Volume Down button are pressed at the same time, it will take a screenshot and save it in the folder `screenshots/`.

Alternatively, while reading a book, press the **Confirm** button to open the reader menu and select **Take screenshot**.

---

## 2. Power & Startup

### Power On / Off

To turn the device on or off, **press and hold the Power button for approximately half a second**.
In the **[Controls Settings](#363-controls)** you can configure the power button to turn the device off with a short press instead of a long one.

To reboot the device (for example after a firmware update or if it's frozen), press and release the Reset button, and then quickly press and hold the Power button for a few seconds.

### First Launch

Upon turning the device on for the first time, you will be placed on the **[Home](#31-home-screen)** screen.

> [!NOTE]
> On subsequent restarts, the firmware will automatically reopen the last book you were reading.

---

## 3. Screens

### 3.1 Home Screen

The Home screen is the main entry point. From here you can navigate to **[Reading Mode](#4-reading-mode)** with the most recently read book, **[Browse Files](#33-browse-files-screen)**, **[Recent Books](#34-recent-books-screen)**, **[File Transfer](#35-file-transfer-screen)**, **[Calendar](#38-calendar)**, **[If Found](#39-if-found-please-return-me)**, and **[Settings](#36-settings)**.

### 3.2 Reading Mode

See [Reading Mode](#4-reading-mode) below for more information.

### 3.3 Browse Files Screen

The Browse Files screen acts as a file and folder browser.

* **Navigate List:** Use **Left** (or **Volume Up**), or **Right** (or **Volume Down**) to move the selection cursor up and down through folders and books. You can also long-press these buttons to scroll a full page up or down.
* **Open Selection:** Press **Confirm** to open a folder or read a selected book.
* **Delete Files:** Hold and release **Confirm** to delete the selected file. You will be given an option to either confirm or cancel deletion. Folder deletion is not supported.

### 3.4 Recent Books Screen

The Recent Books screen lists the most recently opened books in a chronological view, displaying title and author.

### 3.5 File Transfer Screen

The File Transfer screen allows you to upload new e-books to the device. When you enter the screen, you'll be prompted with a WiFi selection dialog and then your device will start hosting a web server.

See the [webserver docs](./docs/webserver.md) for more information on how to connect to the web server and upload files.

> [!TIP]
> Advanced users can also manage files programmatically or via the command line using `curl`. See the [webserver docs](./docs/webserver.md) for details.

### 3.6 Settings

The Settings screen allows you to configure the device's behavior.

#### 3.6.1 Display

- **Sleep Screen**: Which sleep screen to display when the device sleeps:
  - "Dark" (default) - The ECHO logo on a dark background
  - "Light" - The ECHO logo on a white background
  - "Custom" - Custom images from the SD card; see [Sleep Screen](#37-sleep-screen) below
  - "Cover" - The book cover image (experimental)
  - "None" - A blank screen
  - "Cover + Custom" - Book cover, falls back to "Custom"
- **Sleep Screen Cover Mode**: How to display the book cover when "Cover" sleep screen is selected:
  - "Fit" (default) - Scale to fit centered, white borders
  - "Crop" - Scale and crop to fill screen (experimental)
- **Sleep Screen Cover Filter**: Filter applied to the book cover:
  - "None" (default) - Grayscale
  - "Contrast" - Black & white
  - "Inverted" - Inverted black & white
- **Status Bar**: Configure the status bar displayed while reading:
  - "None" - No status bar
  - "No Progress" - Status bar without reading progress
  - "Full w/ Percentage" - Book progress as percentage
  - "Full w/ Book Bar" - Book progress as bar
  - "Book Bar Only" - Book progress bar only
  - "Full w/ Chapter Bar" - Chapter progress as bar
- **Hide Battery %**: Configure where to suppress battery percentage display
- **Refresh Frequency**: Set how often the screen does a full refresh while reading (every 1, 5, 10, 15, or 30 pages)
- **UI Theme**: "Classic", "Lyra", or "Lyra Extended"
- **Sunlight Fading Fix**: Software fix for white X4 models in direct sunlight (ON/OFF)

#### 3.6.2 Reader

- **Reader Font Family**: ChareInk (default), Lexend Deca, Bitter
- **Reader Font Size**: Teensy, Tiny, Small, Medium (default), Large
- **Reader Line Spacing**: Tight, Normal (default), Wide
- **Reader Screen Margin**: 5–40 pixels in 5-pixel increments
- **Reader Paragraph Alignment**: Justified (default), Left, Center, Right
- **Embedded Style**: Use EPUB file's HTML/CSS formatting (ON/OFF)
- **Hyphenation**: ON/OFF
- **Reading Orientation**: Portrait (default), Landscape CW, Inverted, Landscape CCW
- **Extra Paragraph Spacing**: Vertical space between paragraphs (ON/OFF)
- **Text Anti-Aliasing**: Smooth grey edges on text (slows page turns slightly)
- **Reading Stats**: Toggle visibility of Reading Stats screens (ON/OFF)

#### 3.6.3 Controls

- **Remap Front Buttons**: Customise function of each bottom edge button
- **Side Button Layout (reader)**: Swap up/down volume buttons (Prev/Next ↔ Next/Prev)
- **Long-press Chapter Skip**: Chapter Skip (default) or Page Scroll
- **Short Power Button Action**: Ignore (default), Sleep, or Page Turn

#### 3.6.4 System

- **Reading Stats**: Show/hide reading stats screens
- **Calendar**: Configure iCal URL for calendar sync
- **WiFi Networks**: Connect to WiFi networks
- **KOReader Sync**: Set up KOReader progress sync
- **OPDS Servers**: Manage OPDS library catalog servers
- **Cloud Library**: Connect a WebDAV cloud drive (Koofr, Nextcloud, ownCloud) to browse and download books
- **Clear Reading Cache**: Clear the SD card cache
- **Check for updates**: Check for ECHO firmware updates over WiFi
- **Language**: Set the system language

#### 3.6.5 OPDS Servers (Multiple Libraries)

ECHO supports saving multiple OPDS servers and switching between them when browsing catalogs.

1. Open **Settings → System → OPDS Servers**.
2. Select **Add Server** to create a new entry, or select an existing server to edit it.
3. Configure these fields:
  - **Server Name**: Optional display name (e.g., "Home Calibre" or "Public Catalog").
  - **OPDS Server URL**: Full catalog root URL (for Calibre Content Server, usually ends with `/opds`).
  - **Username / Password**: Optional credentials for authenticated servers.
4. Use **Delete Server** inside a server entry to remove it.

Behavior notes:

- You can store up to 8 OPDS servers.
- OPDS authentication supports HTTP Basic auth.

You can also manage OPDS servers from the web interface while in File Transfer mode:

1. Connect to the device web UI.
2. Open `http://<device-ip>/settings`.
3. Use the **OPDS Servers** card to add, edit, or delete entries.

#### 3.6.6 Cloud Library (WebDAV)

The Cloud Library browses a remote WebDAV drive the same way the file browser
browses the SD card. Folders open, **Back** goes up, and selecting a book
downloads it to the SD card root and opens it straight away.

1. Open **Settings → System → Cloud Library**.
2. Configure these fields:
  - **WebDAV URL**: The service's DAV endpoint. For Koofr this is `https://app.koofr.net/dav/Koofr`.
  - **Username**: Your account name (usually the account e-mail).
  - **Password**: Your password, or an app-specific password where the provider requires one
    (Koofr calls these *app passwords* and requires one when two-factor authentication is on).
  - **Root Folder**: Folder to start browsing from, relative to the WebDAV URL. Defaults to `/`.
3. A **Cloud Library** entry appears on the home screen once a URL is set.

Behavior notes:

- Only `.epub`, `.txt` and `.xtc` files are listed; everything else on the drive is hidden.
- Authentication is HTTP Basic over HTTPS. Permanent redirects are followed.
- A folder listing is capped at 64 entries; when a folder holds more, the path line shows `64+`.
- The same fields appear in the web settings page under the **Cloud Library** card. The stored
  password is never sent back to the browser: leave the field blank to keep it unchanged.

#### 3.6.7 Web Settings (WiFi + OPDS)

While in **File Transfer** mode, the web settings page includes management cards for both **WiFi Networks** and **OPDS Servers**.

1. On device: open **File Transfer** and connect to WiFi.
1. In a browser, open `http://<device-ip>/settings` or `http://crosspoint.local`.
1. In **WiFi Networks**, add, edit, or delete saved network entries.
1. In **OPDS Servers**, add, edit, or delete OPDS catalogs.

#### 3.6.8 KOReader Sync Quick Setup

ECHO can sync reading progress with KOReader-compatible sync servers.

##### Option A: Free Public Server (`sync.koreader.rocks`)

1. Register a user once:

```bash
USERNAME="user"
PASSWORD="pass"
PASSWORD_MD5="$(printf '%s' "$PASSWORD" | openssl md5 | awk '{print $2}')"

curl -i "https://sync.koreader.rocks/users/create" \
  -H "Accept: application/vnd.koreader.v1+json" \
  -H "Content-Type: application/json" \
  --data "{\"username\":\"$USERNAME\",\"password\":\"$PASSWORD_MD5\"}"
```

2. On the device:
   - Go to **Settings → System → KOReader Sync**.
   - Set **Username** and **Password**.
   - Set **Sync Server URL** to `https://sync.koreader.rocks` (or leave empty).
   - Run **Authenticate**.

3. While reading, press **Confirm** → **Sync Progress** → Apply Remote or Upload Local.

##### Option B: Self-Hosted Server (Docker Compose)

1. Start a sync server:

```bash
mkdir -p kosync-quickstart && cd kosync-quickstart

cat > compose.yaml <<'YAML'
services:
  kosync:
    image: koreader/kosync:latest
    ports:
      - "7200:7200"
      - "17200:17200"
    volumes:
      - ./data/redis:/var/lib/redis
    environment:
      - ENABLE_USER_REGISTRATION=true
    restart: unless-stopped
YAML

docker compose up -d
```

2. On the device:
   - Go to **Settings → System → KOReader Sync**.
   - Set **Sync Server URL** to `http://<server-ip>:17200`.
   - Run **Authenticate**.

### 3.7 Sleep Screen

The **Sleep Screen** setting controls what is displayed when the device goes to sleep:

| Mode | Behavior |
|------|----------|
| **Dark** (default) | The ECHO logo on a dark background. |
| **Light** | The ECHO logo on a white background. |
| **Custom** | A custom image from the SD card. Falls back to **Dark** if not found. |
| **Cover** | The cover of the currently open book. Falls back to **Dark**. |
| **Cover + Custom** | Book cover, falls back to **Custom** behavior. |
| **None** | A blank screen. |

#### Custom images

Set sleep screen mode to **Custom**, then place images on the SD card:

- **Multiple Images (recommended):** Create a `.sleep` directory at the SD root and place `.bmp` images inside. One is randomly selected each sleep.
- **Single Image:** Place `sleep.bmp` at the SD root as a fallback.

> [!TIP]
> Use uncompressed BMP files, 24-bit color depth.
> X4: 480×800 pixels. X3: 528×792 pixels.

---

### 3.8 Calendar

ECHO can display upcoming calendar events synced from any iCal (.ics) URL (Google Calendar, Nextcloud, Fastmail, etc.).

**Setup:**
1. Go to **Settings → System → Calendar**.
2. Enter your iCal URL.

**Using:**
1. From the Home screen, select **Calendar**.
2. Press **Confirm** to sync (connects to WiFi if needed).
3. Use **Up/Down** to scroll through events.

Events are cached locally at `/.crosspoint/calendar.ics`. Up to 50 upcoming events are shown, sorted by date.

---

### 3.9 If Found, Please Return Me

If your device is lost, the "If Found" screen displays your contact information to whoever finds it.

**Setup:**
1. Create a file named `if_found.txt` at the root of your SD card.
2. Write your contact message (name, email, phone, reward offer — whatever you like).

**Using:**
1. From the Home screen, select **If Found**.
2. Use **Up/Down** to scroll through the text.

If `if_found.txt` is not present, a default message is shown with instructions to create the file.

---

### 3.10 Dictionary

ECHO can look up word definitions offline from StarDict-format dictionaries on the SD card.

**Setup:**
1. Copy a StarDict dictionary onto the SD card as `/dictionaries/<name>/<name>.ifo` + `.idx` + `.dict` (and optionally `.syn`).
2. Only **plain, uncompressed `.dict` files are supported** — if your dictionary was downloaded as `.dict.dz`, decompress it first (e.g. `dictzip -d name.dict.dz`) before copying it over.
3. From the Home screen, select **Dictionary**, then pick a dictionary from the list. The first time you select a dictionary, ECHO builds a small on-device search index (`.cpridx`, cached under `/.crosspoint/`) — this only happens once per dictionary.

**Using:**
1. Inside a book, open the reader menu and select **Search word**.
2. Press and release **Confirm** to look up the highlighted word; hold **Confirm** to open the keyboard and search for a different word.
3. Recent lookups are kept as history so you can revisit them without retyping.
4. The active dictionary and the definition text size (Small/Medium/Large) can be changed under **Settings → System → Dictionary**.

---

## 4. Reading Mode

Once you have opened a book, the button layout changes to facilitate reading.

### Page Turning
| Action            | Buttons                              |
| ----------------- | ------------------------------------ |
| **Previous Page** | Press **Left** _or_ **Volume Up**    |
| **Next Page**     | Press **Right** _or_ **Volume Down** |

The role of the volume (side) buttons can be swapped in the **[Controls Settings](#363-controls)**.

If the **Short Power Button Action** setting is set to "Page Turn", you can also turn to the next page by briefly pressing the Power button.

### Chapter Navigation
* **Next Chapter:** Press and **hold** the **Right** (or **Volume Down**) button briefly, then release.
* **Previous Chapter:** Press and **hold** the **Left** (or **Volume Up**) button briefly, then release.

This feature can be disabled in the **[Controls Settings](#363-controls)** to help avoid changing chapters by mistake.

### System Navigation
* **Return to Home:** Press the **Back** button to close the book and return to the **[Home](#31-home-screen)** screen.
* **Return to Browse Files:** Press and hold the **Back** button to close the book and return to the **[Browse Files](#33-browse-files-screen)** screen.
* **Chapter Menu:** Press **Confirm** to open the **[Table of Contents/Chapter Selection](#5-chapter-selection-screen)** screen.

### Supported Languages

ECHO renders text using the following Unicode character blocks:

*   **Latin Script (Basic, Supplement, Extended-A):** English, German, French, Spanish, Portuguese, Italian, Dutch, Swedish, Norwegian, Danish, Finnish, Polish, Czech, Hungarian, Romanian, Slovak, Slovenian, Turkish, and others.
*   **Cyrillic Script (Standard and Extended):** Russian, Ukrainian, Belarusian, Bulgarian, Serbian, Macedonian, Kazakh, Kyrgyz, Mongolian, and others.

Not supported: Chinese, Japanese, Korean, Vietnamese, Hebrew, Arabic, Greek and Farsi.

---

## 5. Chapter Selection Screen

Accessible by pressing **Confirm** while inside a book.

1.  Use **Left** (or **Volume Up**), or **Right** (or **Volume Down**) to highlight the desired chapter.
2.  Press **Confirm** to jump to that chapter.
3.  Press **Back** to cancel and return to your current page.

---

## 6. Current Limitations & Roadmap

Please note that this firmware is currently in active development. The following features are **not yet supported** but are planned for future updates:

* **Images:** Embedded images in e-books will not render.
* **Cover Images:** Large cover images embedded into EPUB require several seconds (~10s for ~2000 pixel tall image) to convert for sleep screen and home screen thumbnail. Consider optimizing the EPUB with e.g. https://github.com/bigbag/epub-to-xtc-converter to speed this up.

---

## 7. Troubleshooting Issues & Escaping Bootloop

If an issue or crash is encountered, attach the serial monitor logs when reporting. Obtain them by connecting the device to a computer:

```
pio device monitor
```

If the device is stuck in a bootloop, press and release the Reset button. Then, press and hold the Back button and the Power button to boot to the Home Screen.

For broken cache or config, delete the `.crosspoint` directory on your SD card (or just `settings.bin`, `state.bin`, or specific `epub_*` cache directories).

#include "CalendarActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <cstring>
#include <new>
#include <string>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "activities/network/WifiSelectionActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

#ifndef SIMULATOR
#include <WiFi.h>

#include "esp_http_client.h"

// Declared by hand, as in OtaUpdater.cpp: including esp_crt_bundle.h resolves to the
// Arduino WiFiClientSecure copy of that header instead of the ESP-IDF one.
extern "C" {
extern esp_err_t esp_crt_bundle_attach(void* conf);
}
#endif

static constexpr int kTextFont = UI_10_FONT_ID;
static constexpr int kLineSpacing = 4;
static constexpr int kDownloadBufSize = 32768;

// ---- iCal parser ----

void CalendarActivity::parseIcs(const char* ics, size_t len) {
  events.clear();
  CalendarEvent cur;
  bool inEvent = false;

  const char* p = ics;
  const char* end = ics + len;

  while (p < end) {
    const char* nl = static_cast<const char*>(memchr(p, '\n', end - p));
    // Line runs from p up to (but not including) the newline, or to end if there is none.
    const char* lineEnd = nl ? nl : end;
    // Strip a trailing '\r'. Guarding on lineEnd != p (not on the length) keeps the
    // before-the-buffer read out of reach on an empty line — where nl == p — and, because
    // memchr can legitimately return p, cppcheck no longer reads the guard as always true.
    if (lineEnd != p && lineEnd[-1] == '\r') --lineEnd;

    std::string line(p, static_cast<size_t>(lineEnd - p));

    if (line == "BEGIN:VEVENT") {
      inEvent = true;
      cur = {};
    } else if (line == "END:VEVENT") {
      if (inEvent && !cur.summary.empty() && !cur.date.empty()) {
        events.push_back(cur);
      }
      inEvent = false;
    } else if (inEvent) {
      if (line.rfind("DTSTART", 0) == 0) {
        const size_t colon = line.find(':');
        if (colon != std::string::npos) {
          cur.date = line.substr(colon + 1);
          // Take only date portion "YYYYMMDD"
          if (cur.date.size() >= 8) cur.date = cur.date.substr(0, 8);
        }
      } else if (line.rfind("SUMMARY:", 0) == 0) {
        cur.summary = line.substr(8);
      }
    }

    if (!nl) break;
    p = nl + 1;  // advance past the newline (independent of any stripped '\r')
  }

  // Sort events by date
  std::sort(events.begin(), events.end(),
            [](const CalendarEvent& a, const CalendarEvent& b) { return a.date < b.date; });

  // Keep only first 50 events to limit memory usage
  if (events.size() > 50) events.resize(50);
}

// ---- Sync (ESP32 only) ----

void CalendarActivity::syncCalendar() {
#ifndef SIMULATOR
  if (SETTINGS.calendarUrl[0] == '\0') return;
  if (WiFi.status() != WL_CONNECTED) return;

  syncing = true;
  requestUpdate(true);

  // nothrow: with WiFi up a 32 KB contiguous block can be unavailable, and a throwing
  // new would abort() instead of skipping the sync.
  char* buf = new (std::nothrow) char[kDownloadBufSize];
  if (!buf) {
    LOG_ERR("CAL", "Failed to allocate %d byte download buffer", kDownloadBufSize);
    syncing = false;
    requestUpdate(true);
    return;
  }
  // esp_http_client_read() only fills the bytes it actually received, and the buffer is
  // handed to parseIcs() further down, so leaving the tail uninitialized is what cppcheck
  // flags as [uninitdata]. An explicit memset, because cppcheck does not treat a
  // value-initialized nothrow new[] as initialized. Runs once per manual sync.
  memset(buf, 0, kDownloadBufSize);

  esp_http_client_config_t cfg{};
  cfg.url = SETTINGS.calendarUrl;
  cfg.method = HTTP_METHOD_GET;
  cfg.timeout_ms = 10000;
  // Without a verification option esp-tls refuses every https:// URL, which is what
  // Google and iCloud publish. Same CA bundle the OTA updater already uses.
  cfg.crt_bundle_attach = esp_crt_bundle_attach;

  esp_http_client_handle_t client = esp_http_client_init(&cfg);
  if (client) {
    if (esp_http_client_open(client, 0) == ESP_OK) {
      int64_t contentLen = esp_http_client_fetch_headers(client);
      const int status = esp_http_client_get_status_code(client);
      if (status != 200) {
        // An error page must not overwrite the last good calendar cache.
        LOG_ERR("CAL", "Sync failed: HTTP %d", status);
      } else {
        if (contentLen <= 0) contentLen = kDownloadBufSize - 1;
        const int toRead = static_cast<int>(std::min((int64_t)(kDownloadBufSize - 1), contentLen));
        const int received = esp_http_client_read(client, buf, toRead);
        if (received > 0) {
          buf[received] = '\0';
          FsFile cacheFile;
          if (Storage.openFileForWrite("CAL", kCachePath, cacheFile)) {
            cacheFile.write(buf, static_cast<size_t>(received));
            cacheFile.close();
          }
          parseIcs(buf, static_cast<size_t>(received));
          scrollOffset = 0;
        } else {
          LOG_ERR("CAL", "Sync failed: read returned %d", received);
        }
      }
    } else {
      LOG_ERR("CAL", "Sync failed: could not open connection");
    }
    esp_http_client_cleanup(client);
  }

  delete[] buf;
  syncing = false;
  requestUpdate(true);
#endif
}

// ---- Activity lifecycle ----

void CalendarActivity::onEnter() {
  Activity::onEnter();
  scrollOffset = 0;
  loadCachedEvents();
  requestUpdate();
}

void CalendarActivity::onExit() { Activity::onExit(); }

void CalendarActivity::loadCachedEvents() {
  events.clear();
  String raw = Storage.readFile(kCachePath);
  if (raw.length() > 0) {
    parseIcs(raw.c_str(), raw.length());
  }
}

void CalendarActivity::loop() {
  if (syncing) return;

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  // Confirm = sync calendar
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (SETTINGS.calendarUrl[0] != '\0') {
#ifndef SIMULATOR
      if (WiFi.status() == WL_CONNECTED) {
        syncCalendar();
      } else {
        startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput, false),
                               [this](const ActivityResult& result) {
                                 if (!result.isCancelled) syncCalendar();
                               });
      }
#endif
    }
    return;
  }

  buttonNavigator.onNext([this] {
    if (scrollOffset + visibleRows < static_cast<int>(events.size())) {
      scrollOffset++;
      requestUpdate();
    }
  });
  buttonNavigator.onPrevious([this] {
    if (scrollOffset > 0) {
      scrollOffset--;
      requestUpdate();
    }
  });
}

void CalendarActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_CALENDAR));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentBottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const int lineHeight = renderer.getTextHeight(kTextFont) + kLineSpacing;

  visibleRows = std::max(1, (contentBottom - contentTop) / lineHeight);

  if (syncing) {
    renderer.drawCenteredText(kTextFont, contentTop + lineHeight, tr(STR_SYNCING_TIME));
  } else if (SETTINGS.calendarUrl[0] == '\0') {
    renderer.drawCenteredText(kTextFont, contentTop + lineHeight, tr(STR_CALENDAR_NO_URL));
  } else if (events.empty()) {
    renderer.drawCenteredText(kTextFont, contentTop + lineHeight, tr(STR_CALENDAR_NO_EVENTS));
  } else {
    const int margin = 20;
    int y = contentTop;
    for (int i = scrollOffset; i < static_cast<int>(events.size()) && i < scrollOffset + visibleRows; ++i) {
      // Format: "YYYY-MM-DD  Summary"
      std::string dateDisp;
      const std::string& d = events[i].date;
      if (d.size() >= 8) {
        dateDisp = d.substr(0, 4) + "-" + d.substr(4, 2) + "-" + d.substr(6, 2) + "  ";
      }
      const std::string row = dateDisp + events[i].summary;
      renderer.drawText(kTextFont, margin, y, row.c_str());
      y += lineHeight;
    }
  }

  const char* syncHint = (SETTINGS.calendarUrl[0] != '\0') ? tr(STR_CALENDAR_SYNC) : "";
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), syncHint, tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

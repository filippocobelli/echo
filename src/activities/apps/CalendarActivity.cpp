#include "CalendarActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
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
static constexpr int kSmallFont = SMALL_FONT_ID;
static constexpr size_t kMaxEvents = 100;
static constexpr int kKeepDaysBeforeToday = 31;  // older events are dropped while parsing
static constexpr int kDownloadBufSize = 32768;


namespace {

constexpr StrId kMonthNames[12] = {StrId::STR_CAL_MONTH_1,  StrId::STR_CAL_MONTH_2,  StrId::STR_CAL_MONTH_3,
                                   StrId::STR_CAL_MONTH_4,  StrId::STR_CAL_MONTH_5,  StrId::STR_CAL_MONTH_6,
                                   StrId::STR_CAL_MONTH_7,  StrId::STR_CAL_MONTH_8,  StrId::STR_CAL_MONTH_9,
                                   StrId::STR_CAL_MONTH_10, StrId::STR_CAL_MONTH_11, StrId::STR_CAL_MONTH_12};
constexpr StrId kWeekdayInitials[7] = {StrId::STR_CAL_WEEKDAY_1, StrId::STR_CAL_WEEKDAY_2, StrId::STR_CAL_WEEKDAY_3,
                                       StrId::STR_CAL_WEEKDAY_4, StrId::STR_CAL_WEEKDAY_5, StrId::STR_CAL_WEEKDAY_6,
                                       StrId::STR_CAL_WEEKDAY_7};

// Days since 1970-01-01 for a proleptic Gregorian date (Howard Hinnant's algorithm).
int64_t daysFromCivil(int y, const int m, const int d) {
  y -= m <= 2;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const int yoe = static_cast<int>(y - era * 400);
  const int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

// Inverse of daysFromCivil().
void civilFromDays(const int64_t days, int& y, int& m, int& d) {
  const int64_t z = days + 719468;
  const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const int doe = static_cast<int>(z - era * 146097);
  const int yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const int doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const int mp = (5 * doy + 2) / 153;
  d = doy - (153 * mp + 2) / 5 + 1;
  m = mp < 10 ? mp + 3 : mp - 9;
  y = static_cast<int>(yoe + era * 400) + (m <= 2);
}

int daysInMonth(const int year, const int month) {
  static constexpr int kDays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
  return (month == 2 && leap) ? 29 : kDays[month - 1];
}

// 0 = Monday ... 6 = Sunday. 1970-01-01 was a Thursday.
int weekdayMondayFirst(const int year, const int month, const int day) {
  const int64_t days = daysFromCivil(year, month, day);
  return static_cast<int>(((days % 7) + 7 + 3) % 7);
}

int readDigits(const char* s, const int count) {
  int v = 0;
  for (int i = 0; i < count; ++i) {
    if (s[i] < '0' || s[i] > '9') return -1;
    v = v * 10 + (s[i] - '0');
  }
  return v;
}

// First `count` UTF-8 characters of `text` (never cuts a multi-byte character).
std::string utf8Prefix(const char* text, int count) {
  std::string out;
  for (const char* p = text; *p != '\0' && count > 0; --count) {
    do {
      out.push_back(*p++);
    } while ((*p & 0xC0) == 0x80);
  }
  return out;
}

}  // namespace

// ---- iCal parser ----

bool CalendarActivity::parseIcsDateTime(const char* value, CalendarEvent& out) {
  if (value == nullptr || strlen(value) < 8) return false;
  const int year = readDigits(value, 4);
  const int month = readDigits(value + 4, 2);
  const int day = readDigits(value + 6, 2);
  if (year < 1970 || month < 1 || month > 12 || day < 1 || day > daysInMonth(year, month)) return false;

  out.year = static_cast<uint16_t>(year);
  out.month = static_cast<uint8_t>(month);
  out.day = static_cast<uint8_t>(day);
  out.hour = 0;
  out.minute = 0;
  out.allDay = true;

  // "T" + HHMMSS, optionally followed by "Z" (UTC).
  if (value[8] == 'T' && strlen(value) >= 13) {
    const int hour = readDigits(value + 9, 2);
    const int minute = readDigits(value + 11, 2);
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59) return false;
    out.hour = static_cast<uint8_t>(hour);
    out.minute = static_cast<uint8_t>(minute);
    out.allDay = false;

    const size_t len = strlen(value);
    if (len >= 15 && value[len - 1] == 'Z') {
      // UTC timestamp: shift into the device's timezone (set through TZ by the time settings).
      const int64_t epoch = daysFromCivil(year, month, day) * 86400 + hour * 3600 + minute * 60;
      const time_t t = static_cast<time_t>(epoch);
      struct tm local {};
      if (localtime_r(&t, &local) != nullptr) {
        out.year = static_cast<uint16_t>(local.tm_year + 1900);
        out.month = static_cast<uint8_t>(local.tm_mon + 1);
        out.day = static_cast<uint8_t>(local.tm_mday);
        out.hour = static_cast<uint8_t>(local.tm_hour);
        out.minute = static_cast<uint8_t>(local.tm_min);
      }
    }
  }
  return true;
}

void CalendarActivity::parseIcs(const char* ics, size_t len) {
  events.clear();
  CalendarEvent cur;
  bool inEvent = false;
  bool haveDate = false;

  // Events far in the past would only crowd out the ones that can still be shown.
  readClock();
  uint32_t oldestKey = 0;
  if (haveToday) {
    int y = 0;
    int m = 0;
    int d = 0;
    civilFromDays(daysFromCivil(todayYear, todayMonth, todayDay) - kKeepDaysBeforeToday, y, m, d);
    oldestKey = static_cast<uint32_t>(y) * 10000u + m * 100u + d;
  }

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
      haveDate = false;
      cur = {};
    } else if (line == "END:VEVENT") {
      if (inEvent && !cur.summary.empty() && haveDate && cur.dateKey() >= oldestKey) {
        events.push_back(cur);
      }
      inEvent = false;
    } else if (inEvent) {
      if (line.rfind("DTSTART", 0) == 0) {
        const size_t colon = line.find(':');
        if (colon != std::string::npos) {
          std::string summary = std::move(cur.summary);
          haveDate = parseIcsDateTime(line.c_str() + colon + 1, cur);
          cur.summary = std::move(summary);
        }
      } else if (line.rfind("SUMMARY:", 0) == 0) {
        cur.summary = line.substr(8);
      }
    }

    if (!nl) break;
    p = nl + 1;  // advance past the newline (independent of any stripped '\r')
  }

  std::sort(events.begin(), events.end(), [](const CalendarEvent& a, const CalendarEvent& b) {
    if (a.dateKey() != b.dateKey()) return a.dateKey() < b.dateKey();
    return a.minuteOfDay() < b.minuteOfDay();
  });

  // Keep the list bounded to limit memory usage.
  if (events.size() > kMaxEvents) events.resize(kMaxEvents);
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

// ---- Clock and month navigation ----

void CalendarActivity::readClock() {
  const time_t now = time(nullptr);
  haveToday = false;
  if (now < 1700000000) return;  // clock not set yet
  struct tm local {};
  if (localtime_r(&now, &local) == nullptr) return;
  haveToday = true;
  todayYear = local.tm_year + 1900;
  todayMonth = local.tm_mon + 1;
  todayDay = local.tm_mday;
}

void CalendarActivity::showMonth(const int year, const int month) {
  viewYear = year;
  viewMonth = month;
  scrollOffset = 0;
}

void CalendarActivity::changeMonth(const int delta) {
  int month = viewMonth + delta;
  int year = viewYear;
  if (month < 1) {
    month = 12;
    --year;
  } else if (month > 12) {
    month = 1;
    ++year;
  }
  if (year < 1970 || year > 2100) return;
  showMonth(year, month);
  requestUpdate();
}

// The agenda lists events from today on when the current month is shown, and from the
// first day of the month otherwise.
uint32_t CalendarActivity::agendaStartKey() const {
  if (haveToday && viewYear == todayYear && viewMonth == todayMonth) {
    return static_cast<uint32_t>(todayYear) * 10000u + todayMonth * 100u + todayDay;
  }
  return static_cast<uint32_t>(viewYear) * 10000u + viewMonth * 100u + 1u;
}

size_t CalendarActivity::firstAgendaIndex() const {
  const uint32_t start = agendaStartKey();
  size_t i = 0;
  while (i < events.size() && events[i].dateKey() < start) ++i;
  return i;
}

// ---- Activity lifecycle ----

void CalendarActivity::onEnter() {
  Activity::onEnter();
  scrollOffset = 0;
  loadCachedEvents();
  readClock();
  if (haveToday) {
    showMonth(todayYear, todayMonth);
  } else if (!events.empty()) {
    showMonth(events.front().year, events.front().month);
  } else {
    showMonth(2026, 1);
  }
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

  // Left / Right (front buttons) change the month.
  if (mappedInput.wasPressed(MappedInputManager::Button::Left)) {
    changeMonth(-1);
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Right)) {
    changeMonth(1);
    return;
  }

  // Up / Down (side buttons) scroll the agenda.
  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Down}, [this] {
    if (scrollOffset + visibleRows < agendaCount) {
      scrollOffset++;
      requestUpdate();
    }
  });
  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Up}, [this] {
    if (scrollOffset > 0) {
      scrollOffset--;
      requestUpdate();
    }
  });
}

void CalendarActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  // Title: "September 2026"
  const std::string title = std::string(I18N.get(kMonthNames[viewMonth - 1])) + " " + std::to_string(viewYear);
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, title.c_str());

  const int margin = metrics.contentSidePadding;
  const int gridWidth = pageWidth - 2 * margin;
  const int cellWidth = gridWidth / 7;
  const int gridLeft = margin + (gridWidth - cellWidth * 7) / 2;
  const int smallLine = renderer.getLineHeight(kSmallFont);
  const int textLine = renderer.getLineHeight(kTextFont);

  // ---- Weekday header ----
  int y = metrics.topPadding + metrics.headerHeight;
  for (int col = 0; col < 7; ++col) {
    const char* label = I18N.get(kWeekdayInitials[col]);
    const int w = renderer.getTextWidth(kSmallFont, label);
    renderer.drawText(kSmallFont, gridLeft + col * cellWidth + (cellWidth - w) / 2, y, label);
  }
  y += smallLine + 2;
  renderer.drawLine(margin, y, pageWidth - margin, y);
  y += 3;

  // ---- Month grid ----
  const int firstWeekday = weekdayMondayFirst(viewYear, viewMonth, 1);
  const int monthDays = daysInMonth(viewYear, viewMonth);
  const int weeks = (firstWeekday + monthDays + 6) / 7;
  const int rowHeight = std::max(textLine + 18, 40);
  const int circle = std::min(cellWidth - 8, textLine + 8);

  uint32_t daysWithEvents = 0;  // bit d = at least one event on day d
  for (const auto& e : events) {
    if (e.year == viewYear && e.month == viewMonth) daysWithEvents |= (1u << e.day);
  }

  for (int day = 1; day <= monthDays; ++day) {
    const int slot = firstWeekday + day - 1;
    const int cellX = gridLeft + (slot % 7) * cellWidth;
    const int cellY = y + (slot / 7) * rowHeight;
    const std::string number = std::to_string(day);
    const int numberWidth = renderer.getTextWidth(kTextFont, number.c_str());
    const bool isToday = haveToday && viewYear == todayYear && viewMonth == todayMonth && day == todayDay;
    const int numberY = cellY + 2;

    if (isToday) {
      // Filled circle with the day number knocked out in white.
      const int circleX = cellX + (cellWidth - circle) / 2;
      const int circleY = numberY + (textLine - circle) / 2;
      renderer.fillRoundedRect(circleX, circleY, circle, circle, circle / 2, Color::Black);
      renderer.drawText(kTextFont, cellX + (cellWidth - numberWidth) / 2, numberY, number.c_str(), false);
    } else {
      renderer.drawText(kTextFont, cellX + (cellWidth - numberWidth) / 2, numberY, number.c_str());
    }

    if (daysWithEvents & (1u << day)) {
      constexpr int kDot = 5;
      const int dotY = numberY + (textLine + circle) / 2 + 3;
      renderer.fillRoundedRect(cellX + (cellWidth - kDot) / 2, dotY, kDot, kDot, kDot / 2, Color::Black);
    }
  }
  y += weeks * rowHeight + 4;

  // ---- Agenda ----
  renderer.drawLine(margin, y, pageWidth - margin, y);
  y += 4;

  const int agendaTop = y;
  const int agendaBottom = pageHeight - metrics.buttonHintsHeight - 6;
  const int agendaRowHeight = textLine + smallLine + 10;
  visibleRows = std::max(1, (agendaBottom - agendaTop) / agendaRowHeight);

  const size_t first = firstAgendaIndex();
  // Everything from the agenda start on, including the following months.
  agendaCount = static_cast<int>(events.size() - first);
  scrollOffset = std::min(scrollOffset, std::max(0, agendaCount - visibleRows));

  if (syncing) {
    renderer.drawCenteredText(kTextFont, agendaTop + agendaRowHeight, tr(STR_SYNCING_TIME));
  } else if (agendaCount == 0) {
    const char* message = (SETTINGS.calendarUrl[0] == '\0' && events.empty()) ? tr(STR_CALENDAR_NO_URL)
                                                                              : tr(STR_CALENDAR_NO_EVENTS);
    const std::string fitted = renderer.truncatedText(kTextFont, message, gridWidth);
    renderer.drawCenteredText(kTextFont, agendaTop + agendaRowHeight / 2, fitted.c_str());
  } else {
    const int blockWidth = 52;
    const int textLeft = margin + blockWidth + 8;
    const int textWidth = pageWidth - margin - textLeft;
    for (int row = 0; row < visibleRows && scrollOffset + row < agendaCount; ++row) {
      const CalendarEvent& e = events[first + scrollOffset + row];
      const int rowY = agendaTop + row * agendaRowHeight;

      // Date block: three-letter month above, day number below.
      const std::string monthAbbr = utf8Prefix(I18N.get(kMonthNames[e.month - 1]), 3);
      const int abbrWidth = renderer.getTextWidth(kSmallFont, monthAbbr.c_str());
      renderer.drawText(kSmallFont, margin + (blockWidth - abbrWidth) / 2, rowY + 2, monthAbbr.c_str());
      const std::string dayText = std::to_string(e.day);
      const int dayWidth = renderer.getTextWidth(kTextFont, dayText.c_str(), EpdFontFamily::BOLD);
      renderer.drawText(kTextFont, margin + (blockWidth - dayWidth) / 2, rowY + 2 + smallLine, dayText.c_str(), true,
                        EpdFontFamily::BOLD);

      // Title and time.
      const std::string titleText = renderer.truncatedText(kTextFont, e.summary.c_str(), textWidth);
      renderer.drawText(kTextFont, textLeft, rowY + 2, titleText.c_str());
      char timeText[24];
      if (e.allDay) {
        snprintf(timeText, sizeof(timeText), "%s", tr(STR_CAL_ALL_DAY));
      } else {
        snprintf(timeText, sizeof(timeText), "%02u:%02u", static_cast<unsigned>(e.hour), static_cast<unsigned>(e.minute));
      }
      renderer.drawText(kSmallFont, textLeft, rowY + 2 + textLine, timeText);
    }
  }

  const char* syncHint = (SETTINGS.calendarUrl[0] != '\0') ? tr(STR_CAL_SYNC_SHORT) : "";
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), syncHint, tr(STR_CAL_PREV), tr(STR_CAL_NEXT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

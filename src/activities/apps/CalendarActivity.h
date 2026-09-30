#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

// One calendar event, already converted to the device's local time.
struct CalendarEvent {
  uint16_t year = 0;
  uint8_t month = 0;  // 1-12
  uint8_t day = 0;    // 1-31
  uint8_t hour = 0;
  uint8_t minute = 0;
  bool allDay = true;  // DTSTART was a plain date, so there is no time of day
  std::string summary;

  uint32_t dateKey() const { return static_cast<uint32_t>(year) * 10000u + month * 100u + day; }
  uint32_t minuteOfDay() const { return hour * 60u + minute; }
};

/**
 * Month grid on top (Monday first, today filled, a dot under days with events)
 * and the agenda of upcoming events below it.
 *
 * Left / Right change the month, Up / Down scroll the agenda, Confirm syncs the
 * iCal feed and Back returns to the previous screen.
 */
class CalendarActivity final : public Activity {
 public:
  explicit CalendarActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Calendar", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

  // iCal "DTSTART" value ("20260930", "20260930T180000", "20260930T180000Z") -> local event fields.
  // Public and static so the host tests can exercise it.
  static bool parseIcsDateTime(const char* value, CalendarEvent& out);

 private:
  std::vector<CalendarEvent> events;  // sorted by date, then time
  int scrollOffset = 0;               // agenda scroll, in rows
  int visibleRows = 1;                // agenda rows that fit on screen (set by render)
  int agendaCount = 0;                // agenda rows available for the shown month
  bool syncing = false;

  int viewYear = 0;
  int viewMonth = 1;  // 1-12
  bool haveToday = false;
  int todayYear = 0;
  int todayMonth = 0;
  int todayDay = 0;

  ButtonNavigator buttonNavigator;

  void loadCachedEvents();
  void syncCalendar();
  void parseIcs(const char* ics, size_t len);
  void readClock();
  void showMonth(int year, int month);
  void changeMonth(int delta);
  uint32_t agendaStartKey() const;
  size_t firstAgendaIndex() const;

  static constexpr const char* kCachePath = "/.crosspoint/calendar.ics";
};

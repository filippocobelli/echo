#pragma once

#include <string>
#include <vector>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

struct CalendarEvent {
  std::string date;  // "YYYYMMDD" or "YYYYMMDDTHHMMSS"
  std::string summary;
};

class CalendarActivity final : public Activity {
 public:
  explicit CalendarActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Calendar", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  std::vector<CalendarEvent> events;
  int scrollOffset = 0;
  int visibleRows = 0;
  bool syncing = false;

  ButtonNavigator buttonNavigator;

  void loadCachedEvents();
  void syncCalendar();
  void parseIcs(const char* ics, size_t len);

  static constexpr const char* kCachePath = "/.crosspoint/calendar.ics";
};

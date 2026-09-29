#pragma once
#include <cstdint>
#include <vector>

// Day-granular reading log, persisted to /.crosspoint/daily_log.bin. Backs the
// daily reading goal, the current streak, and per-day time correction (book
// stats "Correct time" action), and is read by the Reading Dashboard sleep
// screen.
//
// Note: "today" is derived from the device clock (time(nullptr), same as the
// NTP sync path in HalClock). Accuracy therefore depends on the platform's RTC:
// reliable on X3 (dedicated DS3231), but can drift on X4 across deep sleep per
// the hardware constraints in AGENTS.md — this class does not attempt to fix
// that, it just uses whatever the system clock currently reports.
class DailyReadingLog {
 public:
  // Days since the Unix epoch (UTC calendar day of the current system clock).
  static uint32_t todayEpochDay();

  // Adds (positive) or removes (negative) seconds from the given day's total.
  // Saturates at zero — a day's total never goes negative. Persists.
  static void addSeconds(uint32_t dayEpoch, int32_t deltaSeconds);

  static uint32_t getSecondsForDay(uint32_t dayEpoch);
  static uint32_t getTodaySeconds() { return getSecondsForDay(todayEpochDay()); }

  // Consecutive days (ending today or yesterday, so a streak isn't lost until
  // a full day is missed) with seconds > 0.
  static uint32_t computeCurrentStreak();

  // Small bitmask of unlocked achievement milestones (see AchievementDefs.h),
  // persisted in this same file.
  static uint32_t getUnlockedAchievementsMask();
  static void setUnlockedAchievementsMask(uint32_t mask);

  // Removes every entry for `dayEpoch` if seconds would go to zero, otherwise
  // updates it. Exposed for AZIONE C (reset book stats): subtract a book's
  // contribution from whichever day(s) it's realistically attributable to. In
  // practice, since sessions aren't attributed per-book in the day log, this
  // is used to subtract from today's bucket only — see BookActions.
  static void removeIfEmpty(uint32_t dayEpoch);

 private:
  struct DayEntry {
    uint32_t dayEpoch;
    uint32_t seconds;
  };

  static bool loadAll(std::vector<DayEntry>& entries, uint32_t& achievementsMask);
  static bool saveAll(const std::vector<DayEntry>& entries, uint32_t achievementsMask);
};

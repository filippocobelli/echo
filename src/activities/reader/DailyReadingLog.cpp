#include "DailyReadingLog.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Serialization.h>

#include <algorithm>
#include <ctime>

namespace {
constexpr uint32_t DAILY_LOG_MAGIC = 0x444C4F47;  // "DLOG"-ish, arbitrary/distinct
constexpr uint8_t DAILY_LOG_VERSION = 1;
constexpr char DAILY_LOG_PATH[] = "/.crosspoint/daily_log.bin";
constexpr uint32_t SECONDS_PER_DAY = 86400;
}  // namespace

uint32_t DailyReadingLog::todayEpochDay() {
  const time_t now = time(nullptr);
  if (now <= 0) return 0;
  return static_cast<uint32_t>(now / SECONDS_PER_DAY);
}

bool DailyReadingLog::loadAll(std::vector<DayEntry>& entries, uint32_t& achievementsMask) {
  entries.clear();
  achievementsMask = 0;

  if (!Storage.exists(DAILY_LOG_PATH)) return true;

  FsFile file;
  if (!Storage.openFileForRead("DailyLog", DAILY_LOG_PATH, file)) return false;

  uint32_t magic = 0;
  uint8_t version = 0;
  uint32_t entryCount = 0;
  const bool headerOk = serialization::tryReadPod(file, magic) && serialization::tryReadPod(file, version) &&
                        serialization::tryReadPod(file, achievementsMask) &&
                        serialization::tryReadPod(file, entryCount) && magic == DAILY_LOG_MAGIC &&
                        version == DAILY_LOG_VERSION;
  if (!headerOk) {
    file.close();
    LOG_ERR("DailyLog", "Bad header, starting fresh");
    achievementsMask = 0;
    return false;
  }

  entries.reserve(entryCount);
  for (uint32_t i = 0; i < entryCount; ++i) {
    DayEntry entry{};
    if (!serialization::tryReadPod(file, entry.dayEpoch) || !serialization::tryReadPod(file, entry.seconds)) {
      break;
    }
    entries.push_back(entry);
  }
  file.close();
  return true;
}

bool DailyReadingLog::saveAll(const std::vector<DayEntry>& entries, const uint32_t achievementsMask) {
  Storage.mkdir("/.crosspoint");

  FsFile file;
  if (!Storage.openFileForWrite("DailyLog", DAILY_LOG_PATH, file)) {
    LOG_ERR("DailyLog", "Could not write daily_log.bin");
    return false;
  }

  serialization::writePod(file, DAILY_LOG_MAGIC);
  serialization::writePod(file, DAILY_LOG_VERSION);
  serialization::writePod(file, achievementsMask);
  serialization::writePod(file, static_cast<uint32_t>(entries.size()));
  for (const auto& entry : entries) {
    serialization::writePod(file, entry.dayEpoch);
    serialization::writePod(file, entry.seconds);
  }
  file.close();
  return true;
}

void DailyReadingLog::addSeconds(const uint32_t dayEpoch, const int32_t deltaSeconds) {
  std::vector<DayEntry> entries;
  uint32_t achievementsMask = 0;
  loadAll(entries, achievementsMask);

  auto it =
      std::find_if(entries.begin(), entries.end(), [dayEpoch](const DayEntry& e) { return e.dayEpoch == dayEpoch; });
  if (it == entries.end()) {
    if (deltaSeconds <= 0) return;  // nothing to subtract from a day with no entry
    entries.push_back(DayEntry{dayEpoch, static_cast<uint32_t>(deltaSeconds)});
  } else {
    if (deltaSeconds < 0 && static_cast<uint32_t>(-deltaSeconds) >= it->seconds) {
      it->seconds = 0;
    } else {
      it->seconds = static_cast<uint32_t>(static_cast<int64_t>(it->seconds) + deltaSeconds);
    }
  }

  std::sort(entries.begin(), entries.end(),
            [](const DayEntry& a, const DayEntry& b) { return a.dayEpoch < b.dayEpoch; });
  saveAll(entries, achievementsMask);
}

uint32_t DailyReadingLog::getSecondsForDay(const uint32_t dayEpoch) {
  std::vector<DayEntry> entries;
  uint32_t achievementsMask = 0;
  loadAll(entries, achievementsMask);

  const auto it = std::find_if(entries.begin(), entries.end(),
                               [dayEpoch](const DayEntry& entry) { return entry.dayEpoch == dayEpoch; });
  return it != entries.end() ? it->seconds : 0;
}

uint32_t DailyReadingLog::computeCurrentStreak() {
  std::vector<DayEntry> entries;
  uint32_t achievementsMask = 0;
  loadAll(entries, achievementsMask);
  if (entries.empty()) return 0;

  std::sort(entries.begin(), entries.end(),
            [](const DayEntry& a, const DayEntry& b) { return a.dayEpoch < b.dayEpoch; });

  const uint32_t today = todayEpochDay();
  // Find the most recent day with reading time, allowing "today" to be missing so
  // far (streak isn't broken until a full day passes with zero reading).
  int lastIdx = -1;
  for (int i = static_cast<int>(entries.size()) - 1; i >= 0; --i) {
    if (entries[i].seconds > 0 && entries[i].dayEpoch <= today) {
      lastIdx = i;
      break;
    }
  }
  if (lastIdx < 0) return 0;
  if (today - entries[lastIdx].dayEpoch > 1) return 0;  // most recent reading day was >1 day ago: streak is broken

  uint32_t streak = 1;
  uint32_t expectedDay = entries[lastIdx].dayEpoch;
  for (int i = lastIdx - 1; i >= 0; --i) {
    if (entries[i].seconds == 0) continue;
    if (entries[i].dayEpoch == expectedDay - 1) {
      streak++;
      expectedDay = entries[i].dayEpoch;
    } else {
      break;
    }
  }
  return streak;
}

uint32_t DailyReadingLog::getUnlockedAchievementsMask() {
  std::vector<DayEntry> entries;
  uint32_t achievementsMask = 0;
  loadAll(entries, achievementsMask);
  return achievementsMask;
}

void DailyReadingLog::setUnlockedAchievementsMask(const uint32_t mask) {
  std::vector<DayEntry> entries;
  uint32_t achievementsMask = 0;
  loadAll(entries, achievementsMask);
  saveAll(entries, mask);
}

void DailyReadingLog::removeIfEmpty(const uint32_t dayEpoch) {
  std::vector<DayEntry> entries;
  uint32_t achievementsMask = 0;
  loadAll(entries, achievementsMask);

  entries.erase(std::remove_if(entries.begin(), entries.end(),
                               [dayEpoch](const DayEntry& e) { return e.dayEpoch == dayEpoch && e.seconds == 0; }),
                entries.end());
  saveAll(entries, achievementsMask);
}

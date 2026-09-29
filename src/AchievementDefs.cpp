#include "AchievementDefs.h"

#include "activities/reader/DailyReadingLog.h"

namespace Achievements {

void checkAndUpdate(const uint32_t currentStreak, const uint32_t completedBooks) {
  uint32_t mask = DailyReadingLog::getUnlockedAchievementsMask();
  const uint32_t before = mask;

  for (const auto& def : kDefs) {
    const uint32_t progress = (def.kind == AchievementDef::Kind::StreakDays) ? currentStreak : completedBooks;
    if (progress >= def.threshold) {
      mask |= def.bit;
    }
  }

  if (mask != before) {
    DailyReadingLog::setUnlockedAchievementsMask(mask);
  }
}

int mostRecentUnlockedIndex() {
  const uint32_t mask = DailyReadingLog::getUnlockedAchievementsMask();
  for (int i = static_cast<int>(kCount) - 1; i >= 0; --i) {
    if (mask & kDefs[i].bit) return i;
  }
  return -1;
}

}  // namespace Achievements

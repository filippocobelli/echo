#pragma once

#include <I18n.h>

#include <cstddef>
#include <cstdint>

// Small fixed set of reading milestones. Unlocked state is a bitmask persisted
// via DailyReadingLog (see DailyReadingLog::getUnlockedAchievementsMask()).
struct AchievementDef {
  enum class Kind { StreakDays, BooksCompleted };
  uint32_t bit;
  StrId label;
  Kind kind;
  uint32_t threshold;
};

namespace Achievements {

inline constexpr AchievementDef kDefs[] = {
    {1u << 0, StrId::STR_ACHIEVEMENT_STREAK_3, AchievementDef::Kind::StreakDays, 3},
    {1u << 1, StrId::STR_ACHIEVEMENT_STREAK_7, AchievementDef::Kind::StreakDays, 7},
    {1u << 2, StrId::STR_ACHIEVEMENT_STREAK_30, AchievementDef::Kind::StreakDays, 30},
    {1u << 3, StrId::STR_ACHIEVEMENT_STREAK_100, AchievementDef::Kind::StreakDays, 100},
    {1u << 4, StrId::STR_ACHIEVEMENT_BOOKS_1, AchievementDef::Kind::BooksCompleted, 1},
    {1u << 5, StrId::STR_ACHIEVEMENT_BOOKS_5, AchievementDef::Kind::BooksCompleted, 5},
    {1u << 6, StrId::STR_ACHIEVEMENT_BOOKS_10, AchievementDef::Kind::BooksCompleted, 10},
    {1u << 7, StrId::STR_ACHIEVEMENT_BOOKS_25, AchievementDef::Kind::BooksCompleted, 25},
    {1u << 8, StrId::STR_ACHIEVEMENT_BOOKS_50, AchievementDef::Kind::BooksCompleted, 50},
};
inline constexpr size_t kCount = sizeof(kDefs) / sizeof(kDefs[0]);

// Unlocks any newly-satisfied achievements given current progress and persists
// the updated mask. Call whenever streak or completedBooks may have changed.
void checkAndUpdate(uint32_t currentStreak, uint32_t completedBooks);

// Index into kDefs of the "latest" unlocked achievement, approximated as the
// highest-threshold unlocked one (there's no unlock timestamp), or -1 if none.
int mostRecentUnlockedIndex();

}  // namespace Achievements

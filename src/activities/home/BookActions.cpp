#include "BookActions.h"

#include <Epub.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Xtc.h>

#include <algorithm>
#include <cstdio>

#include "AchievementDefs.h"
#include "BookmarkStore.h"
#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "FavoritesStore.h"
#include "RecentBooksStore.h"
#include "activities/reader/BookReadingStats.h"
#include "activities/reader/DailyReadingLog.h"
#include "activities/reader/GlobalReadingStats.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {

std::string buildReadFolderDestination(const std::string& srcPath) {
  const size_t lastSlash = srcPath.rfind('/');
  const std::string filename = (lastSlash != std::string::npos) ? srcPath.substr(lastSlash + 1) : srcPath;

  Storage.mkdir("/Read");
  std::string dstPath = "/Read/" + filename;
  if (!Storage.exists(dstPath.c_str())) {
    return dstPath;
  }

  const size_t dotPos = filename.rfind('.');
  const std::string base = (dotPos != std::string::npos) ? filename.substr(0, dotPos) : filename;
  const std::string ext = (dotPos != std::string::npos) ? filename.substr(dotPos) : "";
  int suffix = 2;
  do {
    dstPath = "/Read/" + base + " (" + std::to_string(suffix) + ")" + ext;
    suffix++;
  } while (Storage.exists(dstPath.c_str()) && suffix < 100);
  return dstPath;
}

}  // namespace

namespace BookActions {

std::vector<FileBrowserActionActivity::MenuItem> buildBookActionItems(const std::string& fullPath,
                                                                      const bool includeRemoveFromRecents) {
  std::vector<FileBrowserActionActivity::MenuItem> items;
  items.reserve(includeRemoveFromRecents ? 5 : 4);
  items.push_back({FileBrowserAction::Delete, StrId::STR_DELETE});
  if (hasClearableBookCache(fullPath)) {
    items.push_back({FileBrowserAction::DeleteCache, StrId::STR_DELETE_CACHE});
  }
  if (FsHelpers::hasEpubExtension(fullPath)) {
    items.push_back({FileBrowserAction::ToggleCompleted,
                     isEpubCompleted(fullPath) ? StrId::STR_MARK_UNFINISHED : StrId::STR_MARK_FINISHED});
  }
  items.push_back(
      {FAVORITES.contains(fullPath) ? FileBrowserAction::RemoveFavoriteBook : FileBrowserAction::AddFavoriteBook,
       FAVORITES.contains(fullPath) ? StrId::STR_REMOVE_FAVORITE : StrId::STR_ADD_FAVORITE});
  if (includeRemoveFromRecents) {
    items.push_back({FileBrowserAction::RemoveFromRecents, StrId::STR_REMOVE_FROM_RECENTS_ACTION});
  }
  return items;
}

bool hasClearableBookCache(const std::string& path) {
  return FsHelpers::hasEpubExtension(path) || FsHelpers::hasXtcExtension(path);
}

void clearFileMetadata(const std::string& fullPath) {
  if (FsHelpers::hasEpubExtension(fullPath)) {
    Epub(fullPath, "/.crosspoint").clearCache();
    BookmarkStore::deleteForFilePath(fullPath, "epub");
  } else if (FsHelpers::hasXtcExtension(fullPath)) {
    BookmarkStore::deleteForFilePath(fullPath, "xtc");
  } else if (FsHelpers::hasTxtExtension(fullPath) || FsHelpers::hasMarkdownExtension(fullPath)) {
    BookmarkStore::deleteForFilePath(fullPath, "txt");
  }
  LOG_DBG("BookActions", "Cleared metadata for: %s", fullPath.c_str());
}

bool clearBookCache(const std::string& fullPath) {
  if (FsHelpers::hasEpubExtension(fullPath)) {
    return Epub(fullPath, "/.crosspoint").clearCache();
  }
  if (FsHelpers::hasXtcExtension(fullPath)) {
    return Xtc(fullPath, "/.crosspoint").clearCache();
  }
  return false;
}

bool isEpubCompleted(const std::string& fullPath) {
  const Epub epub(fullPath, "/.crosspoint");
  return BookReadingStats::load(epub.getCachePath()).isCompleted;
}

bool toggleEpubCompleted(const std::string& fullPath, const std::string& displayName, bool& completed) {
  if (!FsHelpers::hasEpubExtension(fullPath)) {
    return false;
  }

  Epub epub(fullPath, "/.crosspoint");
  epub.setupCacheDir();

  BookReadingStats stats = BookReadingStats::load(epub.getCachePath());
  completed = !stats.isCompleted;
  stats.isCompleted = completed;

  GlobalReadingStats globalStats = GlobalReadingStats::load();
  if (completed) {
    globalStats.completedBooks++;
  } else if (globalStats.completedBooks > 0) {
    globalStats.completedBooks--;
  }

  stats.save(epub.getCachePath());
  globalStats.save();
  Achievements::checkAndUpdate(DailyReadingLog::computeCurrentStreak(), globalStats.completedBooks);

  if (completed && SETTINGS.moveFinishedToReadFolder && fullPath.rfind("/Read/", 0) != 0) {
    const std::string oldCachePath = epub.getCachePath();
    const std::string dstPath = buildReadFolderDestination(fullPath);
    LOG_INF("BookActions", "Moving completed epub: %s -> %s", fullPath.c_str(), dstPath.c_str());
    if (!Storage.rename(fullPath.c_str(), dstPath.c_str())) {
      LOG_ERR("BookActions", "Failed to move book to 'Read' folder");
      snprintf(APP_STATE.pendingAlertTitle, sizeof(APP_STATE.pendingAlertTitle), "%s",
               tr(STR_MOVE_TO_READ_FAILED_TITLE));
      snprintf(APP_STATE.pendingAlertBody, sizeof(APP_STATE.pendingAlertBody), tr(STR_MOVE_TO_READ_FAILED_BODY),
               displayName.c_str());
      APP_STATE.pendingAlertGoHomeOnBack.store(false, std::memory_order_relaxed);
      APP_STATE.hasPendingAlert.store(true, std::memory_order_release);
      return true;
    }

    const std::string newCachePath = Epub::cachePathForFilePath(dstPath, "/.crosspoint");
    if (!oldCachePath.empty() && Storage.exists(oldCachePath.c_str())) {
      if (!Storage.rename(oldCachePath.c_str(), newCachePath.c_str())) {
        LOG_ERR("BookActions", "Failed to rename cache dir %s -> %s (non-fatal)", oldCachePath.c_str(),
                newCachePath.c_str());
      }
    }

    RECENT_BOOKS.updatePath(fullPath, dstPath, oldCachePath, newCachePath);
    if (APP_STATE.openEpubPath == fullPath) {
      APP_STATE.openEpubPath = dstPath;
      APP_STATE.saveToFile();
    }
  }

  return true;
}

namespace {
uint32_t saturatingSub(const uint32_t a, const uint32_t b) { return a > b ? a - b : 0; }
}  // namespace

void correctBookReadingTime(const std::string& cachePath, const uint32_t daysAgo, const uint16_t minutes,
                            const bool isAdd) {
  const int32_t deltaSeconds = static_cast<int32_t>(minutes) * 60 * (isAdd ? 1 : -1);

  BookReadingStats stats = BookReadingStats::load(cachePath);
  GlobalReadingStats globalStats = GlobalReadingStats::load();

  if (isAdd) {
    stats.totalReadingSeconds += static_cast<uint32_t>(minutes) * 60;
    globalStats.totalReadingSeconds += static_cast<uint32_t>(minutes) * 60;
  } else {
    stats.totalReadingSeconds = saturatingSub(stats.totalReadingSeconds, static_cast<uint32_t>(minutes) * 60);
    globalStats.totalReadingSeconds =
        saturatingSub(globalStats.totalReadingSeconds, static_cast<uint32_t>(minutes) * 60);
  }

  stats.save(cachePath);
  globalStats.save();

  const uint32_t dayEpoch = DailyReadingLog::todayEpochDay() - daysAgo;
  DailyReadingLog::addSeconds(dayEpoch, deltaSeconds);
  if (!isAdd) DailyReadingLog::removeIfEmpty(dayEpoch);
  Achievements::checkAndUpdate(DailyReadingLog::computeCurrentStreak(), globalStats.completedBooks);
}

void setBookStartDateOverride(const std::string& cachePath, const uint32_t daysAgo) {
  BookReadingStats stats = BookReadingStats::load(cachePath);
  stats.startDateOverrideEpochDay = DailyReadingLog::todayEpochDay() - daysAgo;
  stats.save(cachePath);
}

void resetBookStats(const std::string& cachePath) {
  const BookReadingStats oldStats = BookReadingStats::load(cachePath);

  GlobalReadingStats globalStats = GlobalReadingStats::load();
  globalStats.totalSessions = saturatingSub(globalStats.totalSessions, oldStats.sessionCount);
  globalStats.totalReadingSeconds = saturatingSub(globalStats.totalReadingSeconds, oldStats.totalReadingSeconds);
  globalStats.totalPagesTurned = saturatingSub(globalStats.totalPagesTurned, oldStats.totalPagesTurned);
  if (oldStats.isCompleted && globalStats.completedBooks > 0) {
    globalStats.completedBooks--;
  }
  globalStats.save();

  // Per-book-per-day attribution isn't tracked, so the book's total is subtracted
  // from today's bucket rather than reversed day-by-day across its real history.
  const uint32_t today = DailyReadingLog::todayEpochDay();
  DailyReadingLog::addSeconds(today,
                              -static_cast<int32_t>(std::min<uint32_t>(oldStats.totalReadingSeconds, INT32_MAX)));
  DailyReadingLog::removeIfEmpty(today);
  Achievements::checkAndUpdate(DailyReadingLog::computeCurrentStreak(), globalStats.completedBooks);

  BookReadingStats().save(cachePath);
}

void drawToast(const GfxRenderer& renderer, const char* msg) {
  constexpr int toastPadX = 20;
  constexpr int toastPadY = 12;
  const int msgW = renderer.getTextWidth(UI_10_FONT_ID, msg);
  const int msgH = renderer.getLineHeight(UI_10_FONT_ID);
  const int toastW = msgW + toastPadX * 2;
  const int toastH = msgH + toastPadY * 2;
  const int toastX = (renderer.getScreenWidth() - toastW) / 2;
  const int toastY = (renderer.getScreenHeight() - toastH) / 2;
  renderer.fillRect(toastX, toastY, toastW, toastH, true);
  renderer.drawText(UI_10_FONT_ID, toastX + toastPadX, toastY + toastPadY, msg, false);
  renderer.displayBuffer();
}

}  // namespace BookActions

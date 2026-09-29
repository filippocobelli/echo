#pragma once

#include <string>
#include <vector>

#include "FileBrowserActionActivity.h"

class GfxRenderer;

namespace BookActions {

std::vector<FileBrowserActionActivity::MenuItem> buildBookActionItems(const std::string& fullPath,
                                                                      bool includeRemoveFromRecents);
bool hasClearableBookCache(const std::string& path);
void clearFileMetadata(const std::string& fullPath);
bool clearBookCache(const std::string& fullPath);
bool isEpubCompleted(const std::string& fullPath);
bool toggleEpubCompleted(const std::string& fullPath, const std::string& displayName, bool& completed);
void drawToast(const GfxRenderer& renderer, const char* msg);

// AZIONE A: adds (isAdd=true) or subtracts (isAdd=false) minutes*60 seconds of
// reading time for the given day (days-ago offset from today), updating the
// book's stats, the global aggregate, and the daily reading log. Saturating.
void correctBookReadingTime(const std::string& cachePath, uint32_t daysAgo, uint16_t minutes, bool isAdd);

// AZIONE B: sets (or clears, if daysAgo is negative) the display-only start-date
// override shown in the book's stats. Does not touch session/stats data.
void setBookStartDateOverride(const std::string& cachePath, uint32_t daysAgo);

// AZIONE C: resets this book's stats to zero, subtracting its previous totals
// (and completed-book count, if applicable) from the global aggregate and the
// daily reading log's most recent bucket.
void resetBookStats(const std::string& cachePath);

}  // namespace BookActions

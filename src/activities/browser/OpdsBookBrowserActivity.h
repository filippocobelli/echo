#pragma once
#include <OpdsParser.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "OpdsServerStore.h"
#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

/**
 * Activity for browsing and downloading books from an OPDS server.
 * Supports navigation through catalog hierarchy and downloading EPUBs.
 */
class OpdsBookBrowserActivity final : public Activity {
 public:
  enum class BrowserState { CHECK_WIFI, WIFI_SELECTION, LOADING, BROWSING, DOWNLOADING, ERROR, SEARCH_INPUT };

  explicit OpdsBookBrowserActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, OpdsServer server)
      : Activity("OpdsBookBrowser", renderer, mappedInput), buttonNavigator(), server(std::move(server)) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  ButtonNavigator buttonNavigator;
  BrowserState state = BrowserState::LOADING;
  std::unique_ptr<OpdsEntry[]> entries;
  size_t entryCount = 0;
  uint64_t coverFailedMask = 0;  // Bit i set: cover for entries[i] failed this feed, don't refetch
  std::vector<std::string> navigationHistory;
  std::string currentPath;
  std::string searchTemplate;
  bool consumeConfirm = false;
  bool consumeBack = false;  // Added missing member
  int selectorIndex = 0;
  std::string errorMessage;
  std::string statusMessage;
  size_t downloadProgress = 0;
  size_t downloadTotal = 0;

  OpdsServer server;  // Copied at construction — safe even if the store changes during browsing

  // Grid view (opdsViewMode = Grid): 2 columns, covers downloaded+converted
  // lazily per visible cell and cached under /.crosspoint/opds_covers/.
  static constexpr int GRID_COLUMNS = 2;
  static constexpr int GRID_COVER_WIDTH = 150;
  static constexpr int GRID_COVER_HEIGHT = 200;
  std::string coverCachePathFor(const OpdsEntry& entry) const;
  bool ensureCoverCached(const OpdsEntry& entry, std::string& outBmpPath);
  void renderGrid(int contentTop, int contentHeight);
  void renderList(int contentTop, int contentHeight);

  void checkAndConnectWifi();
  void launchWifiSelection();
  void onWifiSelectionComplete(bool connected);
  void showLoadingBeforeFetch();
  void fetchFeed(const std::string& path);
  bool ensureEntryBuffer();
  void clearEntries();
  bool appendEntry(OpdsEntry&& entry);
  void navigateToEntry(const OpdsEntry& entry);
  void navigateBack();
  void downloadBook(const OpdsEntry& book);
  void launchSearch();
  void performSearch(const std::string& query);
  bool preventAutoSleep() override;
};

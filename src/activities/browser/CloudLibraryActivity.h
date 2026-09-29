#pragma once
#include <WebDavParser.h>

#include <memory>
#include <string>
#include <vector>

#include "activities/Activity.h"
#include "network/WebDavClient.h"
#include "util/ButtonNavigator.h"

/**
 * Browses a remote WebDAV cloud drive (Koofr, Nextcloud, ownCloud ...) with the
 * same interaction model as the local file browser: folders descend, Back goes
 * up, and Confirm on a book downloads it to the SD card and opens it.
 *
 * The remote listing is fetched with PROPFIND (see WebDavClient); only folders
 * and supported book formats are shown.
 */
class CloudLibraryActivity final : public Activity {
 public:
  enum class BrowserState { CHECK_WIFI, WIFI_SELECTION, LOADING, BROWSING, DOWNLOADING, ERROR };

  explicit CloudLibraryActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("CloudLibrary", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  ButtonNavigator buttonNavigator;
  BrowserState state = BrowserState::CHECK_WIFI;

  WebDavClient::Account account;
  // Allocated once in onEnter() and reused for every folder, so navigating deep
  // into a drive never churns the heap.
  std::unique_ptr<WebDavEntry[]> entries;
  size_t entryCount = 0;
  bool listTruncated = false;

  std::string rootPath;     // configured starting folder; Back at this level goes home
  std::string currentPath;  // absolute, percent-encoded server path
  std::vector<std::string> navigationHistory;

  int selectorIndex = 0;
  std::string errorMessage;
  std::string statusMessage;
  size_t downloadProgress = 0;
  size_t downloadTotal = 0;
  // Set once a book has been downloaded: onExit reboots straight into it,
  // which also clears the WiFi session's heap fragmentation.
  std::string pendingOpenPath;

  bool ensureEntryBuffer();
  void checkAndConnectWifi();
  void launchWifiSelection();
  void onWifiSelectionComplete(bool connected);
  void showLoadingBeforeFetch();
  void fetchDirectory(const std::string& path);
  void navigateToEntry(const WebDavEntry& entry);
  void navigateBack();
  void downloadBook(const WebDavEntry& book);
  int listPageItems() const;
  bool preventAutoSleep() override;
};

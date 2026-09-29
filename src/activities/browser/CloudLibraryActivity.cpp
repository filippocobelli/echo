#include "CloudLibraryActivity.h"

#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>
#include <WiFi.h>

#include <algorithm>
#include <cstdio>
#include <utility>

#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "MappedInputManager.h"
#include "SdCardFontSystem.h"
#include "SilentRestart.h"
#include "activities/network/WifiSelectionActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "network/HttpDownloader.h"
#include "util/BookCacheUtils.h"
#include "util/StringUtils.h"

namespace {
constexpr size_t CLOUD_DOWNLOAD_BUFFER_SIZE = 4096;

// Only formats the reader can open are worth listing; everything else on the
// drive would just be dead rows the user cannot act on.
bool isSupportedBook(const std::string& name) {
  return FsHelpers::hasEpubExtension(name) || FsHelpers::hasTxtExtension(name) || FsHelpers::hasXtcExtension(name);
}

std::string formatSize(const uint32_t bytes) {
  char buffer[16];
  if (bytes >= 1024u * 1024u) {
    snprintf(buffer, sizeof(buffer), "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
  } else if (bytes >= 1024u) {
    snprintf(buffer, sizeof(buffer), "%u KB", static_cast<unsigned>(bytes / 1024u));
  } else if (bytes > 0) {
    snprintf(buffer, sizeof(buffer), "%u B", static_cast<unsigned>(bytes));
  } else {
    return {};
  }
  return buffer;
}

StrId messageForError(const WebDavClient::Error error) {
  switch (error) {
    case WebDavClient::Error::NO_URL:
      return StrId::STR_CLOUD_NOT_CONFIGURED;
    case WebDavClient::Error::AUTH_FAILED:
      return StrId::STR_CLOUD_AUTH_FAILED;
    case WebDavClient::Error::NO_BUFFER:
      return StrId::STR_MEMORY_ERROR;
    case WebDavClient::Error::PARSE_FAILED:
    case WebDavClient::Error::NOT_FOUND:
    case WebDavClient::Error::HTTP_FAILED:
    case WebDavClient::Error::OK:
    default:
      return StrId::STR_CLOUD_LIST_FAILED;
  }
}
}  // namespace

void CloudLibraryActivity::onEnter() {
  Activity::onEnter();

  // Free the SD-card font before the TLS handshake: NetworkClientSecure needs a
  // large contiguous allocation and the font buffer is the biggest rival for it.
  sdFontSystem.releaseLoadedFont(renderer);

  account.url = SETTINGS.cloudWebdavUrl;
  account.username = SETTINGS.cloudWebdavUsername;
  account.password = SETTINGS.cloudWebdavPassword;

  entryCount = 0;
  listTruncated = false;
  selectorIndex = 0;
  navigationHistory.clear();
  errorMessage.clear();
  pendingOpenPath.clear();
  statusMessage = tr(STR_CHECKING_WIFI);

  rootPath = WebDavClient::rootPathFor(account, SETTINGS.cloudWebdavRootPath);
  currentPath = rootPath;

  if (account.url.empty()) {
    state = BrowserState::ERROR;
    errorMessage = tr(STR_CLOUD_NOT_CONFIGURED);
    requestUpdate();
    return;
  }

  if (!ensureEntryBuffer()) {
    state = BrowserState::ERROR;
    errorMessage = tr(STR_MEMORY_ERROR);
    requestUpdate();
    return;
  }

  state = BrowserState::CHECK_WIFI;
  requestUpdate();
  checkAndConnectWifi();
}

void CloudLibraryActivity::onExit() {
  Activity::onExit();
  entryCount = 0;
  entries.reset();
  navigationHistory.clear();

  const bool wifiWasUp = WiFi.getMode() != WIFI_MODE_NULL;
  if (wifiWasUp) {
    WiFi.disconnect(false);
    delay(30);
  }

  // Rebooting is how every WiFi activity here hands memory back; routing the
  // reboot to the reader is what makes "download then read" a single gesture.
  if (!pendingOpenPath.empty()) {
    APP_STATE.openEpubPath = pendingOpenPath;
    APP_STATE.saveToFile();
    silentRestartToReader();
    return;
  }
  if (wifiWasUp) silentRestart();
}

bool CloudLibraryActivity::ensureEntryBuffer() {
  if (entries) return true;
  entries = makeUniqueNoThrow<WebDavEntry[]>(MAX_WEBDAV_ENTRIES);
  return entries != nullptr;
}

int CloudLibraryActivity::listPageItems() const {
  const int pathReserved = renderer.getLineHeight(SMALL_FONT_ID) + UITheme::getInstance().getMetrics().verticalSpacing;
  return std::max(1, UITheme::getNumberOfItemsPerPage(renderer, true, false, true, false, pathReserved));
}

bool CloudLibraryActivity::preventAutoSleep() {
  switch (state) {
    case BrowserState::CHECK_WIFI:
    case BrowserState::WIFI_SELECTION:
    case BrowserState::LOADING:
    case BrowserState::DOWNLOADING:
      return true;
    case BrowserState::BROWSING:
    case BrowserState::ERROR:
      return false;
  }
  return false;
}

void CloudLibraryActivity::loop() {
  if (state == BrowserState::WIFI_SELECTION || state == BrowserState::DOWNLOADING) return;

  if (state == BrowserState::ERROR) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      if (account.url.empty()) return;  // Nothing to retry until the account is set up
      if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
        showLoadingBeforeFetch();
        fetchDirectory(currentPath);
      } else {
        launchWifiSelection();
      }
    } else if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      navigateBack();
    }
    return;
  }

  if (state == BrowserState::CHECK_WIFI || state == BrowserState::LOADING) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      state == BrowserState::CHECK_WIFI ? onGoHome() : navigateBack();
    }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (entryCount > 0) {
      const auto& entry = entries[selectorIndex];
      entry.isCollection ? navigateToEntry(entry) : downloadBook(entry);
    }
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    navigateBack();
    return;
  }

  if (entryCount == 0) return;

  const int listSize = static_cast<int>(entryCount);
  const int pageItems = listPageItems();
  buttonNavigator.onNextRelease([this, listSize] {
    selectorIndex = ButtonNavigator::nextIndex(selectorIndex, listSize);
    requestUpdate();
  });
  buttonNavigator.onPreviousRelease([this, listSize] {
    selectorIndex = ButtonNavigator::previousIndex(selectorIndex, listSize);
    requestUpdate();
  });
  buttonNavigator.onNextContinuous([this, listSize, pageItems] {
    selectorIndex = ButtonNavigator::nextPageIndex(selectorIndex, listSize, pageItems);
    requestUpdate();
  });
  buttonNavigator.onPreviousContinuous([this, listSize, pageItems] {
    selectorIndex = ButtonNavigator::previousPageIndex(selectorIndex, listSize, pageItems);
    requestUpdate();
  });
}

void CloudLibraryActivity::showLoadingBeforeFetch() {
  state = BrowserState::LOADING;
  statusMessage = tr(STR_LOADING);
  if (requestUpdateAndWait() != RequestUpdateResult::Rendered) {
    LOG_ERR("CLOUD", "Loading screen could not be rendered before PROPFIND");
    requestUpdate(true);
  }
}

void CloudLibraryActivity::fetchDirectory(const std::string& path) {
  if (!ensureEntryBuffer()) {
    state = BrowserState::ERROR;
    errorMessage = tr(STR_MEMORY_ERROR);
    requestUpdate();
    return;
  }

  entryCount = 0;
  size_t parsedCount = 0;
  const auto error =
      WebDavClient::listDirectory(account, path, entries.get(), MAX_WEBDAV_ENTRIES, parsedCount, listTruncated);
  if (error != WebDavClient::Error::OK) {
    state = BrowserState::ERROR;
    errorMessage = I18N.get(messageForError(error));
    requestUpdate();
    return;
  }

  // Keep folders and readable books; drop everything else in place so the
  // single preallocated buffer stays the only storage we use.
  size_t kept = 0;
  for (size_t i = 0; i < parsedCount; i++) {
    if (!entries[i].isCollection && !isSupportedBook(entries[i].name)) continue;
    if (kept != i) entries[kept] = std::move(entries[i]);
    kept++;
  }
  for (size_t i = kept; i < parsedCount; i++) {
    entries[i] = WebDavEntry{};
  }
  entryCount = kept;

  selectorIndex = 0;
  state = BrowserState::BROWSING;
  requestUpdate();
}

void CloudLibraryActivity::navigateToEntry(const WebDavEntry& entry) {
  navigationHistory.push_back(currentPath);
  currentPath = entry.path;
  entryCount = 0;
  selectorIndex = 0;
  showLoadingBeforeFetch();
  fetchDirectory(currentPath);
}

void CloudLibraryActivity::navigateBack() {
  if (navigationHistory.empty()) {
    onGoHome();
    return;
  }
  currentPath = navigationHistory.back();
  navigationHistory.pop_back();
  entryCount = 0;
  selectorIndex = 0;
  showLoadingBeforeFetch();
  fetchDirectory(currentPath);
}

void CloudLibraryActivity::downloadBook(const WebDavEntry& book) {
  state = BrowserState::DOWNLOADING;
  statusMessage = book.name;
  downloadProgress = downloadTotal = 0;
  requestUpdate(true);

  const std::string url = WebDavClient::urlForPath(account, book.path);
  const std::string destination = "/" + StringUtils::sanitizeFilename(book.name);
  LOG_DBG("CLOUD", "Downloading %s -> %s", url.c_str(), destination.c_str());

  bool cancelRequested = false;
  auto pollCancel = [this, &cancelRequested] {
    if (cancelRequested) return true;
    mappedInput.update();
    if (mappedInput.isPressed(MappedInputManager::Button::Back) ||
        mappedInput.wasPressed(MappedInputManager::Button::Back) ||
        mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      cancelRequested = true;
    }
    return cancelRequested;
  };

  HttpDownloader::DownloadOptions options;
  options.shouldCancel = pollCancel;
  options.bufferSize = CLOUD_DOWNLOAD_BUFFER_SIZE;

  const auto result = HttpDownloader::downloadToFile(
      url, destination,
      [this](const size_t downloaded, const size_t total) {
        downloadProgress = downloaded;
        downloadTotal = total;
        requestUpdate(true);
      },
      &cancelRequested, account.username, account.password, options);

  if (result == HttpDownloader::OK) {
    // A previous file of the same name may have left a stale reading cache.
    clearBookCache(destination);
    pendingOpenPath = destination;
    state = BrowserState::BROWSING;
    // Leaving the activity tears down WiFi and reboots into the new book.
    onGoHome();
    return;
  }

  if (result == HttpDownloader::ABORTED) {
    LOG_DBG("CLOUD", "Download cancelled");
    mappedInput.suppressNextBackRelease();
    state = BrowserState::BROWSING;
  } else {
    state = BrowserState::ERROR;
    errorMessage = tr(STR_DOWNLOAD_FAILED);
  }
  requestUpdate();
}

void CloudLibraryActivity::checkAndConnectWifi() {
  if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
    showLoadingBeforeFetch();
    fetchDirectory(currentPath);
    return;
  }
  launchWifiSelection();
}

void CloudLibraryActivity::launchWifiSelection() {
  state = BrowserState::WIFI_SELECTION;
  requestUpdate();

  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                         [this](const ActivityResult& result) { onWifiSelectionComplete(!result.isCancelled); });
}

void CloudLibraryActivity::onWifiSelectionComplete(const bool connected) {
  if (connected) {
    showLoadingBeforeFetch();
    fetchDirectory(currentPath);
    return;
  }
  // Leave WiFi up; onExit's silent reboot handles teardown without fragmenting.
  state = BrowserState::ERROR;
  errorMessage = tr(STR_WIFI_CONN_FAILED);
  requestUpdate();
}

void CloudLibraryActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  const std::string folderName = WebDavPath::decode(WebDavPath::lastSegment(currentPath));
  const bool atRoot = navigationHistory.empty();
  const char* headerTitle = (atRoot || folderName.empty()) ? tr(STR_CLOUD_LIBRARY) : folderName.c_str();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, headerTitle);

  if (state == BrowserState::CHECK_WIFI || state == BrowserState::LOADING) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, statusMessage.c_str());
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  if (state == BrowserState::ERROR) {
    const auto lines = renderer.wrappedText(UI_10_FONT_ID, errorMessage.c_str(), pageWidth - 80, 3);
    int y = pageHeight / 2 - static_cast<int>(lines.size()) * renderer.getLineHeight(UI_10_FONT_ID) / 2;
    for (const auto& line : lines) {
      renderer.drawCenteredText(UI_10_FONT_ID, y, line.c_str());
      y += renderer.getLineHeight(UI_10_FONT_ID);
    }
    const char* retryLabel = account.url.empty() ? "" : tr(STR_RETRY);
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), retryLabel, "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  if (state == BrowserState::DOWNLOADING) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 40, tr(STR_DOWNLOADING));
    const auto title = renderer.truncatedText(UI_10_FONT_ID, statusMessage.c_str(), pageWidth - 40);
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 10, title.c_str());
    if (downloadTotal > 0) {
      GUI.drawProgressBar(renderer, Rect{50, pageHeight / 2 + 20, pageWidth - 100, 20}, downloadProgress,
                          downloadTotal);
    }
    const auto labels = mappedInput.mapLabels(tr(STR_CANCEL), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  const int pathLineHeight = renderer.getLineHeight(SMALL_FONT_ID);
  const int pathReserved = pathLineHeight + metrics.verticalSpacing;
  const int pathY = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing - pathLineHeight;
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight =
      pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing - pathReserved;

  if (entryCount == 0) {
    renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, contentTop + 20, tr(STR_CLOUD_FOLDER_EMPTY));
  } else {
    GUI.drawList(
        renderer, Rect{0, contentTop, pageWidth, contentHeight}, static_cast<int>(entryCount), selectorIndex,
        [this](int index) { return entries[index].name; }, nullptr,
        [this](int index) { return entries[index].isCollection ? Folder : UITheme::getFileIcon(entries[index].name); },
        [this](int index) { return entries[index].isCollection ? std::string() : formatSize(entries[index].size); },
        false);
  }

  // Remote path, left-truncated so the deepest folder stays visible.
  {
    const int separatorY = pathY - metrics.verticalSpacing / 2;
    renderer.drawLine(0, separatorY, pageWidth - 1, separatorY, 3, true);
    std::string displayPath = WebDavPath::decode(currentPath);
    // A folder with more members than the buffer holds is shown capped, not silently short.
    if (listTruncated) displayPath += "  (" + std::to_string(MAX_WEBDAV_ENTRIES) + "+)";
    const auto truncated =
        renderer.truncatedText(SMALL_FONT_ID, displayPath.c_str(), pageWidth - metrics.contentSidePadding * 2);
    renderer.drawText(SMALL_FONT_ID, metrics.contentSidePadding, pathY, truncated.c_str());
  }

  const char* backLabel = atRoot ? tr(STR_HOME) : tr(STR_BACK);
  const char* confirmLabel =
      entryCount == 0 ? "" : (entries[selectorIndex].isCollection ? tr(STR_OPEN) : tr(STR_DOWNLOAD));
  const auto labels = mappedInput.mapLabels(backLabel, confirmLabel, entryCount == 0 ? "" : tr(STR_DIR_UP),
                                            entryCount == 0 ? "" : tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

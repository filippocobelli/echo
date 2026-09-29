#include "KOReaderSettingsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <WiFi.h>

#include <cstring>

#include "KOReaderAuthActivity.h"
#include "KOReaderCredentialStore.h"
#include "KOReaderSyncClient.h"
#include "MappedInputManager.h"
#include "SilentRestart.h"
#include "activities/network/WifiSelectionActivity.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
// Menu indices — no credentials
constexpr int NO_CREDS_SERVER_URL = 0;
constexpr int NO_CREDS_DOC_MATCHING = 1;
constexpr int NO_CREDS_REGISTER = 2;
constexpr int NO_CREDS_LOGIN = 3;
constexpr int NO_CREDS_COUNT = 4;

// Menu indices — credentials saved
constexpr int HAS_CREDS_USERNAME = 0;
constexpr int HAS_CREDS_SERVER_URL = 1;
constexpr int HAS_CREDS_DOC_MATCHING = 2;
constexpr int HAS_CREDS_DISCONNECT = 3;
constexpr int HAS_CREDS_TEST = 4;
constexpr int HAS_CREDS_COUNT = 5;
}  // namespace

void KOReaderSettingsActivity::onEnter() {
  Activity::onEnter();
  selectedIndex = 0;
  pageState = PageState::MAIN;
  requestUpdate();
}

void KOReaderSettingsActivity::onExit() { Activity::onExit(); }

int KOReaderSettingsActivity::menuItemCount() const {
  return KOREADER_STORE.hasCredentials() ? HAS_CREDS_COUNT : NO_CREDS_COUNT;
}

void KOReaderSettingsActivity::loop() {
  if (pageState == PageState::RESULT) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Back) ||
        mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      if (needsWifiCleanup && WiFi.getMode() != WIFI_MODE_NULL) {
        WiFi.disconnect(false);
        delay(30);
        silentRestart();
        return;
      }
      pageState = PageState::MAIN;
      selectedIndex = 0;
      requestUpdate();
    }
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    handleSelection();
    return;
  }

  const int count = menuItemCount();
  buttonNavigator.onNext([this, count] {
    selectedIndex = (selectedIndex + 1) % count;
    requestUpdate();
  });

  buttonNavigator.onPrevious([this, count] {
    selectedIndex = (selectedIndex + count - 1) % count;
    requestUpdate();
  });
}

void KOReaderSettingsActivity::editServerUrl() {
  const std::string currentUrl = KOREADER_STORE.getServerUrl();
  const std::string prefill = currentUrl.empty() ? "https://" : currentUrl;
  startActivityForResult(std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_SYNC_SERVER_URL),
                                                                 prefill, 128, InputType::Url),
                         [this](const ActivityResult& result) {
                           if (!result.isCancelled) {
                             const auto& kb = std::get<KeyboardResult>(result.data);
                             const std::string urlToSave =
                                 (kb.text == "https://" || kb.text == "http://") ? "" : kb.text;
                             KOREADER_STORE.setServerUrl(urlToSave);
                             KOREADER_STORE.saveToFile();
                           }
                         });
}

void KOReaderSettingsActivity::toggleDocMatching() {
  const auto current = KOREADER_STORE.getMatchMethod();
  const auto next =
      (current == DocumentMatchMethod::FILENAME) ? DocumentMatchMethod::BINARY : DocumentMatchMethod::FILENAME;
  KOREADER_STORE.setMatchMethod(next);
  KOREADER_STORE.saveToFile();
  requestUpdate();
}

void KOReaderSettingsActivity::handleSelection() {
  const bool hasCreds = KOREADER_STORE.hasCredentials();

  if (hasCreds) {
    switch (selectedIndex) {
      case HAS_CREDS_USERNAME:
        break;
      case HAS_CREDS_SERVER_URL:
        editServerUrl();
        break;
      case HAS_CREDS_DOC_MATCHING:
        toggleDocMatching();
        break;
      case HAS_CREDS_DISCONNECT:
        KOREADER_STORE.clearCredentials();
        selectedIndex = 0;
        requestUpdate();
        break;
      case HAS_CREDS_TEST:
        startActivityForResult(std::make_unique<KOReaderAuthActivity>(renderer, mappedInput),
                               [](const ActivityResult&) {});
        break;
    }
  } else {
    switch (selectedIndex) {
      case NO_CREDS_SERVER_URL:
        editServerUrl();
        break;
      case NO_CREDS_DOC_MATCHING:
        toggleDocMatching();
        break;
      case NO_CREDS_REGISTER:
        startRegisterFlow();
        break;
      case NO_CREDS_LOGIN:
        startLoginFlow();
        break;
    }
  }
}

void KOReaderSettingsActivity::startRegisterFlow() {
  startActivityForResult(std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_KOREADER_USERNAME), "",
                                                                 64, InputType::Text),
                         [this](const ActivityResult& r1) {
                           if (r1.isCancelled) return;
                           pendingUsername = std::get<KeyboardResult>(r1.data).text;
                           startActivityForResult(
                               std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_KOREADER_PASSWORD),
                                                                       "", 64, InputType::Password),
                               [this](const ActivityResult& r2) {
                                 if (r2.isCancelled) return;
                                 pendingPassword = std::get<KeyboardResult>(r2.data).text;

                                 if (WiFi.status() == WL_CONNECTED) {
                                   performRegister();
                                   return;
                                 }
                                 startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                                                        [this](const ActivityResult& wifiResult) {
                                                          if (wifiResult.isCancelled) {
                                                            showResult(tr(STR_WIFI_CONN_FAILED), false, false);
                                                            return;
                                                          }
                                                          performRegister();
                                                        });
                               });
                         });
}

void KOReaderSettingsActivity::startLoginFlow() {
  const std::string existingUser = KOREADER_STORE.getUsername();
  startActivityForResult(std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_KOREADER_USERNAME),
                                                                 existingUser, 64, InputType::Text),
                         [this](const ActivityResult& r1) {
                           if (r1.isCancelled) return;
                           pendingUsername = std::get<KeyboardResult>(r1.data).text;
                           startActivityForResult(
                               std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_KOREADER_PASSWORD),
                                                                       "", 64, InputType::Password),
                               [this](const ActivityResult& r2) {
                                 if (r2.isCancelled) return;
                                 pendingPassword = std::get<KeyboardResult>(r2.data).text;

                                 if (WiFi.status() == WL_CONNECTED) {
                                   performLogin();
                                   return;
                                 }
                                 startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                                                        [this](const ActivityResult& wifiResult) {
                                                          if (wifiResult.isCancelled) {
                                                            showResult(tr(STR_WIFI_CONN_FAILED), false, false);
                                                            return;
                                                          }
                                                          performLogin();
                                                        });
                               });
                         });
}

void KOReaderSettingsActivity::performRegister() {
  KOREADER_STORE.setCredentials(pendingUsername, pendingPassword);
  const std::string md5pass = KOREADER_STORE.getMd5Password();

  const auto error = KOReaderSyncClient::registerUser(pendingUsername, md5pass);
  if (error == KOReaderSyncClient::OK) {
    KOREADER_STORE.saveToFile();
    showResult(tr(STR_KOSYNC_REGISTERED_OK), true, true);
  } else {
    KOREADER_STORE.clearCredentials();
    showResult(KOReaderSyncClient::errorString(error), false, true);
  }
}

void KOReaderSettingsActivity::performLogin() {
  KOREADER_STORE.setCredentials(pendingUsername, pendingPassword);

  const auto error = KOReaderSyncClient::authenticate();
  if (error == KOReaderSyncClient::OK) {
    KOREADER_STORE.saveToFile();
    showResult(tr(STR_KOSYNC_LOGGED_IN_OK), true, true);
  } else {
    KOREADER_STORE.clearCredentials();
    showResult(KOReaderSyncClient::errorString(error), false, true);
  }
}

void KOReaderSettingsActivity::showResult(const char* message, bool success, bool wifiUsed) {
  resultMessage = message;
  resultSuccess = success;
  needsWifiCleanup = wifiUsed;
  pageState = PageState::RESULT;
  requestUpdate();
}

void KOReaderSettingsActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_KOREADER_SYNC));

  if (pageState == PageState::RESULT) {
    const auto fontId = UI_10_FONT_ID;
    const auto lineH = renderer.getLineHeight(fontId);
    const auto top = (pageHeight - lineH * 2 - 10) / 2;
    if (resultSuccess) {
      renderer.drawCenteredText(fontId, top, tr(STR_AUTH_SUCCESS), true, EpdFontFamily::BOLD);
    } else {
      renderer.drawCenteredText(fontId, top, tr(STR_AUTH_FAILED), true, EpdFontFamily::BOLD);
    }
    renderer.drawCenteredText(fontId, top + lineH + 10, resultMessage.c_str());
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_DONE), "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  const bool hasCreds = KOREADER_STORE.hasCredentials();
  const int count = hasCreds ? HAS_CREDS_COUNT : NO_CREDS_COUNT;

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;

  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, contentHeight}, count, static_cast<int>(selectedIndex),
      [hasCreds](int index) -> std::string {
        if (hasCreds) {
          switch (index) {
            case HAS_CREDS_USERNAME:
              return std::string(tr(STR_USERNAME));
            case HAS_CREDS_SERVER_URL:
              return std::string(tr(STR_SYNC_SERVER_URL));
            case HAS_CREDS_DOC_MATCHING:
              return std::string(tr(STR_DOCUMENT_MATCHING));
            case HAS_CREDS_DISCONNECT:
              return std::string(tr(STR_KOSYNC_DISCONNECT));
            case HAS_CREDS_TEST:
              return std::string(tr(STR_KOSYNC_TEST_CONNECTION));
          }
        } else {
          switch (index) {
            case NO_CREDS_SERVER_URL:
              return std::string(tr(STR_SYNC_SERVER_URL));
            case NO_CREDS_DOC_MATCHING:
              return std::string(tr(STR_DOCUMENT_MATCHING));
            case NO_CREDS_REGISTER:
              return std::string(tr(STR_KOSYNC_REGISTER));
            case NO_CREDS_LOGIN:
              return std::string(tr(STR_KOSYNC_LOGIN));
          }
        }
        return "";
      },
      nullptr, nullptr,
      [hasCreds](int index) -> std::string {
        if (hasCreds) {
          if (index == HAS_CREDS_USERNAME) return KOREADER_STORE.getUsername();
          if (index == HAS_CREDS_SERVER_URL) {
            const auto url = KOREADER_STORE.getServerUrl();
            return url.empty() ? std::string(tr(STR_DEFAULT_VALUE)) : url;
          }
          if (index == HAS_CREDS_DOC_MATCHING) {
            return KOREADER_STORE.getMatchMethod() == DocumentMatchMethod::FILENAME ? std::string(tr(STR_FILENAME))
                                                                                    : std::string(tr(STR_BINARY));
          }
        } else {
          if (index == NO_CREDS_SERVER_URL) {
            const auto url = KOREADER_STORE.getServerUrl();
            return url.empty() ? std::string(tr(STR_DEFAULT_VALUE)) : url;
          }
          if (index == NO_CREDS_DOC_MATCHING) {
            return KOREADER_STORE.getMatchMethod() == DocumentMatchMethod::FILENAME ? std::string(tr(STR_FILENAME))
                                                                                    : std::string(tr(STR_BINARY));
          }
        }
        return "";
      },
      true);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

#include "CloudLibrarySettingsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <cstring>
#include <string>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr int FIELD_COUNT = 4;
constexpr int FIELD_URL = 0;
constexpr int FIELD_USERNAME = 1;
constexpr int FIELD_PASSWORD = 2;
constexpr int FIELD_ROOT_PATH = 3;

constexpr StrId FIELD_LABELS[FIELD_COUNT] = {StrId::STR_CLOUD_WEBDAV_URL, StrId::STR_USERNAME, StrId::STR_PASSWORD,
                                             StrId::STR_CLOUD_ROOT_PATH};

// Maps a row to the CrossPointSettings buffer it edits, so the read and write
// paths can never drift apart.
char* fieldBuffer(const int index, size_t& outSize) {
  switch (index) {
    case FIELD_URL:
      outSize = sizeof(SETTINGS.cloudWebdavUrl);
      return SETTINGS.cloudWebdavUrl;
    case FIELD_USERNAME:
      outSize = sizeof(SETTINGS.cloudWebdavUsername);
      return SETTINGS.cloudWebdavUsername;
    case FIELD_PASSWORD:
      outSize = sizeof(SETTINGS.cloudWebdavPassword);
      return SETTINGS.cloudWebdavPassword;
    case FIELD_ROOT_PATH:
    default:
      outSize = sizeof(SETTINGS.cloudWebdavRootPath);
      return SETTINGS.cloudWebdavRootPath;
  }
}

InputType inputTypeForField(const int index) {
  if (index == FIELD_URL) return InputType::Url;
  if (index == FIELD_PASSWORD) return InputType::Password;
  return InputType::Text;
}
}  // namespace

void CloudLibrarySettingsActivity::onEnter() {
  Activity::onEnter();
  selectedIndex = 0;
  requestUpdate();
}

void CloudLibrarySettingsActivity::onExit() { Activity::onExit(); }

void CloudLibrarySettingsActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    handleSelection();
    return;
  }

  buttonNavigator.onNext([this] {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, FIELD_COUNT);
    requestUpdate();
  });
  buttonNavigator.onPrevious([this] {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, FIELD_COUNT);
    requestUpdate();
  });
}

void CloudLibrarySettingsActivity::handleSelection() { editField(selectedIndex); }

void CloudLibrarySettingsActivity::editField(const int index) {
  size_t bufferSize = 0;
  const char* current = fieldBuffer(index, bufferSize);

  std::string prefill = current;
  if (index == FIELD_URL && prefill.empty()) prefill = "https://";
  if (index == FIELD_ROOT_PATH && prefill.empty()) prefill = "/";

  startActivityForResult(std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, I18N.get(FIELD_LABELS[index]),
                                                                 prefill, bufferSize - 1, inputTypeForField(index)),
                         [this, index, bufferSize](const ActivityResult& result) {
                           if (result.isCancelled) return;

                           std::string value = std::get<KeyboardResult>(result.data).text;
                           if (index == FIELD_URL && (value == "https://" || value == "http://")) value.clear();
                           if (index == FIELD_ROOT_PATH && value.empty()) value = "/";

                           size_t destSize = 0;
                           char* dest = fieldBuffer(index, destSize);
                           strncpy(dest, value.c_str(), destSize - 1);
                           dest[destSize - 1] = '\0';
                           SETTINGS.saveToFile();
                           requestUpdate();
                         });
}

void CloudLibrarySettingsActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_CLOUD_LIBRARY));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;

  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, contentHeight}, FIELD_COUNT, selectedIndex,
      [](int index) { return std::string(I18N.get(FIELD_LABELS[index])); }, nullptr, nullptr,
      [](int index) {
        size_t size = 0;
        const char* value = fieldBuffer(index, size);
        if (value[0] == '\0') return std::string(tr(STR_NOT_SET));
        return index == FIELD_PASSWORD ? std::string("******") : std::string(value);
      },
      true);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

#include "CalendarSettingsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <cstring>
#include <string>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

void CalendarSettingsActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();
}

void CalendarSettingsActivity::onExit() { Activity::onExit(); }

void CalendarSettingsActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    handleSelection();
    return;
  }
  buttonNavigator.onNext([this] { requestUpdate(); });
  buttonNavigator.onPrevious([this] { requestUpdate(); });
}

void CalendarSettingsActivity::handleSelection() {
  const std::string current = SETTINGS.calendarUrl;
  const std::string prefill = current.empty() ? "https://" : current;
  startActivityForResult(std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_CALENDAR_URL), prefill,
                                                                 127, InputType::Url),
                         [this](const ActivityResult& result) {
                           if (!result.isCancelled) {
                             const auto& kb = std::get<KeyboardResult>(result.data);
                             const std::string urlToSave =
                                 (kb.text == "https://" || kb.text == "http://") ? "" : kb.text;
                             strncpy(SETTINGS.calendarUrl, urlToSave.c_str(), sizeof(SETTINGS.calendarUrl) - 1);
                             SETTINGS.calendarUrl[sizeof(SETTINGS.calendarUrl) - 1] = '\0';
                             SETTINGS.saveToFile();
                             requestUpdate();
                           }
                         });
}

void CalendarSettingsActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_CALENDAR));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;

  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, contentHeight}, 1, 0,
      [](int) { return std::string(I18N.get(StrId::STR_CALENDAR_URL)); }, nullptr, nullptr,
      [](int) {
        const char* url = SETTINGS.calendarUrl;
        return std::string(url[0] ? url : tr(STR_NOT_SET));
      },
      true);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

#include "DictionaryActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <StarDictionary.h>

#include <cstring>

#include "CrossPointSettings.h"
#include "DictionaryLookupActivity.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

void DictionaryActivity::onEnter() {
  Activity::onEnter();
  dictionaries = StarDict::discoverDictionaries();
  selectedIndex = 0;
  requestUpdate();
}

void DictionaryActivity::onExit() { Activity::onExit(); }

void DictionaryActivity::selectCurrent() {
  if (selectedIndex < 0 || selectedIndex >= static_cast<int>(dictionaries.size())) return;
  const StarDictInfo& info = dictionaries[selectedIndex];

  building = true;
  requestUpdateAndWait();

  StarDictionary dict;
  const bool ok = dict.buildCacheIfMissing(info) && dict.open(info);
  dict.close();
  building = false;

  if (!ok) {
    // Leave the list up; the build error was logged. A future retry (e.g. after
    // freeing SD space) will attempt the build again since no cache file was left behind.
    requestUpdate(true);
    return;
  }

  strncpy(SETTINGS.dictionaryActive, info.id().c_str(), sizeof(SETTINGS.dictionaryActive) - 1);
  SETTINGS.dictionaryActive[sizeof(SETTINGS.dictionaryActive) - 1] = '\0';
  SETTINGS.saveToFile();

  if (openLookupAfterSelect) {
    startActivityForResult(std::make_unique<DictionaryLookupActivity>(renderer, mappedInput),
                           [this](const ActivityResult&) { finish(); });
  } else {
    finish();
  }
}

void DictionaryActivity::loop() {
  if (building) return;

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (dictionaries.empty()) return;

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    selectCurrent();
    return;
  }

  buttonNavigator.onNext([this] {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, static_cast<int>(dictionaries.size()));
    requestUpdate();
  });
  buttonNavigator.onPrevious([this] {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, static_cast<int>(dictionaries.size()));
    requestUpdate();
  });
}

void DictionaryActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_DICTIONARY));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;

  if (building) {
    renderer.drawCenteredText(UI_10_FONT_ID, contentTop + contentHeight / 2, tr(STR_DICT_BUILDING_CACHE));
  } else if (dictionaries.empty()) {
    renderer.drawCenteredText(UI_10_FONT_ID, contentTop + contentHeight / 2, tr(STR_DICT_NO_DICTIONARIES));
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  } else {
    GUI.drawList(renderer, Rect{0, contentTop, pageWidth, contentHeight}, static_cast<int>(dictionaries.size()),
                 selectedIndex, [this](int index) { return dictionaries[index].bookname; });

    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }

  renderer.displayBuffer();
}

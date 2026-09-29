#include "DictionarySettingsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <StarDictInfo.h>

#include <algorithm>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "activities/apps/DictionaryActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

std::string DictionarySettingsActivity::activeDictionaryLabel() {
  if (SETTINGS.dictionaryActive[0] == '\0') return std::string(tr(STR_NOT_SET));

  const auto dictionaries = StarDict::discoverDictionaries();
  const auto it = std::find_if(dictionaries.begin(), dictionaries.end(),
                               [](const StarDictInfo& info) { return info.id() == SETTINGS.dictionaryActive; });
  return it != dictionaries.end() ? it->bookname : std::string(tr(STR_NOT_SET));
}

StrId DictionarySettingsActivity::textSizeLabel() {
  switch (SETTINGS.dictDefinitionTextSize) {
    case CrossPointSettings::DICT_TEXT_SMALL:
      return StrId::STR_SMALL;
    case CrossPointSettings::DICT_TEXT_LARGE:
      return StrId::STR_LARGE;
    default:
      return StrId::STR_MEDIUM;
  }
}

void DictionarySettingsActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();
}

void DictionarySettingsActivity::onExit() { Activity::onExit(); }

void DictionarySettingsActivity::handleSelection() {
  if (selectedIndex == Row::ActiveDictionary) {
    startActivityForResult(std::make_unique<DictionaryActivity>(renderer, mappedInput, /*openLookupAfterSelect=*/false),
                           [this](const ActivityResult&) { requestUpdate(true); });
  } else if (selectedIndex == Row::TextSize) {
    SETTINGS.dictDefinitionTextSize = (SETTINGS.dictDefinitionTextSize + 1) % CrossPointSettings::DICT_TEXT_SIZE_COUNT;
    SETTINGS.saveToFile();
    requestUpdate();
  }
}

void DictionarySettingsActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    handleSelection();
    return;
  }
  buttonNavigator.onNext([this] {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, Row::ROW_COUNT);
    requestUpdate();
  });
  buttonNavigator.onPrevious([this] {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, Row::ROW_COUNT);
    requestUpdate();
  });
}

void DictionarySettingsActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_DICTIONARY));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;

  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, contentHeight}, Row::ROW_COUNT, selectedIndex,
      [](int index) {
        return index == Row::ActiveDictionary ? std::string(I18N.get(StrId::STR_DICT_ACTIVE))
                                              : std::string(I18N.get(StrId::STR_DICT_TEXT_SIZE));
      },
      nullptr, nullptr,
      [](int index) {
        return index == Row::ActiveDictionary ? activeDictionaryLabel() : std::string(I18N.get(textSizeLabel()));
      },
      true);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

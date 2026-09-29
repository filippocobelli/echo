#include "DictionaryLookupActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <iterator>

#include "CrossPointSettings.h"
#include "DictHistoryStore.h"
#include "MappedInputManager.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
int definitionFontId() {
  switch (SETTINGS.dictDefinitionTextSize) {
    case CrossPointSettings::DICT_TEXT_SMALL:
      return SMALL_FONT_ID;
    case CrossPointSettings::DICT_TEXT_LARGE:
      return UI_12_FONT_ID;
    default:
      return UI_10_FONT_ID;
  }
}
}  // namespace

void DictionaryLookupActivity::onEnter() {
  Activity::onEnter();

  hasActiveDictionary = false;
  if (SETTINGS.dictionaryActive[0] != '\0') {
    const auto dictionaries = StarDict::discoverDictionaries();
    const auto it = std::find_if(dictionaries.begin(), dictionaries.end(),
                                 [](const StarDictInfo& info) { return info.id() == SETTINGS.dictionaryActive; });
    if (it != dictionaries.end()) {
      activeInfo = *it;
      hasActiveDictionary = dictionary.buildCacheIfMissing(*it) && dictionary.open(*it);
    }
  }

  reloadHistoryList();
  requestUpdate();
}

void DictionaryLookupActivity::onExit() {
  Activity::onExit();
  dictionary.close();
}

void DictionaryLookupActivity::reloadHistoryList() {
  listWords.clear();
  showingSuggestions = false;
  const auto& historyEntries = DICT_HISTORY.getEntries();
  listWords.reserve(historyEntries.size());
  std::transform(historyEntries.begin(), historyEntries.end(), std::back_inserter(listWords),
                 [](const auto& entry) { return entry.word; });
  selectedIndex = 0;
}

void DictionaryLookupActivity::openSearchKeyboard() {
  startActivityForResult(
      std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_DICT_LOOKUP), "", 63, InputType::Text),
      [this](const ActivityResult& result) {
        if (!result.isCancelled) {
          const auto& kb = std::get<KeyboardResult>(result.data);
          onSearchComplete(kb.text);
        }
        requestUpdate(true);
      });
}

void DictionaryLookupActivity::onSearchComplete(const std::string& query) {
  if (query.empty() || !hasActiveDictionary) return;

  listWords = dictionary.suggest(query, 30);
  showingSuggestions = true;
  selectedIndex = 0;

  if (listWords.size() == 1) {
    // Single exact-ish match: skip straight to the definition.
    openDefinitionFor(listWords[0]);
  }
}

void DictionaryLookupActivity::openDefinitionFor(const std::string& word) {
  currentWord = word;
  std::string definitionText;
  definitionFound = hasActiveDictionary && dictionary.lookup(word, definitionText);

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int contentWidth = renderer.getScreenWidth() - metrics.contentSidePadding * 2;
  const int fontId = definitionFontId();

  definitionLines = definitionFound ? renderer.wrappedText(fontId, definitionText.c_str(), contentWidth, 500)
                                    : std::vector<std::string>{tr(STR_DICT_NOT_FOUND)};

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentBottom = renderer.getScreenHeight() - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const int lineHeight = renderer.getTextHeight(fontId) + 4;
  definitionLinesPerPage = std::max(1, (contentBottom - contentTop) / lineHeight);
  definitionPage = 0;

  if (definitionFound && hasActiveDictionary) {
    DICT_HISTORY.addOrPromote(word, activeInfo.id());
  }

  inDefinitionView = true;
  requestUpdate(true);
}

void DictionaryLookupActivity::backFromDefinition() {
  inDefinitionView = false;
  reloadHistoryList();
  requestUpdate();
}

void DictionaryLookupActivity::loop() {
  if (!hasActiveDictionary) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
      finish();
    }
    return;
  }

  if (inDefinitionView) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
      backFromDefinition();
      return;
    }
    const int pageCount =
        std::max(1, (static_cast<int>(definitionLines.size()) + definitionLinesPerPage - 1) / definitionLinesPerPage);
    buttonNavigator.onNext([this, pageCount] {
      if (definitionPage + 1 < pageCount) {
        definitionPage++;
        requestUpdate();
      }
    });
    buttonNavigator.onPrevious([this] {
      if (definitionPage > 0) {
        definitionPage--;
        requestUpdate();
      }
    });
    return;
  }

  // Word-list view (history or suggestions).
  if (!listWords.empty() && !longPressSearchHandled && mappedInput.isPressed(MappedInputManager::Button::Confirm) &&
      mappedInput.getHeldTime() >= LONG_PRESS_MS) {
    longPressSearchHandled = true;
    openSearchKeyboard();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (longPressSearchHandled) {
      longPressSearchHandled = false;
      return;
    }
    if (listWords.empty()) {
      openSearchKeyboard();
    } else {
      openDefinitionFor(listWords[selectedIndex]);
    }
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (!listWords.empty()) {
    buttonNavigator.onNext([this] {
      selectedIndex = ButtonNavigator::nextIndex(selectedIndex, static_cast<int>(listWords.size()));
      requestUpdate();
    });
    buttonNavigator.onPrevious([this] {
      selectedIndex = ButtonNavigator::previousIndex(selectedIndex, static_cast<int>(listWords.size()));
      requestUpdate();
    });
  }
}

void DictionaryLookupActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  if (!hasActiveDictionary) {
    GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_DICT_LOOKUP));
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, tr(STR_DICT_NO_ACTIVE));
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;

  if (inDefinitionView) {
    GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, currentWord.c_str());

    const int fontId = definitionFontId();
    const int lineHeight = renderer.getTextHeight(fontId) + 4;
    const int startLine = definitionPage * definitionLinesPerPage;
    int y = contentTop;
    for (int i = startLine; i < static_cast<int>(definitionLines.size()) && i < startLine + definitionLinesPerPage;
         ++i) {
      renderer.drawText(fontId, metrics.contentSidePadding, y, definitionLines[i].c_str());
      y += lineHeight;
    }

    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", tr(STR_DIR_UP), tr(STR_DIR_DOWN));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  } else {
    GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight},
                   I18N.get(showingSuggestions ? StrId::STR_DICT_LOOKUP : StrId::STR_DICT_HISTORY));

    if (listWords.empty()) {
      renderer.drawCenteredText(UI_10_FONT_ID, contentTop + contentHeight / 2,
                                I18N.get(showingSuggestions ? StrId::STR_DICT_NO_RESULTS : StrId::STR_DICT_SEARCH));
    } else {
      GUI.drawList(renderer, Rect{0, contentTop, pageWidth, contentHeight}, static_cast<int>(listWords.size()),
                   selectedIndex, [this](int index) { return listWords[index]; });
    }

    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_DICT_SEARCH), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }

  renderer.displayBuffer();
}

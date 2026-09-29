#pragma once

#include <StarDictInfo.h>
#include <StarDictionary.h>

#include <string>
#include <vector>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

// In-reader "Cerca parola" / Apps > Dictionary lookup screen for the currently
// active dictionary (SETTINGS.dictionaryActive). Shows recent-lookup history
// until the user searches; short Confirm on a word opens its definition, a
// long Confirm press opens the on-screen keyboard for a new search.
//
// v1 simplification: there's no word-under-cursor selection in the reader, so
// entry always starts from an empty search box, and switching the active
// dictionary happens via Settings > System > Dictionary or Apps > Dictionary,
// not from within this screen.
class DictionaryLookupActivity final : public Activity {
 public:
  explicit DictionaryLookupActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("DictionaryLookup", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  bool hasActiveDictionary = false;
  StarDictInfo activeInfo;
  StarDictionary dictionary;

  bool inDefinitionView = false;
  std::vector<std::string> listWords;  // history or suggestions, depending on showingSuggestions
  bool showingSuggestions = false;
  int selectedIndex = 0;

  std::string currentWord;
  std::vector<std::string> definitionLines;
  int definitionPage = 0;
  int definitionLinesPerPage = 1;
  bool definitionFound = false;

  bool longPressSearchHandled = false;
  static constexpr uint16_t LONG_PRESS_MS = 600;

  ButtonNavigator buttonNavigator;

  void reloadHistoryList();
  void openSearchKeyboard();
  void onSearchComplete(const std::string& query);
  void openDefinitionFor(const std::string& word);
  void backFromDefinition();
};

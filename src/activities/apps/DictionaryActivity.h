#pragma once

#include <StarDictInfo.h>

#include <string>
#include <vector>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

// Apps > Dictionary: lists StarDict dictionaries found on the SD card
// (/dictionaries/<lang>/*.ifo). Selecting one builds its lookup cache if
// missing, sets it as the active dictionary, and (when opened as a standalone
// app) opens the lookup screen for it. Also reused by DictionarySettingsActivity
// as a picker-only screen (openLookupAfterSelect = false).
class DictionaryActivity final : public Activity {
 public:
  explicit DictionaryActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, bool openLookupAfterSelect = true)
      : Activity("Dictionary", renderer, mappedInput), openLookupAfterSelect(openLookupAfterSelect) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  bool openLookupAfterSelect;
  std::vector<StarDictInfo> dictionaries;
  int selectedIndex = 0;
  bool building = false;

  ButtonNavigator buttonNavigator;

  void selectCurrent();
};

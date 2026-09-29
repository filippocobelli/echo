#pragma once

#include <I18n.h>

#include <string>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

// Settings > System > Dictionary: shows the active dictionary (tap to reopen
// the picker) and lets the user cycle the definition text size.
class DictionarySettingsActivity final : public Activity {
 public:
  explicit DictionarySettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("DictionarySettings", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum Row { ActiveDictionary = 0, TextSize = 1, ROW_COUNT = 2 };
  int selectedIndex = 0;
  ButtonNavigator buttonNavigator;

  void handleSelection();
  static std::string activeDictionaryLabel();
  static StrId textSizeLabel();
};

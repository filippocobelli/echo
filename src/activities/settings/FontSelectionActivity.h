#pragma once

#include <SdCardFontRegistry.h>

#include <string>
#include <vector>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class FontSelectionActivity final : public Activity {
 public:
  explicit FontSelectionActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                 const SdCardFontRegistry* registry);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  void handleSelection();
  // Position in fonts_ of the family currently in use, for both the initial cursor
  // and the "Selected" marker.
  int listPositionForCurrentSelection() const;

  struct FontEntry {
    std::string name;
    bool isBuiltin;
    // For built-in entries: the stored CrossPointSettings::FONT_FAMILY value.
    // For SD entries: the index into SdCardFontRegistry::getFamilies().
    // Neither is the position in fonts_ — those diverge when a built-in family is
    // omitted from the firmware (see OMIT_LEXENDDECA_FONT).
    uint8_t settingIndex;
  };

  const SdCardFontRegistry* registry_;
  ButtonNavigator buttonNavigator_;
  std::vector<FontEntry> fonts_;
  int selectedIndex_ = 0;
};

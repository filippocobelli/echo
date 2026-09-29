#pragma once

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class CalendarSettingsActivity final : public Activity {
 public:
  explicit CalendarSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("CalendarSettings", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  ButtonNavigator buttonNavigator;
  void handleSelection();
};

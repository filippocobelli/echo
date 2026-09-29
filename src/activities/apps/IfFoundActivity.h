#pragma once

#include <string>
#include <vector>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class IfFoundActivity final : public Activity {
 public:
  explicit IfFoundActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("IfFound", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  std::vector<std::string> lines;
  int scrollOffset = 0;
  int visibleLines = 0;

  ButtonNavigator buttonNavigator;

  void loadText();
};

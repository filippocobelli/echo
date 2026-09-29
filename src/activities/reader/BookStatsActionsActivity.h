#pragma once

#include <I18n.h>

#include <vector>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

// Small action menu launched via long-press Confirm from BookStatsActivity:
// Correct time / Modify start date / Reset stats. Selection is returned via
// MenuResult{action} (reusing the generic reader-menu result shape).
class BookStatsActionsActivity final : public Activity {
 public:
  enum class Action { CorrectTime = 0, ModifyStartDate = 1, ResetStats = 2 };

  explicit BookStatsActionsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("BookStatsActions", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  struct MenuItem {
    Action action;
    StrId labelId;
  };
  const std::vector<MenuItem> items = {
      {Action::CorrectTime, StrId::STR_STATS_CORRECT_TIME},
      {Action::ModifyStartDate, StrId::STR_STATS_MODIFY_START},
      {Action::ResetStats, StrId::STR_STATS_RESET_BOOK},
  };
  int selectedIndex = 0;
  ButtonNavigator buttonNavigator;
};

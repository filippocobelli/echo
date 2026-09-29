#pragma once

#include <I18n.h>

#include <string>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

// AZIONE A screen: Add/Subtract toggle -> quick-pick minutes (15/30/45/60) ->
// day picker (reusing IntervalSelectionActivity as a "N days ago" stepper,
// since no calendar-grid date picker exists in this codebase). Applies the
// correction via BookActions::correctBookReadingTime on completion.
class StatsTimeCorrectionActivity final : public Activity {
 public:
  StatsTimeCorrectionActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string cachePath)
      : Activity("StatsTimeCorrection", renderer, mappedInput), cachePath(std::move(cachePath)) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class Step { AddSubtract, Amount };
  static constexpr uint16_t kAmounts[4] = {15, 30, 45, 60};
  static constexpr StrId kAmountLabels[4] = {StrId::STR_STATS_MIN_15, StrId::STR_STATS_MIN_30, StrId::STR_STATS_MIN_45,
                                             StrId::STR_STATS_MIN_60};

  std::string cachePath;
  Step step = Step::AddSubtract;
  int addSubtractIndex = 0;  // 0 = Add, 1 = Subtract
  int amountIndex = 0;
  bool applied = false;

  ButtonNavigator buttonNavigator;

  void openDayPicker();
};

#pragma once
#include <string>

#include "../Activity.h"
#include "BookReadingStats.h"
#include "GlobalReadingStats.h"

class BookStatsActivity final : public Activity {
  std::string bookTitle;
  std::string cachePath;  // empty = read-only, no correction/reset actions offered
  BookReadingStats stats;
  GlobalReadingStats globalStats;
  GlobalReadingStats allDevicesStats;
  bool showAllDevicesStats = false;
  bool longPressActionsHandled = false;
  static constexpr uint16_t LONG_PRESS_MS = 600;

  void openActionsMenu();
  void reloadStatsFromDisk();

 public:
  BookStatsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::string& title,
                    const BookReadingStats& stats, const GlobalReadingStats& globalStats, std::string cachePath = "");
  BookStatsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::string& title,
                    const BookReadingStats& stats, const GlobalReadingStats& globalStats,
                    const GlobalReadingStats& allDevicesStats, std::string cachePath = "");

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool allowPowerAsConfirmInReaderMode() const override { return true; }
};

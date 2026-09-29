#include "BookStatsActivity.h"

#include "BookStatsActionsActivity.h"
#include "BookStatsView.h"
#include "MappedInputManager.h"
#include "StatsTimeCorrectionActivity.h"
#include "activities/home/BookActions.h"
#include "activities/util/ConfirmationActivity.h"
#include "activities/util/IntervalSelectionActivity.h"

BookStatsActivity::BookStatsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::string& title,
                                     const BookReadingStats& stats, const GlobalReadingStats& globalStats,
                                     std::string cachePath)
    : Activity("BookStats", renderer, mappedInput),
      bookTitle(title),
      cachePath(std::move(cachePath)),
      stats(stats),
      globalStats(globalStats) {}

BookStatsActivity::BookStatsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::string& title,
                                     const BookReadingStats& stats, const GlobalReadingStats& globalStats,
                                     const GlobalReadingStats& allDevicesStats, std::string cachePath)
    : Activity("BookStats", renderer, mappedInput),
      bookTitle(title),
      cachePath(std::move(cachePath)),
      stats(stats),
      globalStats(globalStats),
      allDevicesStats(allDevicesStats),
      showAllDevicesStats(true) {}

void BookStatsActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();
}

void BookStatsActivity::reloadStatsFromDisk() {
  if (cachePath.empty()) return;
  stats = BookReadingStats::load(cachePath);
  globalStats = GlobalReadingStats::load();
  requestUpdate(true);
}

void BookStatsActivity::openActionsMenu() {
  startActivityForResult(
      std::make_unique<BookStatsActionsActivity>(renderer, mappedInput), [this](const ActivityResult& result) {
        if (result.isCancelled) {
          requestUpdate(true);
          return;
        }
        const auto action = static_cast<BookStatsActionsActivity::Action>(std::get<MenuResult>(result.data).action);
        switch (action) {
          case BookStatsActionsActivity::Action::CorrectTime:
            startActivityForResult(std::make_unique<StatsTimeCorrectionActivity>(renderer, mappedInput, cachePath),
                                   [this](const ActivityResult&) { reloadStatsFromDisk(); });
            return;
          case BookStatsActionsActivity::Action::ModifyStartDate:
            startActivityForResult(std::make_unique<IntervalSelectionActivity>(
                                       renderer, mappedInput, "StatsStartDatePicker", StrId::STR_STATS_MODIFY_START,
                                       StrId::STR_STATS_DAYS_STEP_HINT, 0, 0, 3650, 1, 7, StrId::STR_STATS_DAYS_AGO),
                                   [this](const ActivityResult& dateResult) {
                                     if (!dateResult.isCancelled) {
                                       BookActions::setBookStartDateOverride(
                                           cachePath, std::get<IntervalResult>(dateResult.data).value);
                                     }
                                     reloadStatsFromDisk();
                                   });
            return;
          case BookStatsActionsActivity::Action::ResetStats:
            startActivityForResult(std::make_unique<ConfirmationActivity>(
                                       renderer, mappedInput, tr(STR_STATS_RESET_BOOK), tr(STR_STATS_CONFIRM_RESET)),
                                   [this](const ActivityResult& confirmResult) {
                                     if (!confirmResult.isCancelled) {
                                       BookActions::resetBookStats(cachePath);
                                     }
                                     reloadStatsFromDisk();
                                   });
            return;
        }
      });
}

void BookStatsActivity::loop() {
  if (!cachePath.empty() && !longPressActionsHandled && mappedInput.isPressed(MappedInputManager::Button::Confirm) &&
      mappedInput.getHeldTime() >= LONG_PRESS_MS) {
    longPressActionsHandled = true;
    openActionsMenu();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back) ||
      mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (longPressActionsHandled) {
      longPressActionsHandled = false;
      return;
    }
    finish();
  }
}

void BookStatsActivity::render(RenderLock&&) {
  renderBookStatsView(renderer, &mappedInput, bookTitle, stats, globalStats,
                      showAllDevicesStats ? &allDevicesStats : nullptr, true);
  renderer.displayBuffer();
}

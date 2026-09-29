#include "StatsTimeCorrectionActivity.h"

#include <GfxRenderer.h>

#include "MappedInputManager.h"
#include "activities/home/BookActions.h"
#include "activities/util/IntervalSelectionActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

void StatsTimeCorrectionActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();
}

void StatsTimeCorrectionActivity::openDayPicker() {
  startActivityForResult(
      std::make_unique<IntervalSelectionActivity>(renderer, mappedInput, "StatsDayPicker", StrId::STR_STATS_PICK_DAY,
                                                  StrId::STR_STATS_DAYS_STEP_HINT, 0, 0, 3650, 1, 7,
                                                  StrId::STR_STATS_DAYS_AGO),
      [this](const ActivityResult& result) {
        if (result.isCancelled) {
          requestUpdate(true);
          return;
        }
        const uint32_t daysAgo = std::get<IntervalResult>(result.data).value;
        BookActions::correctBookReadingTime(cachePath, daysAgo, kAmounts[amountIndex], addSubtractIndex == 0);
        applied = true;

        ActivityResult finalResult;
        finalResult.isCancelled = false;
        setResult(std::move(finalResult));
        finish();
      });
}

void StatsTimeCorrectionActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (step == Step::Amount) {
      step = Step::AddSubtract;
      requestUpdate();
      return;
    }
    ActivityResult result;
    result.isCancelled = true;
    setResult(std::move(result));
    finish();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (step == Step::AddSubtract) {
      step = Step::Amount;
      requestUpdate();
    } else {
      openDayPicker();
    }
    return;
  }

  const int itemCount = (step == Step::AddSubtract) ? 2 : 4;
  int& selected = (step == Step::AddSubtract) ? addSubtractIndex : amountIndex;
  buttonNavigator.onNext([this, &selected, itemCount] {
    selected = ButtonNavigator::nextIndex(selected, itemCount);
    requestUpdate();
  });
  buttonNavigator.onPrevious([this, &selected, itemCount] {
    selected = ButtonNavigator::previousIndex(selected, itemCount);
    requestUpdate();
  });
}

void StatsTimeCorrectionActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_STATS_CORRECT_TIME));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  if (step == Step::AddSubtract) {
    GUI.drawList(renderer, Rect{0, contentTop, pageWidth, contentHeight}, 2, addSubtractIndex, [](int index) {
      return std::string(I18N.get(index == 0 ? StrId::STR_STATS_ADD_TIME : StrId::STR_STATS_SUBTRACT_TIME));
    });
  } else {
    GUI.drawList(renderer, Rect{0, contentTop, pageWidth, contentHeight}, 4, amountIndex,
                 [](int index) { return std::string(I18N.get(kAmountLabels[index])); });
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

#include "BookStatsView.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

void renderBookStatsView(GfxRenderer& renderer, const MappedInputManager* mappedInput, const std::string& bookTitle,
                         const BookReadingStats& stats, const GlobalReadingStats& globalStats,
                         const GlobalReadingStats* allDevicesStats, bool showButtonHints) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const bool compactLayout = allDevicesStats != nullptr;
  const int screenWidth = renderer.getScreenWidth();
  const int cardX = metrics.contentSidePadding;
  const int cardW = screenWidth - metrics.contentSidePadding * 2;
  const int thirdW = cardW / 3;
  const int halfW = cardW / 2;

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, screenWidth, metrics.headerHeight}, "");

  const int availableH = metrics.headerHeight - metrics.batteryBarHeight;
  const int titleX = metrics.contentSidePadding;
  const int lineHeight = renderer.getLineHeight(UI_12_FONT_ID);
  const int titleY = metrics.topPadding + metrics.batteryBarHeight + (availableH - lineHeight) / 2;
  const int batteryStartX = screenWidth - metrics.contentSidePadding - metrics.batteryWidth;
  const int maxTitleWidth = batteryStartX - titleX - metrics.contentSidePadding;
  const std::string truncTitle =
      renderer.truncatedText(UI_12_FONT_ID, tr(STR_READING_STATS), maxTitleWidth, EpdFontFamily::BOLD);
  renderer.drawText(UI_12_FONT_ID, titleX, titleY, truncTitle.c_str(), true, EpdFontFamily::BOLD);

  int y = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;

  const int valueLineH = renderer.getLineHeight(UI_12_FONT_ID);
  const int labelLineH = renderer.getLineHeight(SMALL_FONT_ID);
  const int titleLineH = renderer.getLineHeight(UI_10_FONT_ID);

  const int screenHeight = renderer.getScreenHeight();
  const int buttonHintsReserve = showButtonHints ? metrics.buttonHintsHeight : 0;
  const int cardGap = metrics.verticalSpacing;
  int cardTitleH = 40;
  int cellH = 90;
  if (compactLayout) {
    constexpr int cardCount = 3;
    const int availableCardsH =
        screenHeight - y - buttonHintsReserve - metrics.verticalSpacing - cardGap * (cardCount - 1);
    const int minTitleH = titleLineH + 4;
    const int minCellH = valueLineH + labelLineH + 2;
    const int minCardH = minTitleH + minCellH * 2;
    const int cardH = availableCardsH / cardCount > minCardH ? availableCardsH / cardCount : minCardH;
    cardTitleH = cardH >= 160 ? 40 : minTitleH;
    cellH = (cardH - cardTitleH) / 2;
  }
  const int labelPad = compactLayout ? 2 : 6;

  auto drawStatCell = [&](int x, int w, int cellY, const char* value, const char* label) {
    const int totalTextH = valueLineH + labelPad + labelLineH;
    const int textY = cellY + (cellH - totalTextH) / 2;

    const int vw = renderer.getTextWidth(UI_12_FONT_ID, value, EpdFontFamily::BOLD);
    renderer.drawText(UI_12_FONT_ID, x + (w - vw) / 2, textY, value, true, EpdFontFamily::BOLD);

    const int lw = renderer.getTextWidth(SMALL_FONT_ID, label);
    renderer.drawText(SMALL_FONT_ID, x + (w - lw) / 2, textY + valueLineH + labelPad, label, true);
  };

  auto drawCardTitle = [&](int cardY, const char* title) {
    const std::string truncTitle =
        renderer.truncatedText(UI_10_FONT_ID, title, cardW - metrics.contentSidePadding * 2, EpdFontFamily::BOLD);
    const int tw = renderer.getTextWidth(UI_10_FONT_ID, truncTitle.c_str(), EpdFontFamily::BOLD);
    renderer.drawText(UI_10_FONT_ID, cardX + (cardW - tw) / 2, cardY + (cardTitleH - titleLineH) / 2,
                      truncTitle.c_str(), true, EpdFontFamily::BOLD);
  };

  char buf[32];

  renderer.drawRect(cardX, y, cardW, cardTitleH + cellH * 2);
  drawCardTitle(y, bookTitle.c_str());

  y += cardTitleH;
  renderer.drawLine(cardX, y, cardX + cardW, y);

  const int row1Y = y;
  snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(stats.sessionCount));
  drawStatCell(cardX, thirdW, row1Y, buf, tr(STR_STATS_SESSIONS_LBL));

  BookReadingStats::formatDuration(stats.totalReadingSeconds, buf, sizeof(buf));
  drawStatCell(cardX + thirdW, thirdW, row1Y, buf, tr(STR_STATS_TIME_LBL));

  snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(stats.totalPagesTurned));
  drawStatCell(cardX + thirdW * 2, thirdW, row1Y, buf, tr(STR_STATS_PAGES_LBL));

  y += cellH;

  const int row2Y = y;
  const uint32_t avgSecs = stats.sessionCount > 0 ? stats.totalReadingSeconds / stats.sessionCount : 0;
  BookReadingStats::formatDuration(avgSecs, buf, sizeof(buf));
  drawStatCell(cardX, halfW, row2Y, buf, tr(STR_STATS_AVG_SESSION_LBL));

  if (stats.totalReadingSeconds > 60) {
    const float ppm =
        static_cast<float>(stats.totalPagesTurned) * 60.0f / static_cast<float>(stats.totalReadingSeconds);
    snprintf(buf, sizeof(buf), "%.1f", ppm);
  } else {
    snprintf(buf, sizeof(buf), "0.0");
  }
  drawStatCell(cardX + halfW, halfW, row2Y, buf, tr(STR_STATS_PAGES_PER_MIN));

  y += cellH;
  y += cardGap;

  const int card2H = cardTitleH + cellH * 2;
  auto drawGlobalCard = [&](int cardY, const char* title, const GlobalReadingStats& cardStats) {
    renderer.drawRect(cardX, cardY, cardW, card2H);

    drawCardTitle(cardY, title);

    int cardContentY = cardY + cardTitleH;
    renderer.drawLine(cardX, cardContentY, cardX + cardW, cardContentY);

    snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(cardStats.totalSessions));
    drawStatCell(cardX, thirdW, cardContentY, buf, tr(STR_STATS_SESSIONS_LBL));

    BookReadingStats::formatDuration(cardStats.totalReadingSeconds, buf, sizeof(buf));
    drawStatCell(cardX + thirdW, thirdW, cardContentY, buf, tr(STR_STATS_TIME_LBL));

    snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(cardStats.totalPagesTurned));
    drawStatCell(cardX + thirdW * 2, thirdW, cardContentY, buf, tr(STR_STATS_PAGES_LBL));

    cardContentY += cellH;

    if (cardStats.completedBooks > 0) {
      const uint32_t globalAvgSecs =
          cardStats.totalSessions > 0 ? cardStats.totalReadingSeconds / cardStats.totalSessions : 0;
      BookReadingStats::formatDuration(globalAvgSecs, buf, sizeof(buf));
      drawStatCell(cardX, thirdW, cardContentY, buf, tr(STR_STATS_AVG_SESSION_LBL));

      if (cardStats.totalReadingSeconds > 60) {
        const float ppm =
            static_cast<float>(cardStats.totalPagesTurned) * 60.0f / static_cast<float>(cardStats.totalReadingSeconds);
        snprintf(buf, sizeof(buf), "%.1f", ppm);
      } else {
        snprintf(buf, sizeof(buf), "0.0");
      }
      drawStatCell(cardX + thirdW, thirdW, cardContentY, buf, tr(STR_STATS_PAGES_PER_MIN));

      snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(cardStats.completedBooks));
      drawStatCell(cardX + thirdW * 2, thirdW, cardContentY, buf, tr(STR_STATS_COMPLETED_LBL));
    } else {
      const uint32_t globalAvgSecs =
          cardStats.totalSessions > 0 ? cardStats.totalReadingSeconds / cardStats.totalSessions : 0;
      BookReadingStats::formatDuration(globalAvgSecs, buf, sizeof(buf));
      drawStatCell(cardX, halfW, cardContentY, buf, tr(STR_STATS_AVG_SESSION_LBL));

      if (cardStats.totalReadingSeconds > 60) {
        const float ppm =
            static_cast<float>(cardStats.totalPagesTurned) * 60.0f / static_cast<float>(cardStats.totalReadingSeconds);
        snprintf(buf, sizeof(buf), "%.1f", ppm);
      } else {
        snprintf(buf, sizeof(buf), "0.0");
      }
      drawStatCell(cardX + halfW, halfW, cardContentY, buf, tr(STR_STATS_PAGES_PER_MIN));
    }
  };

  if (screenHeight - y - buttonHintsReserve - metrics.verticalSpacing >= card2H || compactLayout) {
    drawGlobalCard(y, allDevicesStats ? tr(STR_STATS_THIS_DEVICE) : tr(STR_STATS_ALL_TIME), globalStats);
    y += card2H;

    if (allDevicesStats) {
      y += cardGap;
      drawGlobalCard(y, tr(STR_STATS_ALL_DEVICES), *allDevicesStats);
    }
  }

  if (showButtonHints && mappedInput) {
    const auto labels = mappedInput->mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4, true);
  }
}

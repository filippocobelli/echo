#include "BootActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <cstdio>

#include "AppVersion.h"
#include "fontIds.h"

static void drawEchoLogoFilled(const GfxRenderer& renderer, int sqX, int sqY) {
  renderer.fillRoundedRect(sqX, sqY, 160, 160, 22, Color::Black);
  renderer.fillRoundedRect(sqX + 30, sqY + 32, 96, 18, 4, Color::White);
  renderer.fillRoundedRect(sqX + 30, sqY + 71, 60, 18, 4, Color::White);
  renderer.fillRoundedRect(sqX + 30, sqY + 110, 96, 18, 4, Color::White);
}

void BootActivity::onEnter() {
  Activity::onEnter();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const int sqX = (pageWidth - 160) / 2;
  const int sqY = (pageHeight - 160) / 2 - 60;

  renderer.clearScreen();
  drawEchoLogoFilled(renderer, sqX, sqY);
  renderer.drawCenteredText(UI_12_FONT_ID, sqY + 180, "ECHO", true, EpdFontFamily::BOLD);
  renderer.drawCenteredText(UI_10_FONT_ID, sqY + 204, "Find your ECHO.");

  // "v1.0.1 · 2026093001": release version plus the build's version code (when the env defines one).
  char versionLine[48];
#ifdef ECHO_VERSION_CODE_STR
  snprintf(versionLine, sizeof(versionLine), "v%s \xC2\xB7 %s", ECHO_VERSION, ECHO_VERSION_CODE_STR);
#else
  snprintf(versionLine, sizeof(versionLine), "v%s", ECHO_VERSION);
#endif
  renderer.drawCenteredText(UI_10_FONT_ID, pageHeight - 30, versionLine);
  renderer.displayBuffer();
}

#include "IfFoundActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include <cstring>
#include <string>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

static constexpr const char* kIfFoundPath = "/if_found.txt";
static constexpr const char* kDefaultText =
    "This e-reader belongs to its owner.\n"
    "If found, please return it.\n"
    "\n"
    "To show a custom message,\n"
    "create /if_found.txt on the SD card.";

static constexpr int kTextFontId = UI_12_FONT_ID;
static constexpr int kLineSpacing = 6;

void IfFoundActivity::onEnter() {
  Activity::onEnter();
  scrollOffset = 0;
  loadText();
  requestUpdate();
}

void IfFoundActivity::onExit() { Activity::onExit(); }

void IfFoundActivity::loadText() {
  lines.clear();

  String raw = Storage.readFile(kIfFoundPath);
  const char* text = (raw.length() > 0) ? raw.c_str() : kDefaultText;

  // Split text on '\n' into input lines, then wrap each at screen width
  const int margin = 20;
  const int maxWidth = renderer.getScreenWidth() - margin * 2;

  const char* p = text;
  while (*p) {
    const char* nl = strchr(p, '\n');
    const size_t len = nl ? static_cast<size_t>(nl - p) : strlen(p);
    std::string inputLine(p, len);

    if (inputLine.empty()) {
      lines.push_back("");
    } else {
      auto wrapped = renderer.wrappedText(kTextFontId, inputLine.c_str(), maxWidth, 999, EpdFontFamily::REGULAR);
      lines.insert(lines.end(), wrapped.begin(), wrapped.end());
    }

    p += len + (nl ? 1 : 0);
    if (!nl) break;
  }
}

void IfFoundActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  buttonNavigator.onNext([this] {
    if (scrollOffset + visibleLines < static_cast<int>(lines.size())) {
      scrollOffset++;
      requestUpdate();
    }
  });
  buttonNavigator.onPrevious([this] {
    if (scrollOffset > 0) {
      scrollOffset--;
      requestUpdate();
    }
  });
}

void IfFoundActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_IF_FOUND));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentBottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const int lineHeight = renderer.getTextHeight(kTextFontId) + kLineSpacing;

  visibleLines = std::max(1, (contentBottom - contentTop) / lineHeight);

  const int margin = 20;
  int y = contentTop;
  for (int i = scrollOffset; i < static_cast<int>(lines.size()) && i < scrollOffset + visibleLines; ++i) {
    if (!lines[i].empty()) {
      renderer.drawText(kTextFontId, margin, y, lines[i].c_str(), true, EpdFontFamily::BOLD);
    }
    y += lineHeight;
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

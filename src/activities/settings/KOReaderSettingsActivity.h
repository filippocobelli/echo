#pragma once

#include <string>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class KOReaderSettingsActivity final : public Activity {
 public:
  explicit KOReaderSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("KOReaderSettings", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class PageState { MAIN, RESULT };

  ButtonNavigator buttonNavigator;
  size_t selectedIndex = 0;
  PageState pageState = PageState::MAIN;
  std::string resultMessage;
  bool resultSuccess = false;
  bool needsWifiCleanup = false;
  std::string pendingUsername;
  std::string pendingPassword;

  int menuItemCount() const;
  void handleSelection();
  void startRegisterFlow();
  void startLoginFlow();
  void performRegister();
  void performLogin();
  void showResult(const char* message, bool success, bool wifiUsed);
  void editServerUrl();
  void toggleDocMatching();
};

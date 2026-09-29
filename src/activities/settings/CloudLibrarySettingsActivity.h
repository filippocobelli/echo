#pragma once

#include <cstddef>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

/**
 * Edit screen for the Cloud Library WebDAV account.
 * Fields: WebDAV URL, Username, Password, Root Folder.
 * Every field is written straight into CrossPointSettings and saved on edit,
 * so a half-configured account survives a reboot.
 */
class CloudLibrarySettingsActivity final : public Activity {
 public:
  explicit CloudLibrarySettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("CloudLibrarySettings", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  ButtonNavigator buttonNavigator;
  int selectedIndex = 0;

  void handleSelection();
  void editField(int index);
};

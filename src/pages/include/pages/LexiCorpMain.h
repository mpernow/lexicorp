#pragma once

#include <Wt/WApplication.h>
#include <Wt/WCheckBox.h>
#include <Wt/WComboBox.h>
#include <Wt/WJavaScript.h>
#include <db/DatabaseHandler.h>
#include <db/WordRepository.h>
#include <pages/AppContext.h>
#include <utils/Language.h>

class LexiCorpMain : public Wt::WApplication {
public:
  LexiCorpMain(const Wt::WEnvironment &env);
  void updateLanguageDisplay();

private:
  void handlePath(const std::string &path);
  // Viewport, favicon and web app manifest tags in <head>.
  void addHeadTags();
  // Sets Bootstrap's colour mode (data-bs-theme on <html>) and the switch.
  void applyTheme(bool dark);

  std::shared_ptr<AppContext> mAppContext;
  Wt::WComboBox *mLanguageCombo;
  Wt::WCheckBox *mDarkModeSwitch;
  // Emitted from the browser with its prefers-color-scheme setting; only used
  // while the user hasn't picked a theme themselves.
  Wt::JSignal<bool> mSystemPrefersDark{this, "systemPrefersDark"};
  static constexpr const char *languageCookieName = "language";
  static constexpr const char *themeCookieName = "theme";
  static constexpr std::array<utils::Language, 3> languages{
      utils::Language::English, utils::Language::French,
      utils::Language::Kurdish};
};

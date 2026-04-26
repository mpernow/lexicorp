#pragma once

#include <Wt/WApplication.h>
#include <Wt/WComboBox.h>
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
  std::shared_ptr<AppContext> mAppContext;
  Wt::WComboBox *mLanguageCombo;
  static constexpr std::array<utils::Language, 3> languages{
      utils::Language::English, utils::Language::French,
      utils::Language::Kurdish};
};

#pragma once

#include "utils/Language.h"
#include <Wt/WApplication.h>
#include <Wt/WText.h>
#include <Wt/WTextArea.h>

class LanguageSelectionPage : public Wt::WApplication {
public:
  LanguageSelectionPage(const Wt::WEnvironment &env);

private:
  static constexpr std::array<utils::Language, 3> languages{
      utils::Language::English, utils::Language::French,
      utils::Language::Kurdish};
};

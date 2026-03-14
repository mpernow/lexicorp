#pragma once

#include "utils/Language.h"
#include <Wt/WContainerWidget.h>
#include <Wt/WText.h>
#include <Wt/WTextArea.h>

class LanguageSelectionPage : public Wt::WContainerWidget {
public:
  LanguageSelectionPage();

private:
  static constexpr std::array<utils::Language, 3> languages{
      utils::Language::English, utils::Language::French,
      utils::Language::Kurdish};
};

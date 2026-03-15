#pragma once

#include "pages/AppContext.h"
#include "utils/Language.h"
#include <Wt/WContainerWidget.h>
#include <Wt/WText.h>
#include <Wt/WTextArea.h>

class LanguageSelectionPage : public Wt::WContainerWidget {
public:
  LanguageSelectionPage(std::shared_ptr<AppContext> appContext);

private:
  static constexpr std::array<utils::Language, 3> languages{
      utils::Language::English, utils::Language::French,
      utils::Language::Kurdish};

  std::shared_ptr<AppContext> mAppContext;
};

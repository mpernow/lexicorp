#pragma once

#include "pages/AppContext.h"
#include <Wt/WContainerWidget.h>
#include <Wt/WText.h>

// Landing page: a short overview of the corpus for the selected language.
class HomePage : public Wt::WContainerWidget {
public:
  HomePage(std::shared_ptr<AppContext> appContext);

private:
  void refresh();

  std::shared_ptr<AppContext> mAppContext;
  Wt::WText *mSubtitle;
  Wt::WText *mWordCount;
  Wt::WText *mKnownCount;
  Wt::WText *mTextCount;
  // This month's Claude requests and their cost; hidden without any.
  Wt::WText *mClaudeUsage;
};

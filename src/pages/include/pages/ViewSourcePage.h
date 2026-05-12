#pragma once

#include "pages/AppContext.h"
#include <Wt/WContainerWidget.h>
#include <memory.h>

class ViewSourcePage : public Wt::WContainerWidget {
public:
  ViewSourcePage(std::shared_ptr<AppContext> appContext);

private:
  void WordSelected();

  std::shared_ptr<AppContext> mAppContext;
  std::wstring mSelectedWord;
  Wt::WText *mText;
};

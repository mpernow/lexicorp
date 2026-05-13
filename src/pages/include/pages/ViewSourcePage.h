#pragma once

#include "db/Text.h"
#include "pages/AppContext.h"
#include <Wt/WContainerWidget.h>
#include <memory.h>
#include <vector>

class ViewSourcePage : public Wt::WContainerWidget {
public:
  ViewSourcePage(std::shared_ptr<AppContext> appContext);

private:
  void WordSelected();

  std::shared_ptr<AppContext> mAppContext;
  Wt::WContainerWidget *mTextsContainer;
  std::wstring mSelectedWord;
  std::vector<db::models::Text> mTextsVec;
};

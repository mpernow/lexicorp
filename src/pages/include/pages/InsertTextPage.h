#pragma once

#include "db/WordRepository.h"
#include "pages/AppContext.h"
#include "processor/TextProcessor.h"
#include <Wt/WContainerWidget.h>
#include <Wt/WText.h>
#include <Wt/WTextArea.h>

class InsertTextPage : public Wt::WContainerWidget {
public:
  InsertTextPage(std::shared_ptr<AppContext> appContext);

private:
  Wt::WTextArea *textEdit_;
  Wt::WText *greeting_;
  std::unique_ptr<processor::TextProcessor> mTextProcessor;
  std::shared_ptr<AppContext> mAppContext;
};

#pragma once

#include "pages/AppContext.h"
#include "processor/TextProcessor.h"
#include <Wt/WContainerWidget.h>
#include <Wt/WPushButton.h>
#include <Wt/WTable.h>
#include <Wt/WText.h>
#include <Wt/WTextArea.h>
#include <unordered_map>

class InsertTextPage : public Wt::WContainerWidget {
public:
  InsertTextPage(std::shared_ptr<AppContext> appContext);

private:
  void greet();
  Wt::WTextArea *textEdit_;
  Wt::WText *mResultsText;
  Wt::WTable *mWordTable;
  Wt::WPushButton *mSubmitButton;
  std::unordered_map<std::wstring, int> mWordCounts;
  std::unique_ptr<processor::TextProcessor> mTextProcessor;
  std::shared_ptr<AppContext> mAppContext;
};

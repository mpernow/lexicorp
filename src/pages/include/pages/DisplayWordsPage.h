#pragma once

#include "pages/AppContext.h"
#include <Wt/WContainerWidget.h>
#include <Wt/WPushButton.h>
#include <Wt/WTable.h>
#include <Wt/WText.h>
#include <Wt/WTextArea.h>
#include <unordered_map>

class DisplayWordsPage : public Wt::WContainerWidget {
public:
  DisplayWordsPage(std::shared_ptr<AppContext> appContext);

private:
  struct RowData {
    std::wstring word;
    int count;
    bool known;
  };

  void buildTable();
  void sortRows();
  void rebuildRows();

  Wt::WTextArea *textEdit_;
  Wt::WText *mResultsText;
  Wt::WTable *mWordTable;
  std::shared_ptr<AppContext> mAppContext;
  std::vector<RowData> mRows;
  std::vector<RowData> mFilteredRows;
  bool mAscending = false;
  bool mShowUnknownOnly = false;
};

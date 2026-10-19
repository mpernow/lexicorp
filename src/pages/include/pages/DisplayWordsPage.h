#pragma once

#include "pages/AppContext.h"
#include <Wt/WContainerWidget.h>
#include <Wt/WPushButton.h>
#include <Wt/WTable.h>
#include <Wt/WText.h>

class DisplayWordsPage : public Wt::WContainerWidget {
public:
  DisplayWordsPage(std::shared_ptr<AppContext> appContext);

  void loadData();

private:
  struct RowData {
    std::wstring word;
    int count;
    bool known;
    std::string gloss; // UTF-8, empty without one
  };

  void buildTable();
  void sortRows();
  void rebuildRows();
  void confirmDelete(const std::wstring &word);
  void deleteWord(const std::wstring &word);

  Wt::WPushButton *mToggleButton;
  Wt::WTable *mWordTable;
  Wt::WContainerWidget *mEmptyState;
  std::shared_ptr<AppContext> mAppContext;
  std::vector<RowData> mRows;
  std::vector<RowData> mFilteredRows;
  bool mAscending = false;
  bool mShowUnknownOnly = false;
};

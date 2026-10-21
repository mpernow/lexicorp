#pragma once

#include "pages/AppContext.h"
#include "processor/TextProcessor.h"
#include <Wt/WContainerWidget.h>
#include <Wt/WLineEdit.h>
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
    bool aiGloss = false;
    std::string foldedGloss; // case-folded, for search
  };

  void buildTable();
  void sortRows();
  void rebuildRows();
  void updateSearch();
  bool matchesSearch(const RowData &row) const;
  void confirmDelete(const std::wstring &word);
  void deleteWord(const std::wstring &word);

  Wt::WContainerWidget *mToolbar;
  Wt::WLineEdit *mSearchEdit;
  Wt::WPushButton *mToggleButton;
  Wt::WTable *mWordTable;
  Wt::WContainerWidget *mEmptyState;
  std::shared_ptr<AppContext> mAppContext;
  std::vector<RowData> mRows;
  std::vector<RowData> mFilteredRows;
  bool mAscending = false;
  bool mShowUnknownOnly = false;
  processor::TextProcessor mTextProcessor;
  // The search box's text: normalised like stored words, and case-folded
  // for glosses. Both empty when there is no search.
  std::wstring mWordQuery;
  std::string mGlossQuery;
};

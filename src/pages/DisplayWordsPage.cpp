#include <Wt/WBreak.h>
#include <Wt/WCheckBox.h>
#include <Wt/WContainerWidget.h>
#include <algorithm>

#include "pages/DisplayWordsPage.h"

DisplayWordsPage::DisplayWordsPage(std::shared_ptr<AppContext> appContext)
    : mAppContext(appContext) {

  auto container = addWidget(std::make_unique<Wt::WContainerWidget>());
  container->setStyleClass("main-container");

  std::vector<db::models::Word> words =
      mAppContext->wordRepository->getAll(mAppContext->selectedLanguage);

  container->addNew<Wt::WBreak>();

  mWordTable = container->addNew<Wt::WTable>();

  mRows.clear();
  for (const db::models::Word word : words) {
    mRows.emplace_back(word.word, word.numOccurrences, word.known);
  }
  std::sort(mRows.begin(), mRows.end(), [](const RowData &a, const RowData &b) {
    return a.count > b.count;
  });

  buildTable();
}

void DisplayWordsPage::buildTable() {
  mWordTable->clear();

  mWordTable->elementAt(0, 0)->addWidget(std::make_unique<Wt::WText>("Word"));

  // Make sortable column
  std::string countLabel = "Count";
  countLabel += mAscending ? " ▲" : " ▼";
  auto countText = std::make_unique<Wt::WText>(countLabel);
  countText->setStyleClass("clickable-header");
  countText->clicked().connect([=] {
    mAscending = !mAscending;
    sortRows();
    buildTable();
  });
  mWordTable->elementAt(0, 1)->addWidget(std::move(countText));
  mWordTable->elementAt(0, 2)->addWidget(std::make_unique<Wt::WText>("Known?"));

  int row = 1;
  for (auto &r : mRows) {

    mWordTable->elementAt(row, 0)->addWidget(
        std::make_unique<Wt::WText>(r.word));

    mWordTable->elementAt(row, 1)->addWidget(
        std::make_unique<Wt::WText>(std::to_string(r.count)));

    mWordTable->elementAt(row, 2)->addWidget(
        std::make_unique<Wt::WText>(r.known ? "Yes" : "No"));

    row++;
  }
}

void DisplayWordsPage::sortRows() {
  std::sort(mRows.begin(), mRows.end(),
            [this](const RowData &a, const RowData &b) {
              if (mAscending) {
                return a.count < b.count;
              } else {
                return a.count > b.count;
              }
            });
}

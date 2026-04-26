#include <Wt/WBreak.h>
#include <Wt/WCheckBox.h>
#include <Wt/WContainerWidget.h>
#include <algorithm>

#include "pages/DisplayWordsPage.h"

DisplayWordsPage::DisplayWordsPage(std::shared_ptr<AppContext> appContext)
    : mAppContext(appContext) {

  mAppContext->languageChanged.connect(this, &DisplayWordsPage::loadData);

  auto container = addWidget(std::make_unique<Wt::WContainerWidget>());
  container->setStyleClass("main-container");

  container->addNew<Wt::WBreak>();

  auto toggleBtn = container->addNew<Wt::WPushButton>("Show Unknown Only");

  toggleBtn->clicked().connect([=] {
    mShowUnknownOnly = !mShowUnknownOnly;

    toggleBtn->setText(mShowUnknownOnly ? "Show All" : "Show Unknown Only");

    rebuildRows();
  });

  container->addNew<Wt::WBreak>();

  mWordTable = container->addNew<Wt::WTable>();

  loadData();
}

void DisplayWordsPage::loadData() {
  std::vector<db::models::Word> words =
      mAppContext->wordRepository->getAll(mAppContext->selectedLanguage);

  mRows.clear();
  for (const db::models::Word word : words) {
    mRows.emplace_back(word.word, word.numOccurrences, word.known);
  }
  std::sort(mRows.begin(), mRows.end(), [](const RowData &a, const RowData &b) {
    return a.count > b.count;
  });

  rebuildRows();
}

void DisplayWordsPage::rebuildRows() {
  mFilteredRows.clear();

  for (const auto &r : mRows) {
    if (!mShowUnknownOnly || !r.known) {
      mFilteredRows.push_back(r);
    }
  }

  sortRows();
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
  for (auto &r : mFilteredRows) {

    mWordTable->elementAt(row, 0)->addWidget(
        std::make_unique<Wt::WText>(r.word));

    mWordTable->elementAt(row, 1)->addWidget(
        std::make_unique<Wt::WText>(std::to_string(r.count)));

    mWordTable->elementAt(row, 2)->addWidget(
        std::make_unique<Wt::WText>(r.known ? "✔" : "✘"));

    if (!r.known) {
      auto makeKnownButton =
          std::make_unique<Wt::WPushButton>("I know this one!");

      makeKnownButton->clicked().connect([=] {
        for (RowData &row : mRows) {
          if (row.word == r.word) {
            row.known = true;
          }
        }
        rebuildRows();
        mAppContext->wordRepository->update(db::models::Word{
            r.word, r.count, true, mAppContext->selectedLanguage});
      });

      mWordTable->elementAt(row, 3)->addWidget(std::move(makeKnownButton));
    }

    row++;
  }
}

void DisplayWordsPage::sortRows() {
  std::sort(mFilteredRows.begin(), mFilteredRows.end(),
            [this](const RowData &a, const RowData &b) {
              if (mAscending) {
                return a.count < b.count;
              } else {
                return a.count > b.count;
              }
            });
}

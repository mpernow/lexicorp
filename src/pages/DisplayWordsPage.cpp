#include <Wt/WApplication.h>
#include <Wt/WBreak.h>
#include <Wt/WCheckBox.h>
#include <Wt/WContainerWidget.h>
#include <algorithm>

#include "db/Savepoint.h"
#include "db/Utils.h"
#include "pages/ConfirmDialog.h"
#include "pages/DisplayWordsPage.h"
#include "pages/EmptyState.h"

DisplayWordsPage::DisplayWordsPage(std::shared_ptr<AppContext> appContext)
    : mAppContext(appContext) {

  mAppContext->languageChanged.connect(this, &DisplayWordsPage::loadData);
  Wt::WApplication::instance()->internalPathChanged().connect(
      [this](const std::string &path) {
        if (path == "/list")
          loadData();
      });

  auto container = addWidget(std::make_unique<Wt::WContainerWidget>());
  container->setStyleClass("main-container");

  mToggleButton = container->addNew<Wt::WPushButton>("Show Unknown Only");
  mToggleButton->setStyleClass("btn-outline-secondary align-self-start mb-3");

  mToggleButton->clicked().connect([this] {
    mShowUnknownOnly = !mShowUnknownOnly;

    mToggleButton->setText(mShowUnknownOnly ? "Show All"
                                            : "Show Unknown Only");

    rebuildRows();
  });

  mWordTable = container->addNew<Wt::WTable>();
  mWordTable->setStyleClass("table table-hover align-middle word-table");
  mWordTable->setHeaderCount(1);

  mEmptyState = container->addNew<Wt::WContainerWidget>();

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
  mEmptyState->clear();

  mToggleButton->setHidden(mRows.empty());
  mWordTable->setHidden(mFilteredRows.empty());
  if (mRows.empty()) {
    mEmptyState->addWidget(makeEmptyState(
        "No " + utils::to_string(mAppContext->selectedLanguage) +
            " words yet. Insert a text to start building your word list.",
        true));
    return;
  }
  if (mFilteredRows.empty()) {
    mEmptyState->addWidget(
        makeEmptyState("No unknown words left — you know them all!", false));
    return;
  }

  mWordTable->elementAt(0, 0)->addWidget(std::make_unique<Wt::WText>("Word"));

  // Make sortable column
  std::string countLabel = "Count";
  countLabel += mAscending ? " ▲" : " ▼";
  auto countText = std::make_unique<Wt::WText>(countLabel);
  countText->setStyleClass("clickable-header");
  countText->clicked().connect([this] {
    mAscending = !mAscending;
    sortRows();
    buildTable();
  });
  mWordTable->elementAt(0, 1)->addWidget(std::move(countText));
  mWordTable->elementAt(0, 2)->addWidget(std::make_unique<Wt::WText>("Known"));

  int rowCount = 1;
  for (auto &r : mFilteredRows) {

    // Tapping the word opens the texts it appears in.
    auto wordButton = std::make_unique<Wt::WPushButton>(Wt::WString(r.word));
    wordButton->setStyleClass("btn-link word-link");
    wordButton->setToolTip("View sources");
    wordButton->clicked().connect([this, word = r.word] {
      mAppContext->selectedWord = word;
      mAppContext->wordSelected.emit();
      Wt::WApplication::instance()->setInternalPath("/source/word", true);
    });
    mWordTable->elementAt(rowCount, 0)->addWidget(std::move(wordButton));
    mWordTable->elementAt(rowCount, 0)->setAttributeValue("dir", "auto");

    mWordTable->elementAt(rowCount, 1)
        ->addWidget(std::make_unique<Wt::WText>(std::to_string(r.count)));

    // Bootstrap draws a switch for a checkbox inside .form-check.form-switch.
    auto switchWrapper = std::make_unique<Wt::WContainerWidget>();
    switchWrapper->setStyleClass("form-check form-switch known-switch");
    auto knownSwitch = std::make_unique<Wt::WCheckBox>();
    knownSwitch->setChecked(r.known);
    knownSwitch->setToolTip("Known");
    Wt::WCheckBox *knownSwitchPtr = knownSwitch.get();
    knownSwitch->changed().connect([this, r, knownSwitchPtr] {
      bool known = knownSwitchPtr->isChecked();
      for (RowData &row : mRows) {
        if (row.word == r.word) {
          row.known = known;
        }
      }
      mAppContext->wordRepository->update(db::models::Word{
          r.word, r.count, known, mAppContext->selectedLanguage});
      rebuildRows();
    });
    switchWrapper->addWidget(std::move(knownSwitch));
    mWordTable->elementAt(rowCount, 2)->addWidget(std::move(switchWrapper));

    // The cross is drawn by CSS (.delete-button), not a ✕ character, which
    // some devices' fonts lack.
    auto deleteButton = std::make_unique<Wt::WPushButton>();
    deleteButton->setStyleClass("btn-sm btn-outline-danger delete-button");
    deleteButton->setToolTip("Delete word");
    deleteButton->setAttributeValue("aria-label", "Delete word");
    deleteButton->clicked().connect([this, word = r.word] {
      confirmDelete(word);
    });
    mWordTable->elementAt(rowCount, 3)->addWidget(std::move(deleteButton));

    rowCount++;
  }
}

void DisplayWordsPage::confirmDelete(const std::wstring &word) {
  std::string utf8Word = db::Utils::toUtf8(word);
  confirmDestructive(
      this, "Delete word",
      "Delete \"" + utf8Word +
          "\" from the word list? The texts it appears in are kept.",
      "Delete", [this, word] { deleteWord(word); });
}

void DisplayWordsPage::deleteWord(const std::wstring &word) {
  db::Savepoint tx{mAppContext->dbHandler->getConnection(), "delete_word"};
  mAppContext->wordRepository->remove(word, mAppContext->selectedLanguage);
  mAppContext->wordTextRepository->removeByWord(word,
                                                mAppContext->selectedLanguage);
  tx.commit();

  std::erase_if(mRows, [&word](const RowData &r) { return r.word == word; });
  rebuildRows();
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

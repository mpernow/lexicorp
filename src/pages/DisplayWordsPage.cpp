#include <Wt/WApplication.h>
#include <Wt/WBreak.h>
#include <Wt/WCheckBox.h>
#include <Wt/WContainerWidget.h>
#include <algorithm>
#include <unicode/unistr.h>

#include "db/Savepoint.h"
#include "db/Utils.h"
#include "pages/ConfirmDialog.h"
#include "pages/DisplayWordsPage.h"
#include "pages/EmptyState.h"
#include "pages/Bidi.h"
#include "pages/Glosses.h"

namespace {
std::string foldCase(const std::string &utf8) {
  std::string folded;
  icu::UnicodeString::fromUTF8(utf8).foldCase().toUTF8String(folded);
  return folded;
}
} // namespace

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

  mToolbar = container->addNew<Wt::WContainerWidget>();
  mToolbar->setStyleClass("word-list-toolbar d-flex flex-wrap gap-2 mb-3");

  mSearchEdit = mToolbar->addNew<Wt::WLineEdit>();
  mSearchEdit->setStyleClass("word-search");
  mSearchEdit->setPlaceholderText("Search words or glosses");
  mSearchEdit->setAttributeValue("type", "search");
  mSearchEdit->setAttributeValue("dir", "auto");
  mSearchEdit->setAttributeValue("aria-label", "Search words");
  mSearchEdit->textInput().connect(this, &DisplayWordsPage::updateSearch);

  mToggleButton = mToolbar->addNew<Wt::WPushButton>("Show Unknown Only");
  mToggleButton->setStyleClass("btn-outline-secondary");

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

  auto glosses = glossesOfStoredWords(*mAppContext);

  mRows.clear();
  for (const db::models::Word word : words) {
    auto gloss = glosses.find(word.word);
    if (gloss == glosses.end()) {
      mRows.emplace_back(word.word, word.numOccurrences, word.known, "");
    } else {
      std::string summary = glossSummary(gloss->second);
      mRows.emplace_back(word.word, word.numOccurrences, word.known, summary,
                         gloss->second.entries.front().aiGenerated,
                         foldCase(summary));
    }
  }
  std::sort(mRows.begin(), mRows.end(), [](const RowData &a, const RowData &b) {
    return a.count > b.count;
  });

  updateSearch(); // the query's normalisation depends on the language
}

void DisplayWordsPage::rebuildRows() {
  mFilteredRows.clear();

  for (const auto &r : mRows) {
    if ((!mShowUnknownOnly || !r.known) && matchesSearch(r)) {
      mFilteredRows.push_back(r);
    }
  }

  sortRows();
  buildTable();
}

void DisplayWordsPage::updateSearch() {
  std::wstring query = mSearchEdit->text().value();
  // Normalise like stored words, so case, punctuation and Kurdish letter
  // variants don't matter.
  mWordQuery.clear();
  for (const auto &word :
       mTextProcessor.words(query, mAppContext->selectedLanguage)) {
    if (!mWordQuery.empty())
      mWordQuery += L' ';
    mWordQuery += word;
  }
  mGlossQuery = foldCase(mSearchEdit->text().toUTF8());
  if (mGlossQuery.find_first_not_of(" \t") == std::string::npos)
    mGlossQuery.clear();
  rebuildRows();
}

bool DisplayWordsPage::matchesSearch(const RowData &row) const {
  if (mWordQuery.empty() && mGlossQuery.empty())
    return true;
  return (!mWordQuery.empty() &&
          row.word.find(mWordQuery) != std::wstring::npos) ||
         (!mGlossQuery.empty() &&
          row.foldedGloss.find(mGlossQuery) != std::string::npos);
}

void DisplayWordsPage::buildTable() {
  mWordTable->clear();
  mEmptyState->clear();

  mToolbar->setHidden(mRows.empty());
  mWordTable->setHidden(mFilteredRows.empty());
  if (mRows.empty()) {
    mEmptyState->addWidget(makeEmptyState(
        "No " + utils::to_string(mAppContext->selectedLanguage) +
            " words yet. Insert a text to start building your word list.",
        true));
    return;
  }
  if (mFilteredRows.empty()) {
    if (!mWordQuery.empty() || !mGlossQuery.empty())
      mEmptyState->addWidget(makeEmptyState(
          "No " + std::string(mShowUnknownOnly ? "unknown " : "") +
              "words match \"" + isolate(mSearchEdit->text().toUTF8()) +
              "\".",
          false));
    else
      mEmptyState->addWidget(makeEmptyState(
          "No unknown words left — you know them all!", false));
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
    if (!r.gloss.empty()) {
      auto gloss = mWordTable->elementAt(rowCount, 0)->addNew<Wt::WText>(
          Wt::WString::fromUTF8(r.gloss), Wt::TextFormat::Plain);
      gloss->setInline(false);
      gloss->setStyleClass("word-gloss small text-body-secondary");
      if (r.aiGloss) {
        gloss->addStyleClass("fst-italic");
        gloss->setToolTip("AI-generated");
      }
    }
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
      "Delete \"" + isolate(utf8Word) +
          "\" from the word list? The texts it appears in are kept.",
      "Delete", [this, word] { deleteWord(word); });
}

void DisplayWordsPage::deleteWord(const std::wstring &word) {
  db::Savepoint tx{mAppContext->dbHandler->getConnection(), "delete_word"};
  mAppContext->wordRepository->remove(word, mAppContext->selectedLanguage);
  mAppContext->wordTextRepository->removeByWord(word,
                                                mAppContext->selectedLanguage);
  mAppContext->wordSentenceRepository->removeByWord(
      word, mAppContext->selectedLanguage);
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

#include <Wt/WApplication.h>
#include <Wt/WBreak.h>
#include <Wt/WCheckBox.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WLabel.h>

#include "db/Savepoint.h"
#include "db/WordText.h"
#include "pages/InsertTextPage.h"

namespace {
// Grows the text area with its content (never below its `rows` height),
// up to the CSS max-height of .auto-grow.
void autoGrow(Wt::WTextArea *textArea) {
  textArea->addStyleClass("auto-grow");
  textArea->doJavaScript(
      "(function(el) {"
      "  function fit() {"
      "    el.style.height = 'auto';"
      "    el.style.height ="
      "        (el.scrollHeight + el.offsetHeight - el.clientHeight) + 'px';"
      "  }"
      "  el.addEventListener('input', fit);"
      "  fit();"
      "})(" +
      textArea->jsRef() + ");");
}

// Returns an auto-grow text area to its `rows` height after its text was
// cleared from the server (which doesn't fire the `input` event). Measuring
// with fit() wouldn't work here: the page is usually hidden at that point.
void resetHeight(Wt::WTextArea *textArea) {
  textArea->doJavaScript(textArea->jsRef() + ".style.height = '';");
}
} // namespace

InsertTextPage::InsertTextPage(std::shared_ptr<AppContext> appContext)
    : mAppContext(appContext) {
  mTextProcessor = std::make_unique<processor::TextProcessor>();

  auto container = addWidget(std::make_unique<Wt::WContainerWidget>());
  container->setStyleClass("main-container");

  auto textLabel = container->addNew<Wt::WLabel>("Text");
  textLabel->setStyleClass("form-label fw-semibold");
  textEdit_ = container->addNew<Wt::WTextArea>();
  textEdit_->setStyleClass("form-control mb-3");
  textEdit_->setRows(10);
  textEdit_->setPlaceholderText("Paste the text you want to learn from");
  textEdit_->setAttributeValue("dir", "auto");
  autoGrow(textEdit_);
  textLabel->setBuddy(textEdit_);

  auto sourceLabel = container->addNew<Wt::WLabel>("Source");
  sourceLabel->setStyleClass("form-label fw-semibold");
  mSourceTextArea = container->addNew<Wt::WTextArea>();
  mSourceTextArea->setStyleClass("form-control mb-3");
  mSourceTextArea->setRows(1);
  mSourceTextArea->setPlaceholderText("Where the text came from, e.g. a URL");
  mSourceTextArea->setAttributeValue("dir", "auto");
  autoGrow(mSourceTextArea);
  sourceLabel->setBuddy(mSourceTextArea);

  mProcessButton = container->addNew<Wt::WPushButton>("Process");
  mProcessButton->setStyleClass(
      "btn-primary align-self-start mb-4 busy-button");
  // disable() runs in the browser as soon as the button is clicked (Wt
  // treats it as a stateless slot), which blocks double clicks and shows the
  // .busy-button spinner until the handler re-enables the button.
  mProcessButton->clicked().connect(mProcessButton, &Wt::WWidget::disable);
  mProcessButton->clicked().connect(this, &InsertTextPage::process);

  auto tableContainer = container->addNew<Wt::WContainerWidget>();
  tableContainer->setStyleClass("table-container");
  mWordTable = tableContainer->addNew<Wt::WTable>();
  mWordTable->setStyleClass(
      "table table-hover table-sm align-middle word-select-table");
  mWordTable->setHeaderCount(1);
  mWordTable->hide(); // until a text is processed

  mSubmitButton = container->addNew<Wt::WPushButton>("Submit");
  // Sticks to the bottom of the screen while scrolling a long word table.
  mSubmitButton->setStyleClass(
      "btn-success align-self-start mt-3 sticky-submit busy-button");
  mSubmitButton->clicked().connect(mSubmitButton, &Wt::WWidget::disable);
  mSubmitButton->clicked().connect(this, &InsertTextPage::submit);
  mSubmitButton->hide();

  mResultsText = container->addNew<Wt::WText>();
  mResultsText->setInline(false);
}

void InsertTextPage::process() {
  mProcessButton->enable();
  mText = textEdit_->text().value();
  mWordCounts = mTextProcessor->computeFrequencies(mText);

  mSource = mSourceTextArea->text().value();

  mWordTable->clear();
  mWordTable->elementAt(0, 0)->addWidget(std::make_unique<Wt::WText>("Word"));
  mWordTable->elementAt(0, 1)->addWidget(std::make_unique<Wt::WText>("Count"));
  mWordTable->elementAt(0, 2)->addWidget(
      std::make_unique<Wt::WText>("Include?"));
  mWordTable->elementAt(0, 3)->addWidget(std::make_unique<Wt::WText>("Known?"));

  mRows.clear();

  for (const auto &[word, count] : mWordCounts) {
    mRows.emplace_back(word, count, true, false);
  }
  std::sort(mRows.begin(), mRows.end(), [](const RowData &a, const RowData &b) {
    return a.count > b.count;
  });

  int row = 1;
  for (const RowData rowData : mRows) {

    mWordTable->elementAt(row, 0)->addWidget(
        std::make_unique<Wt::WText>(rowData.word));
    mWordTable->elementAt(row, 0)->setAttributeValue("dir", "auto");

    mWordTable->elementAt(row, 1)->addWidget(
        std::make_unique<Wt::WText>(std::to_string(rowData.count)));

    auto includeCheckbox = mWordTable->elementAt(row, 2)->addWidget(
        std::make_unique<Wt::WCheckBox>());
    includeCheckbox->setChecked(mRows.at(row - 1).include);
    includeCheckbox->changed().connect([this, row, includeCheckbox] {
      mRows.at(row - 1).include = includeCheckbox->isChecked();
    });

    auto knownCheckbox = mWordTable->elementAt(row, 3)->addWidget(
        std::make_unique<Wt::WCheckBox>());
    knownCheckbox->setChecked(mRows.at(row - 1).known);
    knownCheckbox->changed().connect([this, row, knownCheckbox] {
      mRows.at(row - 1).known = knownCheckbox->isChecked();
    });
    row++;
  }

  mHash = mTextProcessor->hashWstring(mText);

  bool noWords = mRows.empty();
  mWordTable->setHidden(noWords);
  mSubmitButton->setHidden(noWords);
  mResultsText->setText(noWords ? "No words found in this text." : "");
  mResultsText->setStyleClass(noWords ? "alert alert-info mt-3" : "");
}

void InsertTextPage::submit() {
  mSubmitButton->enable();
  if (mRows.empty())
    return; // nothing processed (e.g. the page was just cleared)
  if (mAppContext->textRepository->getByHash(static_cast<int>(mHash))) {
    mResultsText->setText("This text has already been added.");
    mResultsText->setStyleClass("alert alert-warning mt-3");
    return;
  }
  mResultsText->setText("");
  mResultsText->setStyleClass("");

  // One transaction for all three tables: a single commit is much faster on
  // the Pi's SD card than one per row, and a failure part-way leaves nothing
  // half-saved.
  db::Savepoint tx{mAppContext->dbHandler->getConnection(), "submit_text"};

  std::vector<db::models::Word> wordsToSave;
  for (const RowData row : mRows) {
    if (row.include) {
      wordsToSave.emplace_back(row.word, row.count, row.known,
                               mAppContext->selectedLanguage);
      mAppContext->wordTextRepository->add(
          db::models::WordText{row.word, static_cast<int>(mHash), row.count,
                               mAppContext->selectedLanguage});
    }
  }
  mAppContext->wordRepository->updateFrequencies(wordsToSave);
  mAppContext->textRepository->add(db::models::Text{
      mText, static_cast<int>(mHash), mAppContext->selectedLanguage, mSource});
  tx.commit();

  clear();
  Wt::WApplication::instance()->setInternalPath("/list", true);
}

void InsertTextPage::clear() {
  textEdit_->setText("");
  mSourceTextArea->setText("");
  resetHeight(textEdit_);
  resetHeight(mSourceTextArea);

  mWordTable->clear();
  mWordTable->hide();
  mSubmitButton->hide();
  mResultsText->setText("");
  mResultsText->setStyleClass("");

  mRows.clear();
  mWordCounts.clear();
  mText.clear();
  mSource.clear();
  mHash = 0;
}

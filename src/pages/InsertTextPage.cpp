#include <Wt/WApplication.h>
#include <Wt/WBreak.h>
#include <Wt/WCheckBox.h>
#include <Wt/WContainerWidget.h>

#include "db/WordText.h"
#include "pages/InsertTextPage.h"

InsertTextPage::InsertTextPage(std::shared_ptr<AppContext> appContext)
    : mAppContext(appContext) {
  mTextProcessor = std::make_unique<processor::TextProcessor>();

  auto container = addWidget(std::make_unique<Wt::WContainerWidget>());
  container->setStyleClass("main-container");
  container->resize(Wt::WLength("100%"), Wt::WLength("100%"));
  container->setOverflow(Wt::Overflow::Auto);

  container->addNew<Wt::WText>("Enter your text here");
  textEdit_ = container->addNew<Wt::WTextArea>();
  textEdit_->setStyleClass("textarea");

  container->addNew<Wt::WText>("Source: ");
  mSourceTextArea = container->addNew<Wt::WTextArea>();

  Wt::WPushButton *button = container->addNew<Wt::WPushButton>("Process");
  button->setStyleClass("button");

  container->addNew<Wt::WBreak>();

  button->clicked().connect(this, &InsertTextPage::process);

  auto tableContainer = container->addNew<Wt::WContainerWidget>();
  tableContainer->setStyleClass("table-container");
  mWordTable = tableContainer->addNew<Wt::WTable>();

  mSubmitButton = container->addNew<Wt::WPushButton>("Submit");
  mSubmitButton->clicked().connect(this, &InsertTextPage::submit);
  mSubmitButton->hide();

  mResultsText = container->addNew<Wt::WText>();
  mResultsText->setStyleClass("default-text");
}

void InsertTextPage::process() {
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
  mSubmitButton->show();
}

void InsertTextPage::submit() {
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

  Wt::WApplication::instance()->setInternalPath("/list", true);
}

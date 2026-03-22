#include <Wt/WBreak.h>
#include <Wt/WCheckBox.h>
#include <Wt/WContainerWidget.h>

#include "pages/InsertTextPage.h"
#include "utils/Language.h"

InsertTextPage::InsertTextPage(std::shared_ptr<AppContext> appContext)
    : mAppContext(appContext) {
  mTextProcessor = std::make_unique<processor::TextProcessor>();

  auto container = addWidget(std::make_unique<Wt::WContainerWidget>());
  container->setStyleClass("main-container");

  container->addNew<Wt::WText>("Enter your text here");
  textEdit_ = container->addNew<Wt::WTextArea>();
  textEdit_->setStyleClass("textarea");

  Wt::WPushButton *button = container->addNew<Wt::WPushButton>("Process");
  button->setStyleClass("button");

  container->addNew<Wt::WBreak>();

  button->clicked().connect(this, &InsertTextPage::process);

  mWordTable = container->addNew<Wt::WTable>();

  mSubmitButton = container->addNew<Wt::WPushButton>("Submit");
  mSubmitButton->clicked().connect(this, &InsertTextPage::submit);
  mSubmitButton->hide();

  mResultsText = container->addNew<Wt::WText>();
  mResultsText->setStyleClass("default-text");
}

void InsertTextPage::process() {
  mWordCounts = mTextProcessor->computeFrequencies(textEdit_->text().value());

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

  mSubmitButton->show();
}

void InsertTextPage::submit() {
  // mAppContext->wordRepository->updateFrequencies(wordCounts,
  //                                                utils::Language::Unknown);
  //
  ////
  //
  std::wstring outputText = L"";
  for (const RowData row : mRows) {
    if (row.include) {

      outputText += row.word + L": " + std::to_wstring(row.count) + L"\n";
    }
  }
  mResultsText->setText(outputText);
  mResultsText->setTextFormat(Wt::TextFormat::Plain);
}

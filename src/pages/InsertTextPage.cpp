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

  mResultsText = container->addNew<Wt::WText>();
  mResultsText->setStyleClass("default-text");

  button->clicked().connect(this, &InsertTextPage::greet);

  mWordTable = container->addNew<Wt::WTable>();

  mSubmitButton = container->addNew<Wt::WPushButton>("Submit");
  mSubmitButton->hide();
}

void InsertTextPage::greet() {
  mWordCounts = mTextProcessor->computeFrequencies(textEdit_->text().value());

  mWordTable->clear();
  mWordTable->elementAt(0, 0)->addWidget(std::make_unique<Wt::WText>("Word"));
  mWordTable->elementAt(0, 1)->addWidget(std::make_unique<Wt::WText>("Count"));
  mWordTable->elementAt(0, 2)->addWidget(std::make_unique<Wt::WText>("Select"));

  int row = 1;

  for (const auto &[word, count] : mWordCounts) {

    mWordTable->elementAt(row, 0)->addWidget(std::make_unique<Wt::WText>(word));

    mWordTable->elementAt(row, 1)->addWidget(
        std::make_unique<Wt::WText>(std::to_string(count)));

    auto checkbox = mWordTable->elementAt(row, 2)->addWidget(
        std::make_unique<Wt::WCheckBox>());

    checkbox->checked().connect(
        [word] { std::wcout << "Selected: " << word << std::endl; });

    row++;
  }

  mSubmitButton->show();

  // mAppContext->wordRepository->updateFrequencies(wordCounts,
  //                                                utils::Language::Unknown);
  std::wstring outputText = L"";
  for (const auto &[word, count] : mWordCounts) {
    outputText += word + L": " + std::to_wstring(count) + L"\n";
  }
  mResultsText->setText(outputText);
  mResultsText->setTextFormat(Wt::TextFormat::Plain);
}

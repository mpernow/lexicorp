#include <Wt/WBreak.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WPushButton.h>
#include <unordered_map>

#include "pages/InsertTextPage.h"
#include "utils/Language.h"

InsertTextPage::InsertTextPage(std::shared_ptr<db::WordRepository> wordRepo)
    : mWordRepository(wordRepo) {
  mTextProcessor = std::make_unique<processor::TextProcessor>();

  auto container = addWidget(std::make_unique<Wt::WContainerWidget>());
  container->setStyleClass("main-container");

  container->addNew<Wt::WText>("Your name, please? ");
  textEdit_ = container->addNew<Wt::WTextArea>();
  textEdit_->setStyleClass("textarea");

  Wt::WPushButton *button = container->addNew<Wt::WPushButton>("Greet me.");
  button->setStyleClass("button");

  container->addNew<Wt::WBreak>();

  greeting_ = container->addNew<Wt::WText>();
  greeting_->setStyleClass("greeting");
  auto greet = [this] {
    std::unordered_map<std::wstring, int> wordCounts =
        mTextProcessor->computeFrequencies(textEdit_->text().value());

    mWordRepository->updateFrequencies(wordCounts, utils::Language::Unknown);
    std::wstring outputText = L"";
    for (const auto &[word, count] : wordCounts) {
      outputText += word + L": " + std::to_wstring(count) + L"\n";
    }
    greeting_->setText(outputText);
  };
  button->clicked().connect(greet);
}

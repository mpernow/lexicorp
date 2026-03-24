#include <Wt/WBreak.h>
#include <Wt/WCheckBox.h>
#include <Wt/WContainerWidget.h>

#include "pages/DisplayWordsPage.h"

DisplayWordsPage::DisplayWordsPage(std::shared_ptr<AppContext> appContext)
    : mAppContext(appContext) {

  auto container = addWidget(std::make_unique<Wt::WContainerWidget>());
  container->setStyleClass("main-container");

  std::vector<db::models::Word> words =
      mAppContext->wordRepository->getAll(mAppContext->selectedLanguage);

  mWordTable = container->addNew<Wt::WTable>();

  container->addNew<Wt::WBreak>();

  mWordTable = container->addNew<Wt::WTable>();
  mWordTable->elementAt(0, 0)->addWidget(std::make_unique<Wt::WText>("Word"));
  mWordTable->elementAt(0, 1)->addWidget(std::make_unique<Wt::WText>("Count"));
  mWordTable->elementAt(0, 2)->addWidget(std::make_unique<Wt::WText>("Known?"));

  int row = 1;
  for (const db::models::Word word : words) {

    mWordTable->elementAt(row, 0)->addWidget(
        std::make_unique<Wt::WText>(word.word));

    mWordTable->elementAt(row, 1)->addWidget(
        std::make_unique<Wt::WText>(std::to_string(word.numOccurrences)));

    mWordTable->elementAt(row, 2)->addWidget(
        std::make_unique<Wt::WText>(std::to_string(word.known)));

    row++;
  }

  mResultsText = container->addNew<Wt::WText>();
  mResultsText->setStyleClass("default-text");
}

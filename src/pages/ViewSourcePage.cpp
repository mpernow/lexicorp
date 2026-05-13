#include "pages/ViewSourcePage.h"
#include "db/Text.h"
#include "db/Utils.h"
#include "db/WordText.h"
#include <Wt/WBreak.h>
#include <Wt/WText.h>
#include <algorithm>

ViewSourcePage::ViewSourcePage(std::shared_ptr<AppContext> appContext)
    : mAppContext(appContext) {

  auto container = addWidget(std::make_unique<Wt::WContainerWidget>());
  container->setStyleClass("main-container");

  mTextsContainer =
      container->addWidget(std::make_unique<Wt::WContainerWidget>());

  WordSelected();

  mAppContext->wordSelected.connect([this] { WordSelected(); });
}

void ViewSourcePage::WordSelected() {
  mSelectedWord = mAppContext->selectedWord;

  mTextsVec.clear();
  mTextsContainer->clear();

  auto wordTextVec = mAppContext->wordTextRepository->getByWord(
      mSelectedWord, mAppContext->selectedLanguage);
  std::sort(wordTextVec.begin(), wordTextVec.end(),
            [](db::models::WordText wt1, db::models::WordText wt2) {
              return wt1.numOccurrences > wt2.numOccurrences;
            });

  for (db::models::WordText &wt : wordTextVec) {
    // TODO: Handle case of std::optional being nullopt - it is an error
    if (auto text = mAppContext->textRepository->getByHash(wt.textHash)) {
      mTextsVec.push_back(*text);
    }
  }

  for (db::models::Text t : mTextsVec) {
    auto textBlock = mTextsContainer->addNew<Wt::WContainerWidget>();
    textBlock->setStyleClass("source-block");

    auto textWidget = textBlock->addNew<Wt::WText>(
        Wt::WString::fromUTF8(db::Utils::toUtf8(t.text)));
    textWidget->setInline(false);
    textWidget->setStyleClass("source-text");
  }
}

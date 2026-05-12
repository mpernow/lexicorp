#include "pages/ViewSourcePage.h"
#include <Wt/WText.h>

ViewSourcePage::ViewSourcePage(std::shared_ptr<AppContext> appContext)
    : mAppContext(appContext) {

  auto container = addWidget(std::make_unique<Wt::WContainerWidget>());
  container->setStyleClass("main-container");

  mText = container->addNew<Wt::WText>("");

  WordSelected();

  mAppContext->wordSelected.connect([this] { WordSelected(); });
}

void ViewSourcePage::WordSelected() {
  mSelectedWord = mAppContext->selectedWord;

  mText->setText(Wt::WString::fromUTF8(
      std::string(mSelectedWord.begin(), mSelectedWord.end())));
}

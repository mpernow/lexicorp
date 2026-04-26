#include "pages/LanguageSelectionPage.h"
#include <Wt/WApplication.h>
#include <Wt/WComboBox.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WPushButton.h>
#include <Wt/WText.h>
#include <memory>

LanguageSelectionPage::LanguageSelectionPage(
    std::shared_ptr<AppContext> appContext)
    : mAppContext(appContext) {

  auto container = addWidget(std::make_unique<Wt::WContainerWidget>());
  container->setStyleClass("center-container");

  auto result = container->addNew<Wt::WText>("Select a language: ");
  result->setStyleClass("default-text");

  auto combo = container->addWidget(std::make_unique<Wt::WComboBox>());
  for (utils::Language language : languages) {
    combo->addItem(utils::to_string(language));
  }

  Wt::WPushButton *button = container->addNew<Wt::WPushButton>("Submit");
  button->setStyleClass("button");

  combo->changed().connect([=] {
    result->setText("You selected: " + combo->currentText().toUTF8());
  });

  button->clicked().connect([this, combo] {
    auto lang = combo->currentText().toUTF8();
    auto newLang = utils::language_from_string(lang);
    mAppContext->selectedLanguage = newLang;
    mAppContext->languageChanged.emit(utils::to_string(newLang));
    Wt::WApplication::instance()->setInternalPath("/insert", true);
  });
}

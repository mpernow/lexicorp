#include "pages/LanguageSelectionPage.h"
#include <Wt/WApplication.h>
#include <Wt/WComboBox.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WText.h>
#include <memory>

LanguageSelectionPage::LanguageSelectionPage() {

  auto container = addWidget(std::make_unique<Wt::WContainerWidget>());
  container->setStyleClass("center-container");

  auto result = container->addNew<Wt::WText>("Select a language: ");
  result->setStyleClass("greeting");

  auto combo = container->addWidget(std::make_unique<Wt::WComboBox>());
  for (utils::Language language : languages) {
    combo->addItem(utils::to_string(language));
  }

  combo->changed().connect([=] {
    result->setText("You selected: " + combo->currentText().toUTF8());
    Wt::WApplication::instance()->setInternalPath("/insert", true);
  });
}
